/* Copyright (C)
* 2015 - John Melton, G0ORX/N6LYT
*
* This program is free software; you can redistribute it and/or
* modify it under the terms of the GNU General Public License
* as published by the Free Software Foundation; either version 2
* of the License, or (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program; if not, write to the Free Software
* Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
*
*/

#include <gtk/gtk.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <net/if_arp.h>
#include <net/if.h>
#include <ifaddrs.h>
#include <semaphore.h>
#include <string.h>
#include <errno.h>
#include <math.h>
#include <wdsp.h>

#include "audio.h"
#include "band.h"
#include "channel.h"
#include "discovered.h"
#include "mode.h"
#include "filter.h"
#include "old_protocol.h"
#include "radio.h"
#include "receiver.h"
#include "transmitter.h"
#include "signal.h"
#include "toolbar.h"
#include "vfo.h"
#include "ext.h"
#include "iambic.h"
#include "error_handler.h"

#define min(x,y) (x<y?x:y)

#define SYNC0 0
#define SYNC1 1
#define SYNC2 2
#define C0 3
#define C1 4
#define C2 5
#define C3 6
#define C4 7

#define DATA_PORT 1024

#define SYNC 0x7F
#define OZY_BUFFER_SIZE 512

// ozy command and control
#define MOX_DISABLED    0x00
#define MOX_ENABLED     0x01

#define MIC_SOURCE_JANUS 0x00
#define MIC_SOURCE_PENELOPE 0x80
#define CONFIG_NONE     0x00
#define CONFIG_PENELOPE 0x20
#define CONFIG_MERCURY  0x40
#define CONFIG_BOTH     0x60
#define PENELOPE_122_88MHZ_SOURCE 0x00
#define MERCURY_122_88MHZ_SOURCE  0x10
#define ATLAS_10MHZ_SOURCE        0x00
#define PENELOPE_10MHZ_SOURCE     0x04
#define MERCURY_10MHZ_SOURCE      0x08
#define SPEED_48K                 0x00
#define SPEED_96K                 0x01
#define SPEED_192K                0x02
#define SPEED_384K                0x03
#define MODE_CLASS_E              0x01
#define MODE_OTHERS               0x00
#define LT2208_GAIN_OFF           0x00
#define LT2208_GAIN_ON            0x04
#define LT2208_DITHER_OFF         0x00
#define LT2208_DITHER_ON          0x08
#define LT2208_RANDOM_OFF         0x00
#define LT2208_RANDOM_ON          0x10

// state machine buffer processing
enum {
  SYNC_0=0,
  SYNC_1,
  SYNC_2,
  CONTROL_0,
  CONTROL_1,
  CONTROL_2,
  CONTROL_3,
  CONTROL_4,
  LEFT_SAMPLE_HI,
  LEFT_SAMPLE_MID,
  LEFT_SAMPLE_LOW,
  RIGHT_SAMPLE_HI,
  RIGHT_SAMPLE_MID,
  RIGHT_SAMPLE_LOW,
  MIC_SAMPLE_HI,
  MIC_SAMPLE_LOW,
  SKIP
};
static int state=SYNC_0;

static int display_width;

static int speed;

static int dsp_rate=48000;
static int output_rate=48000;

static int data_socket=-1;
static int tcp_socket=-1;
static struct sockaddr_in data_addr;

static unsigned char control_in[5]={0x00,0x00,0x00,0x00,0x00};

static double tuning_phase;
static double phase=0.0;

static int running;
static long ep4_sequence;

static uint32_t last_seq_num=-0xffffffff;
static int tx_fifo_flag = 0;

static int current_rx=0;

static int samples=0;
static int mic_samples=0;
static int mic_sample_divisor=1;
static int micsamplecount=0;

static int local_ptt=0;
static int dash=0;
static int dot=0;

static double micinputbuffer[MAX_BUFFER_SIZE*2];

static int left_rx_sample;
static int right_rx_sample;
static int left_tx_sample;
static int right_tx_sample;

static unsigned char output_buffer[OZY_BUFFER_SIZE];
static int output_buffer_index=8;

static int command=1;

static GThread *receive_thread_id;
static gpointer receive_thread(gpointer arg);
static void process_ozy_input_buffer(unsigned const char  *buffer);
static void process_bandscope_buffer(char  *buffer);
void ozy_send_buffer(void);

static unsigned char metis_buffer[1032];
static uint32_t send_sequence=0;
static int metis_offset=8;

static int metis_write(unsigned char ep,unsigned const char* buffer,int length);
static void metis_start_stop(int command);
static void metis_send_buffer(unsigned char* buffer,int length);
static void metis_restart(void);

static void open_tcp_socket(void);
static void open_udp_socket(void);
static int how_many_receivers(void);

#define COMMON_MERCURY_FREQUENCY 0x80
#define PENELOPE_MIC 0x80

#ifdef USBOZY
//
// additional defines if we include USB Ozy support
//
#include "ozyio.h"

static GThread *ozy_EP4_rx_thread_id;
static GThread *ozy_EP6_rx_thread_id;
static gpointer ozy_ep4_rx_thread(gpointer arg);
static gpointer ozy_ep6_rx_thread(gpointer arg);
static void start_usb_receive_threads(void);
static void ozyusb_write(unsigned char* buffer,int length);
#define EP6_IN_ID  0x86                         // end point = 6, direction toward PC
#define EP2_OUT_ID  0x02                        // end point = 2, direction from PC
#define EP6_BUFFER_SIZE 2048
static unsigned char usb_output_buffer[EP6_BUFFER_SIZE];
static unsigned char ep6_inbuffer[EP6_BUFFER_SIZE];
static unsigned char usb_buffer_block = 0;
#define USB_TIMEOUT -7
#endif

static GMutex dump_mutex;

//
// To avoid race conditions, we need a mutex covering the next three functions
// that are called both by the RX and TX thread, and are filling and sending the
// output buffer.
//
static pthread_mutex_t send_audio_mutex   = PTHREAD_MUTEX_INITIALIZER;

//
// This mutex "protects" ozy_send_buffer. This is necessary only for
// TCP and USB-OZY since here the communication is a byte stream.
//
static pthread_mutex_t send_ozy_mutex   = PTHREAD_MUTEX_INITIALIZER;



//
// Ring buffer for outgoing samples.
// Samples going to the radio are produces in big chunks.
// The TX engine receives bunches of mic samples (e.g. 1024),
// and produces bunches of TX IQ samples (1024 * (sample_rate/48)).
// During RX, audio samples are also created in chunks although
// they are smaller, namely 1024 / (sample_rate/48). The "magic"
// constant 1024 is the "buffer size" from the props file and
// can also be 512 or 2048.
//
// So what happens is that the TX IQ FIFO in the SDR is nearly
// drained, then several UDP packets are sent within 1 msec
// and then no further packets are sent for some time. This also
// produces a possible delay when sending the C&C data.
//
// So the idea is to put all the samples that go to the radio into
// a large ring buffer (about 4096 samples), and send them to the
// radio following the pace of incoming mic samples. If we decide
// to send a packet to the radio, we must have at least 126 samples
// in the ring buffer and will send 126 samples (two ozy buffers)
// in one shot.
//
// TXRINGBUF must contain a multiple of 8 bytes (1 sample = 8 bytes,
// since there are four 16-bit numbers, namely I, Q, L, R where
// L and R are the left and right audio samples.
//
// We will try to make the ring buffer access thread-safe, however
// this is not necessary since everything happens within the
// P1 "receive thread".
//
#define TXRINGBUFLEN 32768
static unsigned char TXRINGBUF[TXRINGBUFLEN];
static unsigned int txring_inptr=0;   // pointer updated when writing into the ring buffer
static unsigned int txring_outptr=0;  // pointer updated when reading from the ring buffer
static unsigned int txring_flag=0;    // 0: RX, 1: TX

void dump_buffer(unsigned char *buffer,int length,const char *who) {
  g_mutex_lock(&dump_mutex);
  g_print("%s: %s: %d\n",__FUNCTION__,who,length);
  int i=0;
  int line=0;
  while(i<length) {
    g_print("%02X",buffer[i]);
    i++;
    line++;
    if(line==16) {
      g_print("\n");
      line=0;
    }
  }
  if(line!=0) {
    g_print("\n");
  }
  g_print("\n");
  g_mutex_unlock(&dump_mutex);
}

void old_protocol_stop() {
  //
  // Mutex is needed since in the TCP case, sending TX IQ packets
  // must not occur while the "stop" packet is sent.
  //
  pthread_mutex_lock(&send_ozy_mutex);
#ifdef USBOZY
  if(device!=DEVICE_OZY) {
#endif
    g_print("%s\n",__FUNCTION__);
    metis_start_stop(0);
#ifdef USBOZY
  }
#endif
  pthread_mutex_unlock(&send_ozy_mutex);
}

void old_protocol_run() {
    g_print("%s\n",__FUNCTION__);
    pthread_mutex_lock(&send_ozy_mutex);
    metis_restart();
    pthread_mutex_unlock(&send_ozy_mutex);
}

void old_protocol_set_mic_sample_rate(int rate) {
  mic_sample_divisor=rate/48000;
}

void old_protocol_init(int rx,int pixels,int rate) {
  int i;
  g_print("old_protocol_init: num_hpsdr_receivers=%d\n",how_many_receivers());
  pthread_mutex_lock(&send_ozy_mutex);

  old_protocol_set_mic_sample_rate(rate);

  if(transmitter->local_microphone) {
    if(audio_open_input()!=0) {
      g_print("audio_open_input failed\n");
      transmitter->local_microphone=0;
    }
  }

  display_width=pixels;

#ifdef USBOZY
//
// if we have a USB interfaced Ozy device:
//
  if (device == DEVICE_OZY) {
    g_print("old_protocol_init: initialise ozy on USB\n");
    ozy_initialise();
    start_usb_receive_threads();
  }
  else
#endif
  {
    g_print("old_protocol starting receive thread\n");
    if (radio->use_tcp) {
      open_tcp_socket();
    } else  {
      open_udp_socket();
    }
    receive_thread_id = g_thread_new( "old protocol", receive_thread, NULL);
    if( ! receive_thread_id )
    {
      g_print("g_thread_new failed on receive_thread\n");
      exit( -1 );
    }
    g_print( "receive_thread: id=%p\n",receive_thread_id);
  }

  g_print("old_protocol_init: prime radio\n");
  for(i=8;i<OZY_BUFFER_SIZE;i++) {
    output_buffer[i]=0;
  }

  metis_restart();
  pthread_mutex_unlock(&send_ozy_mutex);

}

#ifdef USBOZY
//
// starts the threads for USB receive
// EP4 is the bandscope endpoint (not yet used)
// EP6 is the "normal" USB frame endpoint
//
static void start_usb_receive_threads()
{
  g_print("old_protocol starting USB receive thread\n");

  ozy_EP6_rx_thread_id = g_thread_new( "OZY EP6 RX", ozy_ep6_rx_thread, NULL);
  if( ! ozy_EP6_rx_thread_id )
  {
    g_print("g_thread_new failed for ozy_ep6_rx_thread\n");
    exit( -1 );
  }
}

//
// receive threat for USB EP4 (bandscope) not currently used.
//
static gpointer ozy_ep4_rx_thread(gpointer arg)
{
  return NULL;
}

//
// receive threat for USB EP6 (512 byte USB Ozy frames)
// this function loops reading 4 frames at a time through USB
// then processes them one at a time.
//
static gpointer ozy_ep6_rx_thread(gpointer arg) {
  int bytes;

  g_print( "old_protocol: USB EP6 receive_thread\n");
  running=1;

  while (running)
  {
    bytes = ozy_read(EP6_IN_ID,ep6_inbuffer,EP6_BUFFER_SIZE); // read a 2K buffer at a time

    //g_print("%s: read %d bytes\n",__FUNCTION__,bytes);
    //dump_buffer(ep6_inbuffer,bytes,__FUNCTION__);

    if (bytes == 0)
    {
      g_print("old_protocol_ep6_read: ozy_read returned 0 bytes... retrying\n");
      continue;
    }
    else if (bytes != EP6_BUFFER_SIZE)
    {
      g_print("old_protocol_ep6_read: OzyBulkRead failed %d bytes\n",bytes);
      perror("ozy_read(EP6 read failed");
    }
    else
// process the received data normally
    {
      process_ozy_input_buffer(&ep6_inbuffer[0]);
      process_ozy_input_buffer(&ep6_inbuffer[512]);
      process_ozy_input_buffer(&ep6_inbuffer[1024]);
      process_ozy_input_buffer(&ep6_inbuffer[1024+512]);
    }

  }
  return NULL;
}
#endif

static void open_udp_socket() {
    int tmp;

    if (data_socket >= 0) {
      tmp=data_socket;
      data_socket=-1;
      usleep(100000);
      close(tmp);
    }
    tmp=socket(PF_INET,SOCK_DGRAM,IPPROTO_UDP);
    if(tmp<0) {
      perror("old_protocol: create socket failed for data_socket\n");
      exit(-1);
    }

    int optval = 1;
    if(setsockopt(tmp, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval))<0) {
      perror("data_socket: SO_REUSEADDR");
    }
    if(setsockopt(tmp, SOL_SOCKET, SO_REUSEPORT, &optval, sizeof(optval))<0) {
      perror("data_socket: SO_REUSEPORT");
    }
    optval=0xffff;
    if (setsockopt(tmp, SOL_SOCKET, SO_SNDBUF, &optval, sizeof(optval))<0) {
      perror("data_socket: SO_SNDBUF");
    }
    if (setsockopt(tmp, SOL_SOCKET, SO_RCVBUF, &optval, sizeof(optval))<0) {
      perror("data_socket: SO_RCVBUF");
    }
#ifdef __APPLE__
    //optval = 0x10;  // IPTOS_LOWDELAY
    optval = 0xb8;  // DSCP EF
    if(setsockopt(tmp, IPPROTO_IP, IP_TOS, &optval, sizeof(optval))<0) {
      perror("data_socket: IP_TOS");
    }
#endif

    //
    // set a timeout for receive
    // This is necessary because we might already "sit" in an UDP recvfrom() call while
    // instructing the radio to switch to TCP. Then this call has to finish eventually
    // and the next recvfrom() then uses the TCP socket.
    //
    struct timeval tv;
    tv.tv_sec=0;
    tv.tv_usec=100000;
    if(setsockopt(tmp, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv))<0) {
      perror("data_socket: SO_RCVTIMEO");
    }

    // bind to the interface
g_print("binding UDP socket to %s:%d\n",inet_ntoa(radio->info.network.interface_address.sin_addr),ntohs(radio->info.network.interface_address.sin_port));
    if(bind(tmp,(struct sockaddr*)&radio->info.network.interface_address,radio->info.network.interface_length)<0) {
      perror("old_protocol: bind socket failed for data_socket\n");
      exit(-1);
    }

    memcpy(&data_addr,&radio->info.network.address,radio->info.network.address_length);
    data_addr.sin_port=htons(DATA_PORT);
    data_socket=tmp;
    g_print("%s: UDP socket established: %d for %s:%d\n",__FUNCTION__,data_socket,inet_ntoa(data_addr.sin_addr),ntohs(data_addr.sin_port));
}

static void open_tcp_socket() {
    int tmp;

    if (tcp_socket >= 0) {
      tmp=tcp_socket;
      tcp_socket=-1;
      usleep(100000);
      close(tmp);
    }
    memcpy(&data_addr,&radio->info.network.address,radio->info.network.address_length);
    data_addr.sin_port=htons(DATA_PORT);
    data_addr.sin_family = AF_INET;
    g_print("Trying to open TCP connection to %s\n", inet_ntoa(radio->info.network.address.sin_addr));

    tmp=socket(AF_INET, SOCK_STREAM, 0);
    if (tmp < 0) {
      perror("tcp_socket: create socket failed for TCP socket");
      exit(-1);
    }
    int optval = 1;
    if(setsockopt(tmp, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval))<0) {
      perror("tcp_socket: SO_REUSEADDR");
    }
    if(setsockopt(tmp, SOL_SOCKET, SO_REUSEPORT, &optval, sizeof(optval))<0) {
      perror("tcp_socket: SO_REUSEPORT");
    }
    if (connect(tmp,(const struct sockaddr *)&data_addr,sizeof(data_addr)) < 0) {
      perror("tcp_socket: connect");
    }
    optval=0xffff;
    if (setsockopt(tmp, SOL_SOCKET, SO_SNDBUF, &optval, sizeof(optval))<0) {
      perror("tcp_socket: SO_SNDBUF");
    }
    if (setsockopt(tmp, SOL_SOCKET, SO_RCVBUF, &optval, sizeof(optval))<0) {
      perror("tcp_socket: SO_RCVBUF");
    }
#ifdef __APPLE__
    //optval = 0x10;  // IPTOS_LOWDELAY
    optval = 0xb8;  // DSCP EF
    if(setsockopt(tmp, IPPROTO_IP, IP_TOS, &optval, sizeof(optval))<0) {
      perror("data_socket: IP_TOS");
    }
#endif
    tcp_socket=tmp;
    g_print("TCP socket established: %d\n", tcp_socket);
}

static gpointer receive_thread(gpointer arg) {
  struct sockaddr_in addr;
  socklen_t length;
  unsigned char buffer[2048];
  int bytes_read;
  int ret,left;
  int ep;
  uint32_t sequence;

  g_print( "old_protocol: receive_thread\n");
  running=1;

  length=sizeof(addr);
  while(running) {

    switch(device) {
#ifdef USBOZY
      case DEVICE_OZY:
        // should not happen
        break;
#endif

      default:
	for (;;) {
          if (tcp_socket >= 0) {
	    // TCP messages may be split, so collect exactly 1032 bytes.
	    // Remember, this is a STREAMING protocol.
            bytes_read=0;
            left=1032;
            while (left > 0) {
              ret=recvfrom(tcp_socket,buffer+bytes_read,(size_t)(left),0,NULL,0);
              if (ret < 0 && errno == EAGAIN) continue; // time-out
              if (ret < 0) break;                       // error
              bytes_read += ret;
              left -= ret;
            }
	    if (ret < 0) {
	      bytes_read=ret;                          // error case: discard whole packet
	    }
	  } else if (data_socket >= 0) {
            bytes_read=recvfrom(data_socket,buffer,sizeof(buffer),0,(struct sockaddr*)&addr,&length);
            if(bytes_read < 0 && errno != EAGAIN) perror("old_protocol recvfrom UDP:");
	    //g_print("%s: bytes_read=%d\n",__FUNCTION__,bytes_read);
          } else {
	    // This could happen in METIS start/stop sequences
	    usleep(100000);
	    continue;
	  }
          if(bytes_read >= 0 || errno != EAGAIN) break;
	}
        if(bytes_read <= 0) {
          continue;
        }

        if(buffer[0]==0xEF && buffer[1]==0xFE) {
          switch(buffer[2]) {
            case 1:
              // get the end point
              ep=buffer[3]&0xFF;

              // get the sequence number
              sequence=((buffer[4]&0xFF)<<24)+((buffer[5]&0xFF)<<16)+((buffer[6]&0xFF)<<8)+(buffer[7]&0xFF);

	      // A sequence error with a seqnum of zero usually indicates a METIS restart
	      // and is no error condition
              if (sequence != 0 && sequence != last_seq_num+1) {
		struct timespec ts;
		double now;
		clock_gettime(CLOCK_MONOTONIC, &ts);
		now=ts.tv_sec + 1E-9*ts.tv_nsec;
		g_print("SEQ ERROR: T=%0.3f last %ld, recvd %ld\n", now, (long) last_seq_num, (long) sequence);
                sequence_errors++;
	      }
	      last_seq_num=sequence;
              switch(ep) {
                case 6: // EP6
                  // process the data
                  process_ozy_input_buffer(&buffer[8]);
                  process_ozy_input_buffer(&buffer[520]);
                  break;
                case 4: // EP4
/*
                  ep4_sequence++;
                  if(sequence!=ep4_sequence) {
                    ep4_sequence=sequence;
                  } else {
                    int seq=(int)(sequence%32L);
                    if((sequence%32L)==0L) {
                      reset_bandscope_buffer_index();
                    }
                    process_bandscope_buffer(&buffer[8]);
                    process_bandscope_buffer(&buffer[520]);
                  }
*/
                  break;
                default:
                  g_print("unexpected EP %d length=%d\n",ep,bytes_read);
                  break;
              }
              break;
            case 2:  // response to a discovery packet
              g_print("unexepected discovery response when not in discovery mode\n");
              break;
            default:
              g_print("unexpected packet type: 0x%02X\n",buffer[2]);
              break;
          }
        } else {
          g_print("received bad header bytes on data port %02X,%02X\n",buffer[0],buffer[1]);
        }
        break;
    }
  }
  return NULL;
}

//
// To avoid overloading code with handling all the different cases
// at various places,
// we define here the channel number of the receivers, as well as the
// number of HPSDR receivers to use (up to 5)
// Furthermore, we provide a function that determines the frequency for
// a given (HPSDR) receiver and for the transmitter.
//
//

static int rx_feedback_channel() {
  //
  // For radios with small FPGAS only supporting 2 RX, use RX1.
  // Else, use the last RX before the TX feedback channel.
  //
  int ret;
  switch (device) {
    case DEVICE_METIS:
    case DEVICE_HERMES_LITE:
#ifdef USBOZY
    case DEVICE_OZY:
#endif
      ret=0;
      break;
    case DEVICE_HERMES:
    case DEVICE_STEMLAB:
    case DEVICE_STEMLAB_Z20:
    case DEVICE_HERMES_LITE2:
      ret=2;
      break;
    case DEVICE_ANGELIA:
    case DEVICE_ORION:
    case DEVICE_ORION2:
      ret=3;
      break;
    default:
      ret=0;
      break;
  }
  return ret;
}

static int tx_feedback_channel() {
  //
  // Radios with small FPGAs use RX2
  // HERMES uses RX4,
  // and Angelia and beyond use RX5
  //
  // This is hard-coded in the firmware.
  //
  int ret;
  switch (device) {
    case DEVICE_METIS:
    case DEVICE_HERMES_LITE:
#ifdef USBOZY
    case DEVICE_OZY:
#endif
      ret=1;
      break;
    case DEVICE_HERMES:
    case DEVICE_STEMLAB:
    case DEVICE_STEMLAB_Z20:
    case DEVICE_HERMES_LITE2:
      ret=3;
      break;
    case DEVICE_ANGELIA:
    case DEVICE_ORION:
    case DEVICE_ORION2:
      ret=4;
      break;
    default:
      ret=1;
      break;
  }
  return ret;
}

static int first_receiver_channel() {
  return 0;
}

static int second_receiver_channel() {
  return 1;
}

static long long channel_freq(int chan) {
  //
  // Return the frequency associated with the current HPSDR
  // RX channel (0 <= chan <= 4).
  //
  // This function returns the TX frequency if chan is
  // outside the allowed range, and thus can be used
  // to set the TX frequency.
  //
  // If transmitting with PureSignal, the frequencies of
  // the feedback and TX DAC channels are set to the TX freq.
  //
  // This subroutine is the ONLY place here where the VFO
  // frequencies are looked at.
  //
  int vfonum;
  long long freq;

  // RX1 and RX2 are normally used for the first and second receiver.
  // all other channels are used for PureSignal and get the TX frequency
  switch (chan) {
    case 0:
      vfonum=receiver[0]->id;
      break;
    case 1:
      if (diversity_enabled) {
	vfonum=receiver[0]->id;
      } else {
	vfonum=receiver[1]->id;
      }
      break;
    default:   // TX frequency used for all other channels
      vfonum=-1;
      break;
  }
  // Radios with small FPGAs use RX1/RX2 for feedback while transmitting,
  //
  if (isTransmitting() && transmitter->puresignal && (chan == rx_feedback_channel() || chan == tx_feedback_channel())) {
    vfonum = -1;
  }
  if (vfonum < 0) {
    //
    // indicates that we should use the TX frequency.
    // We have to adjust by the offset for CTUN mode
    //
    vfonum=get_tx_vfo();
    freq=vfo[vfonum].frequency-vfo[vfonum].lo;
    if (vfo[vfonum].ctun) freq += vfo[vfonum].offset;
    if(transmitter->xit_enabled) {
      freq+=transmitter->xit;
    }
    if (!cw_is_on_vfo_freq) {
      if(vfo[vfonum].mode==modeCWU) {
        freq+=(long long)cw_keyer_sidetone_frequency;
      } else if(vfo[vfonum].mode==modeCWL) {
        freq-=(long long)cw_keyer_sidetone_frequency;
      }
    }
  } else {
    //
    // determine RX frequency associated with VFO #vfonum
    // This is the center freq in CTUN mode.
    //
    freq=vfo[vfonum].frequency-vfo[vfonum].lo;
    if(vfo[vfonum].rit_enabled) {
      freq+=vfo[vfonum].rit;
    }
    if (cw_is_on_vfo_freq) {
      if(vfo[vfonum].mode==modeCWU) {
        freq-=(long long)cw_keyer_sidetone_frequency;
      } else if(vfo[vfonum].mode==modeCWL) {
        freq+=(long long)cw_keyer_sidetone_frequency;
      }
    }
  }
  freq+=frequency_calibration;
  return freq;
}

static int how_many_receivers() {
  //
  // For DIVERSITY, we need at least two RX channels
  // When PureSignal is active, we need to include the TX DAC channel.
  //
  int ret = receivers;   	// 1 or 2
  if (diversity_enabled) ret=2; // need both RX channels, even if there is only one RX

#ifdef USBOZY
  //
  // Always return 2 so the number of HPSDR-RX is NEVER changed.
  // With 2 RX you can do 1RX or 2RX modes, and it is
  // also OK for doing PureSignal on OZY.
  // Rationale: Rick reported that OZY's tend to hang if the
  //            number of receivers is changed while running.
  // OK it wastes bandwidth and this might be a problem at higher
  //    sample rates but this is simply safer.
  //
  if (device == DEVICE_OZY) return 2;
#endif

    // for PureSignal, the number of receivers needed is hard-coded below.
    // we need at least 2, and up to 5 for Orion2 boards. This is so because
    // the TX DAC is hard-wired to RX4 for HERMES,STEMLAB and to RX5 for ANGELIA
    // and beyond.
  if (transmitter->puresignal) {
    switch (device) {
      case DEVICE_METIS:
      case DEVICE_HERMES_LITE:
#ifdef USBOZY
      case DEVICE_OZY:
#endif
	ret=2;  // TX feedback hard-wired to RX2
	break;
      case DEVICE_HERMES:
      case DEVICE_STEMLAB:
      case DEVICE_STEMLAB_Z20:
      case DEVICE_HERMES_LITE2:
	ret=4;  // TX feedback hard-wired to RX4
	break;
      case DEVICE_ANGELIA:
      case DEVICE_ORION:
      case DEVICE_ORION2:
	ret=5;  // TX feedback hard-wired to RX5
	break;
      default:
	ret=2; // This is the minimum for PureSignal
	break;
    }
  }
  return ret;
}

static int nreceiver;
static int left_sample;
static int right_sample;
static short mic_sample;
static double left_sample_double;
static double right_sample_double;
double left_sample_double_rx;
double right_sample_double_rx;
double left_sample_double_tx;
double right_sample_double_tx;
double left_sample_double_main;
double right_sample_double_main;
double left_sample_double_aux;
double right_sample_double_aux;

static int nsamples;
static int iq_samples;

static void process_control_bytes() {
  int previous_ptt;
  int previous_dot;
  int previous_dash;

  // do not set ptt. In PureSignal, this would stop the
  // receiver sending samples to WDSP abruptly.
  // Do the RX-TX change only via ext_mox_update.
  previous_ptt=local_ptt;
  previous_dot=dot;
  previous_dash=dash;
  local_ptt=(control_in[0]&0x01)==0x01;
  dash=(control_in[0]&0x02)==0x02;
  dot=(control_in[0]&0x04)==0x04;

  // Stops CAT cw transmission if radio reports "CW action"
  if (dash || dot) {
    cw_key_hit=1;
    CAT_cw_is_active=0;
  }
#ifdef LOCALCW
  if (!cw_keyer_internal) {
    if (dash != previous_dash) keyer_event(0, dash);
    if (dot  != previous_dot ) keyer_event(1, dot );
  }
#else
    //
    // Note that if an external keyer is connected to the "CW" jack of
    // the ANAN-7000, it will report its state via the "dot" state
    // so we can do CW directly. Only act on dot state changes so we
    // do not intervene with CAT CW. Note it is assumed that the external
    // keyer also takes care of PTT.
    //
    if (!cw_keyer_internal && dot != previous_dot) {
      cw_key_down=dot ? 960000 : 0;
    }
#endif

  if(previous_ptt!=local_ptt) {
    g_idle_add(ext_mox_update,(gpointer)(long)(local_ptt));
  }

  switch((control_in[0]>>3)&0x1F) {
    case 0:
      adc_overload=control_in[1]&0x01;
      if (device != DEVICE_HERMES_LITE2) {
        //
        // HL2 uses these bits of the protocol for a different purpose:
        // C1 unused except the ADC overload bit
        // C2/C3 contains underflow/overflow and TX FIFO count
        //
        IO1=(control_in[1]&0x02)?0:1;
        IO2=(control_in[1]&0x04)?0:1;
        IO3=(control_in[1]&0x08)?0:1;
        if(mercury_software_version!=control_in[2]) {
          mercury_software_version=control_in[2];
          g_print("  Mercury Software version: %d (0x%0X)\n",mercury_software_version,mercury_software_version);
        }
        if(penelope_software_version!=control_in[3]) {
          penelope_software_version=control_in[3];
          g_print("  Penelope Software version: %d (0x%0X)\n",penelope_software_version,penelope_software_version);
        }
      }
      //
      //DEBUG code to monitor HL2 TX-FIFO filling and
      //underflow/overflow detection.
      //
      // Measured on HL2 software version 7.2:
      // multiply FIFO value with 32 to get sample count
      // multiply FIFO value with 0.67 to get FIFO length in milli-seconds
      // Overflow at about 3600 samples (75 msec).
      //
      // As a result, we set the "TX latency" to 40 msec (see below).
      //
      //
      // Note after an RX/TX transition, "underflow" is reported
      // until the TX fifo begins to fill, so we ignore underflows
      // until the first packet reporting "no underflow" after each
      // RX/TX transition.
      //
      if (device == DEVICE_HERMES_LITE2) {
           if (!isTransmitting()) {
              // during RX: set flag to zero
              tx_fifo_flag=0;
           } else {
             // after RX/TX transition: ignore underflow condition
             // until it first vanishes. tx_fifo_flag becomes "true"
             // as soon as a "no underflow" condition is seen.
             //
             if ((control_in[3] & 0xC0) != 0x80) tx_fifo_flag=1;
             if ((control_in[3] & 0xC0) == 0x80 && tx_fifo_flag) tx_fifo_underrun=1;
             if ((control_in[3] & 0xC0) == 0xC0) tx_fifo_overrun=1;
           }
      }
      if(ozy_software_version!=control_in[4]) {
        ozy_software_version=control_in[4];
        g_print("FPGA firmware version: %d.%d\n",ozy_software_version/10,ozy_software_version%10);
      }
      break;
    case 1:
      if (device != DEVICE_HERMES_LITE2) {
        //
        // HL2 uses C1/C2 for measuring the temperature
        //
        exciter_power=((control_in[1]&0xFF)<<8)|(control_in[2]&0xFF); // from Penelope or Hermes
        temperature=0;
      } else {
        exciter_power=0;
        temperature+=((control_in[1]&0xFF)<<8)|(control_in[2]&0xFF); // HL2
        n_temperature++;
        if(n_temperature==10) {
          average_temperature=temperature/10;
          temperature=0;
          n_temperature=0;
        }
      }
      //
      //  calculate moving averages of fwd and rev voltages to have a correct SWR
      //  at the edges of an RF pulse. Otherwise a false trigger of the SWR
      //  protection may occur. Note that the rate at which these packets arrive strongly
      //  depend on the sample rate and the number of receivers.
      //  This exponential average means that the power drops to 1 percent within 16 hits
      //  (at most 16 msec).
      //
      alex_forward_power=((control_in[3]&0xFF)<<8)|(control_in[4]&0xFF); // from Alex or Apollo
      alex_forward_power_average = (alex_forward_power + 3*alex_forward_power_average) >> 2;
      break;
    case 2:
      alex_reverse_power=((control_in[1]&0xFF)<<8)|(control_in[2]&0xFF); // from Alex or Apollo
      alex_reverse_power_average = (alex_reverse_power + 3*alex_reverse_power_average) >> 2;
      if (device != DEVICE_HERMES_LITE2) {
        AIN3=((control_in[3]&0xFF)<<8)|(control_in[4]&0xFF); // For Penelope or Hermes
        current=0;
      } else {
        AIN3=0;
        current+=((control_in[3]&0xFF)<<8)|(control_in[4]&0xFF); // HL2
        n_current++;
        if(n_current==10) {
          average_current=current/10;
          current=0;
          n_current=0;
        }
      }
      break;
    case 3:
      AIN4=((control_in[1]&0xFF)<<8)|(control_in[2]&0xFF); // For Penelope or Hermes
      AIN6=((control_in[3]&0xFF)<<8)|(control_in[4]&0xFF); // For Penelope or Hermes
      break;
  }

}

static int num_hpsdr_receivers;
static int rxfdbk;
static int txfdbk;
static int rx1channel;
static int rx2channel;

static void process_ozy_byte(int b) {
  float fsample;
  switch(state) {
    case SYNC_0:
      if(b==SYNC) {
        state++;
      }
      break;
    case SYNC_1:
      if(b==SYNC) {
        state++;
      }
      break;
    case SYNC_2:
      if(b==SYNC) {
        state++;
      }
      break;
    case CONTROL_0:
      control_in[0]=b;
      state++;
      break;
    case CONTROL_1:
      control_in[1]=b;
      state++;
      break;
    case CONTROL_2:
      control_in[2]=b;
      state++;
      break;
    case CONTROL_3:
      control_in[3]=b;
      state++;
      break;
    case CONTROL_4:
      control_in[4]=b;
      process_control_bytes();
      nreceiver=0;
      iq_samples=(512-8)/((num_hpsdr_receivers*6)+2);
      nsamples=0;
      state++;
      break;
    case LEFT_SAMPLE_HI:
      left_sample=(int)((signed char)b<<16);
      state++;
      break;
    case LEFT_SAMPLE_MID:
      left_sample|=(int)((((unsigned char)b)<<8)&0xFF00);
      state++;
      break;
    case LEFT_SAMPLE_LOW:
      left_sample|=(int)((unsigned char)b&0xFF);
      left_sample_double=(double)left_sample*1.1920928955078125E-7;
      state++;
      break;
    case RIGHT_SAMPLE_HI:
      right_sample=(int)((signed char)b<<16);
      state++;
      break;
    case RIGHT_SAMPLE_MID:
      right_sample|=(int)((((unsigned char)b)<<8)&0xFF00);
      state++;
      break;
    case RIGHT_SAMPLE_LOW:
      right_sample|=(int)((unsigned char)b&0xFF);
      right_sample_double=(double)right_sample*1.1920928955078125E-7;

      if (isTransmitting() && transmitter->puresignal) {
        //
        // transmitting with PureSignal. Get sample pairs and feed to pscc
        //
        if (nreceiver == rxfdbk) {
          left_sample_double_rx=left_sample_double;
          right_sample_double_rx=right_sample_double;
        } else if (nreceiver == txfdbk) {
          left_sample_double_tx=left_sample_double;
          right_sample_double_tx=right_sample_double;
        }
        // this is pure paranoia, it allows for txfdbk < rxfdbk
        if (nreceiver+1 == num_hpsdr_receivers) {
          add_ps_iq_samples(transmitter, left_sample_double_tx,right_sample_double_tx,left_sample_double_rx,right_sample_double_rx);
        }
      }

      if (!isTransmitting() && diversity_enabled) {
        //
        // receiving with DIVERSITY. Get sample pairs and feed to diversity mixer
        //
        if (nreceiver == rx1channel) {
          left_sample_double_main=left_sample_double;
          right_sample_double_main=right_sample_double;
        } else if (nreceiver == rx2channel) {
          left_sample_double_aux=left_sample_double;
          right_sample_double_aux=right_sample_double;
        }
        // this is pure paranoia, it allows for rx2channel < rx1channel
        if (nreceiver+1 == num_hpsdr_receivers) {
          add_div_iq_samples(receiver[0], left_sample_double_main,right_sample_double_main,left_sample_double_aux,right_sample_double_aux);
          // if we have a second receiver, display "auxiliary" receiver as well
          if (receivers >1) add_iq_samples(receiver[1], left_sample_double_aux,right_sample_double_aux);
        }
      }

      if ((!isTransmitting() || duplex) && !diversity_enabled) {
        //
        // RX without DIVERSITY. Feed samples to RX1 and RX2
        //
        if (nreceiver == rx1channel) {
           add_iq_samples(receiver[0], left_sample_double,right_sample_double);
        } else if (nreceiver == rx2channel && receivers > 1) {
           add_iq_samples(receiver[1], left_sample_double,right_sample_double);
        }
      }
      nreceiver++;
      if(nreceiver==num_hpsdr_receivers) {
        state++;
      } else {
        state=LEFT_SAMPLE_HI;
      }
      break;
    case MIC_SAMPLE_HI:
      mic_sample=(short)(b<<8);
      state++;
      break;
    case MIC_SAMPLE_LOW:
      mic_sample|=(short)(b&0xFF);
      mic_samples++;
      if(mic_samples>=mic_sample_divisor) { // reduce to 48000
        //
        // if local_ptt is set, this usually means the PTT at the microphone connected
        // to the SDR is pressed. In this case, we take audio from BOTH sources
        // then we can use a "voice keyer" on some loop-back interface but at the same
        // time use our microphone.
        // In most situations only one source will be active so we just add.
        //
        if (local_ptt) {
          fsample = (float) mic_sample * 0.00003051;
          if (transmitter->local_microphone) fsample += audio_get_next_mic_sample();
        } else {
          fsample = transmitter->local_microphone ? audio_get_next_mic_sample() : (float) mic_sample * 0.00003051;
        }
        add_mic_sample(transmitter,fsample);
        // micsamplecount is the "heart beat" for sending data from the
        // ring buffer to the radio
        micsamplecount++;
        mic_samples=0;
      }
      nsamples++;
      if(nsamples==iq_samples) {
        state=SYNC_0;
      } else {
        nreceiver=0;
        state=LEFT_SAMPLE_HI;
      }
      break;
  }
}

static void process_ozy_input_buffer(unsigned const char  *buffer) {
  int i;
  num_hpsdr_receivers=how_many_receivers();
  rxfdbk = rx_feedback_channel();
  txfdbk = tx_feedback_channel();
  rx1channel = first_receiver_channel();
  rx2channel = second_receiver_channel();
  for(i=0;i<512;i++) {
    process_ozy_byte(buffer[i]&0xFF);
    //
    // The first time data is available, micsamplecount will be
    // MUCH larger than 126. It is reset to zero once the first
    // buffer has been sent. This way, a complete buffer is sent
    // as early as possible. Note we send two buffers at once since
    // METIS sends buffers in UDP packets containing two of them.
    //
    if (micsamplecount >= 126) {
      int avail = txring_inptr - txring_outptr;
      if (avail < 0) avail += TXRINGBUFLEN;
      if (avail >= 1008) {
        //
        // ship out two buffers with 2*63 samples
        // the k-loop could be done with 1-2 memcpy
        //
        if (pthread_mutex_trylock(&send_ozy_mutex)) {
	  //
	  // This can only happen if the GUI thread initiates
	  // a protocol stop/start sequence, as it does e.g.
	  // when changing the number of receivers, changing
	  // the sample rate, en/dis-abling PureSignal or
	  // DIVERSITY, or executing the RESTART button.
	  // In these cases, the TX ring buffer is reset anywas
	  // so we do not have to do anything here.
	  //
        } else {
          for (int j=0; j<2; j++) {
            unsigned char *p=output_buffer+8;
            for (int k=0; k<63; k++) {
              *p++ = TXRINGBUF[txring_outptr++];
              *p++ = TXRINGBUF[txring_outptr++];
              *p++ = TXRINGBUF[txring_outptr++];
              *p++ = TXRINGBUF[txring_outptr++];
              *p++ = TXRINGBUF[txring_outptr++];
              *p++ = TXRINGBUF[txring_outptr++];
              *p++ = TXRINGBUF[txring_outptr++];
              *p++ = TXRINGBUF[txring_outptr++];
              if (txring_outptr >= TXRINGBUFLEN) txring_outptr=0;
            }
            ozy_send_buffer();
          }
          micsamplecount=0;
	  pthread_mutex_unlock(&send_ozy_mutex);
	}
      }
    }
  }
}

void old_protocol_audio_samples(RECEIVER *rx,short left_audio_sample,short right_audio_sample) {
  if(!isTransmitting()) {
    pthread_mutex_lock(&send_audio_mutex);
    if (txring_flag) {
      //
      // First time we arrive here after a TX->RX transition:
      // Clear TX IQ ring buffer, so the audio samples will be sent
      // as soon as possible at the radio.
      //
      txring_flag=0;
      txring_inptr = txring_outptr = 0;
    }
    //
    // The HL2 makes no use of audio samples, but instead
    // uses them to write to extended addrs which we do not
    // want to do un-intentionally, therefore send zeros.
    // Note special variants of the HL2 *do* have an audio codec!
    //
    if (device == DEVICE_HERMES_LITE2 && !hl2_audio_codec) {
      TXRINGBUF[txring_inptr++]=0;
      TXRINGBUF[txring_inptr++]=0;
      TXRINGBUF[txring_inptr++]=0;
      TXRINGBUF[txring_inptr++]=0;
    } else {
      TXRINGBUF[txring_inptr++]=left_audio_sample>>8;
      TXRINGBUF[txring_inptr++]=left_audio_sample;
      TXRINGBUF[txring_inptr++]=right_audio_sample>>8;
      TXRINGBUF[txring_inptr++]=right_audio_sample;
    }
    TXRINGBUF[txring_inptr++]=0;
    TXRINGBUF[txring_inptr++]=0;
    TXRINGBUF[txring_inptr++]=0;
    TXRINGBUF[txring_inptr++]=0;
    if (txring_inptr >= TXRINGBUFLEN) txring_inptr=0;
    pthread_mutex_unlock(&send_audio_mutex);
  }
}

void old_protocol_iq_samples(int isample, int qsample, int side) {
  if(isTransmitting()) {
    pthread_mutex_lock(&send_audio_mutex);
    if (!txring_flag) {
      //
      // First time we arrive here after a RX->TX transition:
      // Clear tX IQ ring buffer so the samples will be sent
      // as soon as possible.
      //
      txring_flag=1;
      txring_inptr = txring_outptr = 0;
    }
    if (device == DEVICE_HERMES_LITE2) {
      TXRINGBUF[txring_inptr++]=0;
      TXRINGBUF[txring_inptr++]=0;
      TXRINGBUF[txring_inptr++]=0;
      TXRINGBUF[txring_inptr++]=0;
    } else {
      TXRINGBUF[txring_inptr++]=side >> 8;
      TXRINGBUF[txring_inptr++]=side;
      TXRINGBUF[txring_inptr++]=side >> 8;
      TXRINGBUF[txring_inptr++]=side;
    }
    TXRINGBUF[txring_inptr++]=isample >> 8;
    TXRINGBUF[txring_inptr++]=isample;
    TXRINGBUF[txring_inptr++]=qsample >> 8;
    TXRINGBUF[txring_inptr++]=qsample;
    if (txring_inptr >= TXRINGBUFLEN) txring_inptr=0;
    pthread_mutex_unlock(&send_audio_mutex);
  }
}

void ozy_send_buffer() {


  int txmode=get_tx_mode();
  int txvfo=get_tx_vfo();
  int i;
  BAND *rxband=band_get_band(vfo[VFO_A].band);
  BAND *txband=band_get_band(vfo[txvfo].band);
  int power;
  int num_hpsdr_receivers=how_many_receivers();
  int rx1channel = first_receiver_channel();
  int rx2channel = second_receiver_channel();

  output_buffer[SYNC0]=SYNC;
  output_buffer[SYNC1]=SYNC;
  output_buffer[SYNC2]=SYNC;

  if(metis_offset==8) {
    //
    // Every second packet is a "C0=0" packet
    // (for JANUS, *every* packet is a "C0=0" packet
    //
    output_buffer[C0]=0x00;
    output_buffer[C1]=0x00;
    switch(active_receiver->sample_rate) {
      case 48000:
        output_buffer[C1]|=SPEED_48K;
        break;
      case 96000:
        output_buffer[C1]|=SPEED_96K;
        break;
      case 192000:
        output_buffer[C1]|=SPEED_192K;
        break;
      case 384000:
        output_buffer[C1]|=SPEED_384K;
        break;
    }

// set more bits for Atlas based device
// CONFIG_BOTH seems to be critical to getting ozy to respond
#ifdef USBOZY
    if ((device == DEVICE_OZY) || (device == DEVICE_METIS))
#else
    if (device == DEVICE_METIS)
#endif
    {
      //
      // A. Assume a mercury board is *always* present (set CONFIG_MERCURY)
      //
      // B. Set CONFIG_PENELOPE in either of the cases
      // - a penelope or pennylane TX is selected (atlas_penelope != 0)
      // - a penelope is specified as mic source (atlas_mic_source != 0)
      // - the penelope is the source for the 122.88 Mhz clock (atlas_clock_source_128mhz == 0)
      // - the penelope is the source for the 10 Mhz reference (atlas_clock_source_10mhz == 1)
      //
      // So if neither penelope nor pennylane is selected but referenced as clock or mic source,
      // a pennylane is chosen implicitly (not no drive level adjustment via IQ scaling in this case!)
      // and CONFIG_BOTH becomes effective.
      //

      output_buffer[C1] |= CONFIG_MERCURY;

      if (atlas_penelope) {
        output_buffer[C1] |= CONFIG_PENELOPE;
      }

      if (atlas_mic_source) {
        output_buffer[C1] |= PENELOPE_MIC;
        output_buffer[C1] |= CONFIG_PENELOPE;
      }

      if (atlas_clock_source_128mhz) {
        output_buffer[C1] |= MERCURY_122_88MHZ_SOURCE;	// Mercury provides 122 Mhz
      } else {
        output_buffer[C1] |= PENELOPE_122_88MHZ_SOURCE;	// Penelope provides 122 Mhz
        output_buffer[C1] |= CONFIG_PENELOPE;
      }
      switch (atlas_clock_source_10mhz) {
	case 0:
	  output_buffer[C1] |= ATLAS_10MHZ_SOURCE;	// ATLAS provides 10 Mhz
	  break;
	case 1:
	  output_buffer[C1] |= PENELOPE_10MHZ_SOURCE;	// Penelope provides 10 Mhz
	  output_buffer[C1] |= CONFIG_PENELOPE;
	  break;
	case 2:
	  output_buffer[C1] |= MERCURY_10MHZ_SOURCE;	// Mercury provides 10 MHz
	  break;
      }
   }

#ifdef USBOZY
    //
    // This is for "Janus only" operation
    //
    if (device == DEVICE_OZY && atlas_janus) {
      output_buffer[C2]=0x00;
      output_buffer[C3]=0x00;
      output_buffer[C4]=0x00;
      ozyusb_write(output_buffer,OZY_BUFFER_SIZE);
      metis_offset=8;  // take care next packet is a C0=0 packet
      return;
    }
#endif

    output_buffer[C2]=0x00;
    if(classE) {
      output_buffer[C2]|=0x01;
    }
    if(isTransmitting()) {
      output_buffer[C2]|=txband->OCtx<<1;
      if(tune) {
        if(OCmemory_tune_time!=0) {
          struct timeval te;
          gettimeofday(&te,NULL);
          long long now=te.tv_sec*1000LL+te.tv_usec/1000;
          if(tune_timeout>now) {
            output_buffer[C2]|=OCtune<<1;
          }
        } else {
          output_buffer[C2]|=OCtune<<1;
        }
      }
    } else {
      output_buffer[C2]|=rxband->OCrx<<1;
    }

    output_buffer[C3] = (receiver[0]->alex_attenuation) & 0x03;  // do not set higher bits
    if(active_receiver->random) {
      output_buffer[C3]|=LT2208_RANDOM_ON;
    }
    if(active_receiver->dither) {
	output_buffer[C3]|=LT2208_DITHER_ON;
    }
    //
    // Some  HL2 firmware variants (ab-) uses this bit for indicating an audio codec is present
    // We also  accept explicit use  of the "dither" box
    //
    if(device == DEVICE_HERMES_LITE2 && hl2_audio_codec) {
	output_buffer[C3]|=LT2208_DITHER_ON;
    }
    if (filter_board == CHARLY25 && active_receiver->preamp) {
      output_buffer[C3]|=LT2208_GAIN_ON;
    }

    //
    // Set ALEX RX1_ANT and RX1_OUT
    //
    i=receiver[0]->alex_antenna;
    //
    // Upon TX, we might have to activate a different RX path for the
    // attenuated feedback signal. Use alex_antenna == 0, if
    // the feedback signal is routed automatically/internally
    // If feedback is to the second ADC, leave RX1 ANT settings untouched
    //
    if (isTransmitting() && transmitter->puresignal) i=receiver[PS_RX_FEEDBACK]->alex_antenna;
    if (device == DEVICE_ORION2) {
      i +=100;
    } else if (new_pa_board) {
      // New-PA setting invalid on ANAN-7000,8000
      i +=1000;
    }
    //
    // There are several combination which do not exist (no jacket present)
    // or which do not work (using EXT1-on-TX with ANAN-7000).
    // In these cases, fall back to a "reasonable" case (e.g. use EXT1 if
    // there is no EXT2).
    // As a result, the "New PA board" setting is overriden for PureSignal
    // feedback: EXT1 assumes old PA board and ByPass assumes new PA board.
    //
    switch(i) {
      case 3: 		// EXT1 with old pa board
      case 6: 		// EXT1-on-TX: assume old pa board
      case 1006:
        output_buffer[C3] |= 0xC0;
        break;
      case 4:		// EXT2 with old pa board
        output_buffer[C3] |= 0xA0;
        break;
      case 5:		// XVTR with old pa board
        output_buffer[C3] |= 0xE0;
        break;
      case 104:		// EXT2 with ANAN-7000: does not exist, use EXT1
      case 103:		// EXT1 with ANAN-7000
        output_buffer[C3]|= 0x40;
        break;
      case 105:		// XVTR with ANAN-7000
        output_buffer[C3]|= 0x60;
        break;
      case 106:		// EXT1-on-TX with ANAN-7000: does not exist, use ByPass
      case 107:		// Bypass-on-TX with ANAN-7000
        output_buffer[C3]|= 0x20;
	break;
      case 1003:	// EXT1 with new PA board
        output_buffer[C3] |= 0x40;
	break;
      case 1004:	// EXT2 with new PA board
        output_buffer[C3] |= 0x20;
	break;
      case 1005:	// XVRT with new PA board
        output_buffer[C3] |= 0x60;
	break;
      case 7:		// Bypass-on-TX: assume new PA board
      case 1007:
        output_buffer[C3] |= 0x80;
	break;
    }

    //
    // ALWAYS set the duplex bit "on". This bit indicates to the
    // FPGA that the TX frequency can be different from the RX
    // frequency, which is the case with Split, XIT, CTUN
    //
    output_buffer[C4]=0x04;
    //
    // This is used to phase-synchronize RX1 and RX2 on some boards
    // and enforces that the RX1 and RX2 frequencies are the same.
    //
    if (diversity_enabled) output_buffer[C4] |= 0x80;

    // 0 ... 7 maps on 1 ... 8 receivers
    output_buffer[C4]|=(num_hpsdr_receivers-1)<<3;

//
//  Now we set the bits for Ant1/2/3 (RX and TX may be different)
//
    if(isTransmitting()) {
      i=transmitter->alex_antenna;
      //
      // TX antenna outside allowd range: this cannot happen.
      // Out of paranoia: print warning and choose ANT1
      //
      if (i<0 || i>2) {
          g_print("WARNING: illegal TX antenna chosen, using ANT1\n");
          transmitter->alex_antenna=0;
          i=0;
      }
    } else {
      i=receiver[0]->alex_antenna;
      //
      // Not using ANT1,2,3: can leave relais in TX state unless using new PA board
      //
      if (i > 2 && !new_pa_board) i=transmitter->alex_antenna;
    }

    switch(i) {
        case 0:  // ANT 1
          output_buffer[C4]|=0x00;
          break;
        case 1:  // ANT 2
          output_buffer[C4]|=0x01;
          break;
        case 2:  // ANT 3
          output_buffer[C4]|=0x02;
          break;
        default:
	  // this happens only with the new pa board and using EXT1/EXT2/XVTR
	  // here we have to disconnect ANT1,2,3
          output_buffer[C4]|=0x03;
          break;
    }
    // end of "C0=0" packet
  } else {
    //
    // metis_offset !=8: send the other C&C packets in round-robin
    // RX frequency commands are repeated for each RX
    output_buffer[C1]=0x00;
    output_buffer[C2]=0x00;
    output_buffer[C3]=0x00;
    output_buffer[C4]=0x00;
    switch(command) {
      case 1: // tx frequency
        output_buffer[C0]=0x02;
        long long txFrequency=channel_freq(-1);
        output_buffer[C1]=txFrequency>>24;
        output_buffer[C2]=txFrequency>>16;
        output_buffer[C3]=txFrequency>>8;
        output_buffer[C4]=txFrequency;
        command=2;
        break;
      case 2: // rx frequency
        if(current_rx<num_hpsdr_receivers) {
          output_buffer[C0]=0x04+(current_rx*2);
	  long long rxFrequency=channel_freq(current_rx);
          output_buffer[C1]=rxFrequency>>24;
          output_buffer[C2]=rxFrequency>>16;
          output_buffer[C3]=rxFrequency>>8;
          output_buffer[C4]=rxFrequency;
          current_rx++;
        }
	// if we have reached the last RX channel, wrap around
        // and proceed with the next "command"
        if(current_rx>=num_hpsdr_receivers) {
          current_rx=0;
          command=3;
        }
        break;
      case 3:
        power=0;
	//
	// Some HPSDR apps for the RedPitaya generate CW inside the FPGA, but while
	// doing this, DriveLevel changes are processed by the server, but do not become effective.
	// If the CW paddle is hit, the new PTT state is sent to piHPSDR, then the TX drive
	// is sent the next time "command 3" is performed, but this often is too late and
	// CW is generated with zero DriveLevel.
	// Therefore, when in CW mode, send the TX drive level also when receiving.
	// (it would be sufficient to do so only with internal CW).
	//
        if(isTransmitting() || (txmode == modeCWU) || (txmode == modeCWL)) {
          power=transmitter->drive_level;
          if (device == DEVICE_HERMES_LITE2) {
	    //
	    // from the "intended" drive level power, calculate the
	    // next lower TX attenuation which can be from 0.0 to -7.5 dB
	    // in 0.5 dB steps, encode the step in a four-bit word and shift
	    // it to the upper 4 bits.
	    // we always use the att level that produces a little bit *less* attenuation
	    // than required, and down-scale the IQ samples in transmitter.c
	    //
	    // NOTE: this down-scaling does not occur when connecting a CW key to the HL2
	    //       and using "internal" CW, because in this case the IQ samples are
	    //       generated in the FPGA. A typical symptom is that the CW signals are
	    //       stronger than the "TUNE" signal, and in the case of very low drive slider
	    //       values they can be *much* stronger.
	    //
	    // NOTE: When using predistortion (PureSignal), the IQ scaling must be switched off.
	    //       In this case, the output  power can also be stronger than intended.
	    //
            int hl2power;
            if (power < 102) {
              hl2power=0;                   // -7.5 dB TX attenuation
            } else if (power < 108) {
              hl2power=16;                  // -7.0 dB TX attenuation
            } else if (power < 114) {
              hl2power=32;                  // -6.5 dB TX attenuation
            } else if (power < 121) {
              hl2power=48;                  // -6.0 dB TX attenuation
            } else if (power < 128) {
              hl2power=64;                  // -5.5 dB TX attenuation
            } else if (power < 136) {
              hl2power=80;                  // -5.0 dB TX attenuation
            } else if (power < 144) {
              hl2power=96;                  // -4.5 dB TX attenuation
            } else if (power < 152) {
              hl2power=112;                 // -4.0 dB TX attenuation
            } else if (power < 161) {
              hl2power=128;                 // -3.5 dB TX attenuation
            } else if (power < 171) {
              hl2power=144;                 // -3.0 dB TX attenuation
            } else if (power < 181) {
              hl2power=160;                 // -2.5 dB TX attenuation
            } else if (power < 192) {
              hl2power=176;                 // -2.0 dB TX attenuation
            } else if (power < 203) {
              hl2power=192;                 // -1.5 dB TX attenuation
            } else if (power < 215) {
              hl2power=208;                 // -1.0 dB TX attenuation
            } else if (power < 228) {
              hl2power=224;                 // -0.5 dB TX attenuation
            } else {
              hl2power=240;                 // no      TX attenuation
            }
            power=hl2power;
          }
        }

       //fprintf(stderr,"%s: TXband=%s disablePA=%d\n",__FUNCTION__,txband->title,txband->disablePA);


        output_buffer[C0]=0x12;
        output_buffer[C1]=power&0xFF;
        if(mic_boost) {
          output_buffer[C2]|=0x01;
        }
        if(mic_linein) {
          output_buffer[C2]|=0x02;
        }
        if(filter_board==APOLLO) {
          output_buffer[C2]|=0x2C;
        }
        if((filter_board==APOLLO) && tune) {
          output_buffer[C2]|=0x10;
        }
        if(band_get_current()==band6) {
          output_buffer[C3]=output_buffer[C3]|0x40; // Alex 6M low noise amplifier
        }
        if (txband->disablePA || !pa_enabled) {
          output_buffer[C3]|=0x80; // disable Alex T/R relay
          if(isTransmitting()) {
            output_buffer[C2]|=0x40; // Manual Filter Selection
            output_buffer[C3]|=0x20; // bypass all RX filters
          }
        }
	//
	// If using PureSignal and a feedback to EXT1, we have to manually activate the RX HPF/BPF
	// filters and select "bypass" since the feedback signal must arrive at the board
        // un-altered. This is not necessary for feedback at the "ByPass" jack since filter bypass
        // is realized in hardware here.
	//
        if (isTransmitting() && transmitter->puresignal && receiver[PS_RX_FEEDBACK]->alex_antenna == 6) {
          output_buffer[C2] |= 0x40;  // enable manual filter selection
          output_buffer[C3] &= 0x80;  // preserve ONLY "PA enable" bit and clear all filters including "6m LNA"
          output_buffer[C3] |= 0x20;  // bypass all RX filters
	  //
	  // For "manual" filter selection we also need to select the appropriate TX LPF
	  //
	  // We here use the transition frequencies used in Thetis by default. Note the
	  // P1 firmware has different default transition frequences.
	  // Even more odd, HERMES routes 15m through the 10/12 LPF, while
	  // Angelia routes 12m through the 17/15m LPF.
	  //
	  long long txFrequency = channel_freq(-1);
	  if (txFrequency > 35600000L) {		// > 10m so use 6m LPF
	    output_buffer[C4] = 0x10;
	  } else if (txFrequency > 24000000L)  {	// > 15m so use 10/12m LPF
	    output_buffer[C4] = 0x20;
	  } else if (txFrequency > 16500000L) {		// > 20m so use 17/15m LPF
	    output_buffer[C4] = 0x40;
	  } else if (txFrequency >  8000000L) {		// > 40m so use 30/20m LPF
	    output_buffer[C4] = 0x01;
	  } else if (txFrequency >  5000000L) {		// > 80m so use 60/40m LPF
	    output_buffer[C4] = 0x02;
	  } else if (txFrequency >  2500000L) {		// > 160m so use 80m LPF
	    output_buffer[C4] = 0x04;
	  } else {					// < 2.5 MHz use 160m LPF
	    output_buffer[C4] = 0x08;
	  }
	}
        if (device==DEVICE_HERMES_LITE2) {
          // do not set any Apollo/Alex bits (ADDR=0x09 bits 0:23)
          // ADDR=0x09 bit 19 follows "PA enable" state
          // ADDR=0x09 bit 20 follows "TUNE" state
          // ADDR=0x09 bit 18 always cleared (external tuner enabled)
          output_buffer[C2]= 0x00;
          output_buffer[C3]= 0x00;
          output_buffer[C4]= 0x00;
          if (pa_enabled && !txband->disablePA) output_buffer[C2] |= 0x08;
          if (tune)                             output_buffer[C2] |= 0x10;
        }
        command=4;
        break;
      case 4:
        output_buffer[C0]=0x14;

#ifdef USBOZY
        if ((device == DEVICE_OZY) || (device == DEVICE_METIS)) {
#else
        if (device == DEVICE_METIS) {
#endif
          for(i=0;i<receivers;i++) {
            output_buffer[C1]|=(receiver[i]->preamp<<i);
          }
        }
        if(mic_ptt_enabled==0) {
          output_buffer[C1]|=0x40;
        }
        if(mic_bias_enabled) {
          output_buffer[C1]|=0x20;
        }
        if(mic_ptt_tip_bias_ring) {
          output_buffer[C1]|=0x10;
        }
        output_buffer[C2]|=linein_gain;
        if(transmitter->puresignal) {
          output_buffer[C2]|=0x40;
        }

	// upon TX, use transmitter->attenuation
	// Usually the firmware takes care of this, but it is no
	// harm to do this here as well
        if (have_rx_gain) {
	  //
	  // HERMESlite has a RXgain value in the range 0-60 that
	  // is stored in rx_gain_slider. The firmware uses bit 6
	  // of C4 to determine this case.
	  //
          int rxgain = adc[active_receiver->adc].gain+12; // -12..48 to 0..60
          if (rxgain <  0) rxgain=0;
          if (rxgain > 60) rxgain=60;
	  // encode all 6 bits of RXgain in ATT value and set bit6
          if (isTransmitting()) {
            //
            // The "TX attenuation" value (0 ... 31 dB) has to be mapped to a
            // a range of preamp settings. This range is very different on the
            // HermesLite when using "internal" feedback (crosstalk from the
            // RX/TX releay) or "external" feedback (attenuator output
            // connected with RF3 input of the HermesLite2).
            // To cope with both situations, the preamp range is
            // +2 ... +33 dB if "Internal" feedback is selected in the
            // PS menu (this is optimal for "crosstalk" feedback) and
            // if external feedback is used (check either EXT1 or ByPass
            // in the PS menu) the preamp range is -12 ... +19 dB.
            //
           output_buffer[C4] = 0x40 | (33 - (transmitter->attenuation & 0x1F));
           if (receiver[PS_RX_FEEDBACK]->alex_antenna == 0) {
             output_buffer[C4] = 0x40 | (45 - (transmitter->attenuation & 0x1F));
            }
          } else {
	    output_buffer[C4] = 0x40 | (rxgain & 0x3F);
          }
        } else {
          if (isTransmitting()) {
            output_buffer[C4]=0x20 | (transmitter->attenuation & 0x1F);
          } else {
            output_buffer[C4]=0x20 | (adc[0].attenuation & 0x1F);
          }
        }
        command=5;
	break;
      case 5:
        output_buffer[C0]=0x16;
        if(n_adc==2) {
	  // must set bit 5 ("Att enable") all the time
          // upon transmitting, use high attenuation, since this is
	  // the best thing you can do when using DIVERSITY to protect
	  // the second ADC from strong signals from the auxiliary antenna.
          // (ANAN-7000 firmware does this automatically).
	  if (isTransmitting()) {
            output_buffer[C1]=0x3F;
          } else {
	    // if diversity is enabled, use RX1 att value for RX2
            if (diversity_enabled) {
              output_buffer[C1]=0x20 | (adc[0].attenuation & 0x1F);
	    } else {
              output_buffer[C1]=0x20 | (adc[1].attenuation & 0x1F);
	    }
          }
        }
        if(cw_keys_reversed!=0) {
          output_buffer[C2]|=0x40;
        }
        output_buffer[C3]=cw_keyer_speed | (cw_keyer_mode<<6);
        output_buffer[C4]=cw_keyer_weight | (cw_keyer_spacing<<7);
        command=6;
        break;
      case 6:
        // need to add tx attenuation and rx ADC selection
        output_buffer[C0]=0x1C;
        // if n_adc == 1, there is only a single ADC, so we can leave everything
        // set to zero
	if (n_adc  > 1) {
	    // set adc of the two RX associated with the two piHPSDR receivers
	    if (diversity_enabled) {
	      // use ADC0 for RX1 and ADC1 for RX2 (fixed setting)
	      output_buffer[C1]|=0x04;
	    } else {
	      output_buffer[C1]|=(receiver[0]->adc<<(2*rx1channel));
	      output_buffer[C1]|=(receiver[1]->adc<<(2*rx2channel));
	    }
	}
        if (device == DEVICE_HERMES_LITE2) {
          //
          // On the HermesLite2, we need bit7 set to make this feature active,
          // and need bit6 set to tell HL2 to directly use the lowest 6 bits
          // for the built-in preamp. For the effect of choosing different
          // "alex antennas" see above.
          output_buffer[C3] = 0xC0 | (33 - (transmitter->attenuation & 0x1F));
          if (receiver[PS_RX_FEEDBACK]->alex_antenna == 0) {
            output_buffer[C3] = 0xC0 | (45 - (transmitter->attenuation & 0x1F));
          }
        } else {
          output_buffer[C3]=transmitter->attenuation & 0x1F;  // Step attenuator of first ADC, value used when TXing
        }
        command=7;
        break;
      case 7:
        output_buffer[C0]=0x1E;
        if((txmode==modeCWU || txmode==modeCWL) && !tune && cw_keyer_internal && !transmitter->twotone) {
          output_buffer[C1]|=0x01;
        }
        output_buffer[C2]=cw_keyer_sidetone_volume;
        output_buffer[C3]=cw_keyer_ptt_delay;
        command=8;
        break;
      case 8:
        output_buffer[C0]=0x20;
        output_buffer[C1]=(cw_keyer_hang_time>>2) & 0xFF;
        output_buffer[C2]=cw_keyer_hang_time & 0x03;
        output_buffer[C3]=(cw_keyer_sidetone_frequency>>4) & 0xFF;
        output_buffer[C4]=cw_keyer_sidetone_frequency & 0x0F;
        command=9;
        break;
      case 9:
        output_buffer[C0]=0x22;
        output_buffer[C1]=(eer_pwm_min>>2) & 0xFF;
        output_buffer[C2]=eer_pwm_min & 0x03;
        output_buffer[C3]=(eer_pwm_max>>3) & 0xFF;
        output_buffer[C4]=eer_pwm_max & 0x03;
        command=10;
        break;
      case 10:
        //
        // This is possibly only relevant for Orion-II boards
        //
        output_buffer[C0]=0x24;

        if(isTransmitting()) {
          output_buffer[C1]|=0x80; // ground RX2 on transmit, bit0-6 are Alex2 filters
        }
        if(receiver[0]->alex_antenna==5) { // XVTR
          output_buffer[C2] |= 0x02;          // Alex2 XVTR enable
        }
        if(transmitter->puresignal) {
          output_buffer[C2] |= 0x40;	   // Synchronize RX5 and TX frequency on transmit (ANAN-7000)
        }

        //
        // This was the last command defined in the HPSDR document so we
        // roll back to the first command.
        // The HermesLite-II uses an extended command set so in this case
        // we proceed.
        if (device == DEVICE_HERMES_LITE2) {
          command=11;
        } else {
          command=1;
        }
        break;
      case 11:
        // DL1YCF: HermesLite-II only
        // specify some more robust TX latency and PTT hang times
        // A latency of 40 msec means that we first send 1920
        // TX iq samples before HL2 starts TXing. This should be
        // enough to prevent underflows and leave some head-room.
        // My measurements indicate that the TX FIFO can hold about
        // 75 msec or 3600 samples (cum grano salis).
        output_buffer[C0]=0x2E;
        output_buffer[C3]=20;   // 20 msec PTT hang time, only bits 4:0
        output_buffer[C4]=40;   // 40 msec TX latency,    only bits 6:0
        //
        // This was the last command we use out of the extended HL2 command set,
        // so roll back to the first one. It is obvious how to extend this
        // to cover more of the HL2 extended command set.
        //
        command=1;
        break;
    }
  }

  // set mox
  if (isTransmitting()) {
    if(txmode==modeCWU || txmode==modeCWL) {
//
//    For "internal" CW, we should not set
//    the MOX bit, everything is done in the FPGA.
//
//    However, if we are doing CAT CW, local CW or tuning/TwoTone,
//    we must put the SDR into TX mode *here*.
//
      if(tune || CAT_cw_is_active || !cw_keyer_internal || transmitter->twotone) {
        output_buffer[C0]|=0x01;
      }
    } else {
      // not doing CW? always set MOX if transmitting
      output_buffer[C0]|=0x01;
    }
  }

#ifdef USBOZY
//
// if we have a USB interfaced Ozy device:
//
  if (device == DEVICE_OZY)
        ozyusb_write(output_buffer,OZY_BUFFER_SIZE);
  else
#endif
  metis_write(0x02,output_buffer,OZY_BUFFER_SIZE);

  //g_print("C0=%02X C1=%02X C2=%02X C3=%02X C4=%02X\n",
  //                output_buffer[C0],output_buffer[C1],output_buffer[C2],output_buffer[C3],output_buffer[C4]);
}

#ifdef USBOZY
static void ozyusb_write(unsigned char* buffer,int length)
{
  int i;
  i = ozy_write(EP2_OUT_ID,buffer,length);
  if(i!=length) {
    if(i==USB_TIMEOUT) {
      g_print("%s: ozy_write timeout for %d bytes\n",__FUNCTION__,length);
    } else {
      g_print("%s: ozy_write for %d bytes returned %d\n",__FUNCTION__,length,i);
    }
  }

/*

// batch up 4 USB frames (2048 bytes) then do a USB write
  switch(usb_buffer_block++)
  {
    case 0:
    default:
      memcpy(usb_output_buffer, buffer, length);
      break;

    case 1:
      memcpy(usb_output_buffer + 512, buffer, length);
      break;

    case 2:
      memcpy(usb_output_buffer + 1024, buffer, length);
      break;

    case 3:
      memcpy(usb_output_buffer + 1024 + 512, buffer, length);
// and write the 4 usb frames to the usb in one 2k packet
      i = ozy_write(EP2_OUT_ID,usb_output_buffer,EP6_BUFFER_SIZE);

      //dump_buffer(usb_output_buffer,EP6_BUFFER_SIZE,__FUNCTION__);

      //g_print("%s: written %d\n",__FUNCTION__,i);
      //dump_buffer(usb_output_buffer,EP6_BUFFER_SIZE);

      if(i != EP6_BUFFER_SIZE)
      {
	if(i==USB_TIMEOUT) {
	  while(i==USB_TIMEOUT) {
            g_print("%s: USB_TIMEOUT: ozy_write ...\n",__FUNCTION__);
            i = ozy_write(EP2_OUT_ID,usb_output_buffer,EP6_BUFFER_SIZE);
	  }
	  g_print("%s: ozy_write TIMEOUT\n",__FUNCTION__);
        } else {
          perror("old_protocol: OzyWrite ozy failed");
	}
      }

      usb_buffer_block = 0;           // reset counter
      break;
  }
*/
  //
  // DL1YCF:
  // Although the METIS offset is not used for OZY, we have to maintain it
  // since it triggers the "alternating" sending of C0=0 and C0!=0
  // C+C packets in ozy_send_buffer().
  //
  if (metis_offset == 8) {
    metis_offset = 520;
  } else {
    metis_offset = 8;
  }
}
#endif

static int metis_write(unsigned char ep,unsigned const char* buffer,int length) {
  int i;

  // copy the buffer over
  for(i=0;i<512;i++) {
    metis_buffer[i+metis_offset]=buffer[i];
  }

  if(metis_offset==8) {
    metis_offset=520;
  } else {
    metis_buffer[0]=0xEF;
    metis_buffer[1]=0xFE;
    metis_buffer[2]=0x01;
    metis_buffer[3]=ep;
    metis_buffer[4]=(send_sequence>>24)&0xFF;
    metis_buffer[5]=(send_sequence>>16)&0xFF;
    metis_buffer[6]=(send_sequence>>8)&0xFF;
    metis_buffer[7]=(send_sequence)&0xFF;

    send_sequence++;
    metis_send_buffer(&metis_buffer[0],1032);

    metis_offset=8;

  }

  return length;
}

static void metis_restart() {
  int i;

  g_print("%s\n",__FUNCTION__);
  //
  // In TCP-ONLY mode, we possibly need to re-connect
  // since if we come from a METIS-stop, the server
  // has closed the socket. Note that the UDP socket, once
  // opened is never closed.
  //
  if (radio->use_tcp && tcp_socket < 1) open_tcp_socket();

  // reset metis frame
  metis_offset=8;

  // reset current rx
  current_rx=0;

  //
  // When restarting, clear the IQ and audio samples
  //
  for(i=8;i<OZY_BUFFER_SIZE;i++) {
    output_buffer[i]=0;
  }
  //
  // Clear TX IQ ring buffer
  //
  txring_inptr = 0;
  txring_outptr = 0;
  //
  // Some (older) HPSDR apps on the RedPitaya have very small
  // buffers that over-run if too much data is sent
  // to the RedPitaya *before* sending a METIS start packet.
  // We fill the DUC FIFO here with about 500 samples before
  // starting. This also sends some vital C&C data.
  // Note we send 8 OZY buffers, that is, 4 METIS buffers
  // containing 504 samples.
  //
  command=1;
  for (i=0; i<8; i++) {
    ozy_send_buffer();
  }
  usleep(100000);

  // start the data flowing
  // No mutex here, since metis_restart() is mutex protected
#ifdef USBOZY
  if(device!=DEVICE_OZY) {
#endif
    metis_start_stop(1);
    usleep(100000);
#ifdef USBOZY
  }
#endif
}

static void metis_start_stop(int command) {
  int i;
  int tmp;
  unsigned char buffer[1032];

  g_print("%s: %d\n",__FUNCTION__,command);
  //
  // Clear TX IQ ring buffer
  //
  txring_inptr = 0;
  txring_outptr = 0;
#ifdef USBOZY
  if(device!=DEVICE_OZY)
  {
#endif

  buffer[0]=0xEF;
  buffer[1]=0xFE;
  buffer[2]=0x04;	// start/stop command
  buffer[3]=command;	// send EP6 and EP4 data (0x00=stop)

  if (tcp_socket < 0) {
    // use UDP  -- send a short packet
    for(i=4;i<64;i++) {
      buffer[i]=0x00;
    }
    metis_send_buffer(buffer,64);
  } else {
    // use TCP -- send a long packet
    //
    // Stop the sending of TX/audio packets (1032-byte-length) and wait a while
    // Then, send the start/stop buffer with a length of 1032
    //
    usleep(100000);
    for(i=4;i<1032;i++) {
      buffer[i]=0x00;
    }
    metis_send_buffer(buffer,1032);
    //
    // Wait a while before resuming sending TX/audio packets.
    // This prevents mangling of data from TX/audio and Start/Stop packets.
    //
    usleep(100000);
  }
  if (command == 0 && tcp_socket >= 0) {
    // We just have sent a METIS stop in TCP
    // Radio will close the TCP connection, therefore we do this as well
    tmp=tcp_socket;
    tcp_socket=-1;
    usleep(100000);  // give some time to swallow incoming TCP packets
    close(tmp);
    g_print("TCP socket closed\n");
  }

#ifdef USBOZY
  }
#endif
}

static void metis_send_buffer(unsigned char* buffer,int length) {
  int bytes_sent;
  //
  // Send using either the UDP or TCP socket. Do not use TCP for
  // packets that are not 1032 bytes long
  //

  //g_print("%s: length=%d\n",__FUNCTION__,length);

  if (tcp_socket >= 0) {
    if (length != 1032) {
       g_print("PROGRAMMING ERROR: TCP LENGTH != 1032\n");
       exit(-1);
    }
    if(sendto(tcp_socket,buffer,length,0,NULL, 0) != length) {
      perror("sendto socket failed for TCP metis_send_data\n");
    }
  } else if (data_socket >= 0) {
//g_print("%s: sendto %d for %s:%d length=%d\n",__FUNCTION__,data_socket,inet_ntoa(data_addr.sin_addr),ntohs(data_addr.sin_port),length);
    bytes_sent=sendto(data_socket,buffer,length,0,(struct sockaddr*)&data_addr,sizeof(data_addr));
    if(bytes_sent!=length) {
      g_print("%s: UDP sendto failed: %d: %s\n",__FUNCTION__,errno,strerror(errno));
    }
  } else {
    // This should not happen
    g_print("METIS send: neither UDP nor TCP socket available!\n");
    exit(-1);
  }
}


