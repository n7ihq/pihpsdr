/* Copyright (C)
* 2015 - John Melton, G0ORX/N6LYT
*
*   This program is free software: you can redistribute it and/or modify
*   it under the terms of the GNU General Public License as published by
*   the Free Software Foundation, either version 3 of the License, or
*   (at your option) any later version.
*
*   This program is distributed in the hope that it will be useful,
*   but WITHOUT ANY WARRANTY; without even the implied warranty of
*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*   GNU General Public License for more details.
*
*   You should have received a copy of the GNU General Public License
*   along with this program.  If not, see <https://www.gnu.org/licenses/>.
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
#include <signal.h>
#include <wdsp.h>

#include "MacOS.h"
#include "audio.h"
#include "band.h"
#include "discovered.h"
#include "mode.h"
#include "filter.h"
#include "old_protocol.h"
#include "radio.h"
#include "receiver.h"
#include "transmitter.h"
#include "vfo.h"
#include "ext.h"
#include "iambic.h"
#include "message.h"

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
  SYNC_0 = 0,
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
static int state = SYNC_0;

static int data_socket = -1;
static int tcp_socket = -1;
static struct sockaddr_in data_addr;

static unsigned char control_in[5] = {0x00, 0x00, 0x00, 0x00, 0x00};

static volatile int running = 0;

static uint32_t last_seq_num = -0xffffffff;
static int tx_fifo_flag = 0;

static int current_rx = 0;

static int mic_samples = 0;
static int mic_sample_divisor = 1;

static int local_ptt = 0;
static int dash = 0;
static int dot = 0;

static unsigned char output_buffer[OZY_BUFFER_SIZE];

static int command = 1;

static gpointer receive_thread(gpointer arg);
static gpointer process_ozy_input_buffer_thread(gpointer arg);

static void queue_two_ozy_input_buffers(unsigned const char *buf1,
                                        unsigned const char *buf2);
void ozy_send_buffer(void);

static unsigned char metis_buffer[1032];
static uint32_t send_sequence = 0;
static int metis_offset = 8;

static int metis_write(unsigned char ep, unsigned const char* buffer, int length);
static void metis_start_stop(int command);
static void metis_send_buffer(unsigned char* buffer, int length);
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

  static gpointer ozy_ep6_rx_thread(gpointer arg);
  static void start_usb_receive_threads(void);
  static void ozyusb_write(unsigned char* buffer, int length);
  #define EP6_IN_ID   0x86                        // end point = 6, direction toward PC
  #define EP2_OUT_ID  0x02                        // end point = 2, direction from PC
  #define EP6_BUFFER_SIZE 2048
  #define USB_TIMEOUT -7
#endif

static GMutex dump_mutex;

#ifdef __APPLE__
  static sem_t *txring_sem;
  static sem_t *rxring_sem;
#else
  static sem_t txring_sem;
  static sem_t rxring_sem;
#endif
//
// probably not needed
//
static pthread_mutex_t send_audio_mutex   = PTHREAD_MUTEX_INITIALIZER;

//
// This mutex "protects" ozy_send_buffer. This is necessary only for
// TCP and USB-OZY since there the communication is a byte stream.
//
static pthread_mutex_t send_ozy_mutex   = PTHREAD_MUTEX_INITIALIZER;

//
// Ring buffer for outgoing samples.
// Samples going to the radio are produced in big chunks.
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
// a large ring buffer (about 4k samples), and send them to the
// radio following the pace of incoming mic samples. If we decide
// to send a packet to the radio, we must have at least 126 samples
// in the ring buffer and will then send 126 samples (two ozy buffers)
// in one shot.
//
// TXRINGBUFLEN must be a multiple of 1008 bytes (126 samples)
//
#define TXRINGBUFLEN 32256     // 80 msec
static unsigned char *TXRINGBUF = NULL;
static volatile int txring_inptr  = 0;  // pointer updated when writing into the ring buffer
static volatile int txring_outptr = 0;  // pointer updated when reading from the ring buffer
static volatile int txring_flag   = 0;  // 0: RX, 1: TX
static volatile int txring_count  = 0;  // a sample counter
static volatile int txring_drain  = 0;  // a flag for draining the output buffer

//
// If we want to store samples of about 75msec, this
// corresponds to 480 kByte (PS, 5RX, 192k) or
// 400 kByyte (2RX, 384k), so we use 512k
//
#define RXRINGBUFLEN 524288  // must be multiple of 1024 since we queue double-buffers
static unsigned char *RXRINGBUF = NULL;
static volatile int rxring_inptr  = 0;  // pointer updated when writing into the ring buffer
static volatile int rxring_outptr = 0;  // pointer updated when reading from the ring buffer
static volatile int rxring_count  = 0;  // a sample counter

static gpointer old_protocol_txiq_thread(gpointer data) {
  int nptr;

  //
  // Ideally, an output METIS buffer with 126 samples is sent every 2625 usec.
  // We thus wait until we have 126 samples, and then send a packet.
  // After sending the packet, wait 2000 usecs before sending the next one.
  // If "txring_drain" is set, drain the buffer
  //
  for (;;) {
#ifdef __APPLE__
    sem_wait(txring_sem);
#else
    sem_wait(&txring_sem);
#endif
    nptr = txring_outptr + 1008;

    if (nptr >= TXRINGBUFLEN) { nptr = 0; }

    if (!running || txring_drain) {
      txring_outptr = nptr;
      continue;
    }

    if (pthread_mutex_trylock(&send_ozy_mutex)) {
      //
      // This can only happen if the GUI thread initiates
      // a protocol stop/start sequence, as it does e.g.
      // when changing the number of receivers, changing
      // the sample rate, en/dis-abling PureSignal or
      // DIVERSITY, or executing the RESTART button.
      //
      txring_outptr = nptr;
    } else {
      memcpy(output_buffer + 8, &TXRINGBUF[txring_outptr    ], 504);
      ozy_send_buffer();
      memcpy(output_buffer + 8, &TXRINGBUF[txring_outptr + 504], 504);
      ozy_send_buffer();
      MEMORY_BARRIER;
      txring_outptr = nptr;
      usleep(2000);
      pthread_mutex_unlock(&send_ozy_mutex);
    }
  }

  return NULL;
}

// This function is used in debug code
void dump_buffer(unsigned char *buffer, int length, const char *who) {
  g_mutex_lock(&dump_mutex);
  t_print("%s: %s: %d\n", __FUNCTION__, who, length);
  int i = 0;
  int line = 0;

  while (i < length) {
    t_print("%02X", buffer[i]);
    i++;
    line++;

    if (line == 16) {
      t_print("\n");
      line = 0;
    }
  }

  if (line != 0) {
    t_print("\n");
  }

  t_print("\n");
  g_mutex_unlock(&dump_mutex);
}

void old_protocol_stop() {
  //
  // Mutex is needed since in the TCP case, sending TX IQ packets
  // must not occur while the "stop" packet is sent.
  //
  pthread_mutex_lock(&send_ozy_mutex);

  if (device != DEVICE_OZY) {
    t_print("%s\n", __FUNCTION__);
    metis_start_stop(0);
  }

  pthread_mutex_unlock(&send_ozy_mutex);
}

void old_protocol_run() {
  t_print("%s\n", __FUNCTION__);
  pthread_mutex_lock(&send_ozy_mutex);
  metis_restart();
  pthread_mutex_unlock(&send_ozy_mutex);
}

void old_protocol_set_mic_sample_rate(int rate) {
  mic_sample_divisor = rate / 48000;
}

//
// old_protocol_init is only called ONCE.
// old_protocol_stop and old_protocol_run just send start/stop packets
// but do not shut down the communication
//
void old_protocol_init(int rx, int pixels, int rate) {
  int i;
  t_print("old_protocol_init: num_hpsdr_receivers=%d\n", how_many_receivers());

  if (TXRINGBUF == NULL) {
    TXRINGBUF = g_new(unsigned char, TXRINGBUFLEN);
  }

  if (RXRINGBUF == NULL) {
    RXRINGBUF = g_new(unsigned char, RXRINGBUFLEN);
  }

#ifdef __APPLE__
  txring_sem = apple_sem(0);
  rxring_sem = apple_sem(0);
#else
  (void) sem_init(&txring_sem, 0, 0);
  (void) sem_init(&rxring_sem, 0, 0);
#endif
  pthread_mutex_lock(&send_ozy_mutex);
  old_protocol_set_mic_sample_rate(rate);
  g_thread_new("P1 out", old_protocol_txiq_thread, NULL);

  if (transmitter->local_microphone) {
    if (audio_open_input() != 0) {
      t_print("audio_open_input failed\n");
      transmitter->local_microphone = 0;
    }
  }

  g_thread_new("P1 proc", process_ozy_input_buffer_thread, NULL);

  //
  // if we have a USB interfaced Ozy device:
  //
  if (device == DEVICE_OZY) {
#ifdef USBOZY
    t_print("old_protocol_init: initialise ozy on USB\n");
    ozy_initialise();
    start_usb_receive_threads();
#endif
  } else {
    t_print("old_protocol starting receive thread\n");

    if (radio->use_tcp) {
      open_tcp_socket();
    } else  {
      open_udp_socket();
    }

    g_thread_new( "METIS", receive_thread, NULL);
  }

  t_print("old_protocol_init: prime radio\n");

  for (i = 8; i < OZY_BUFFER_SIZE; i++) {
    output_buffer[i] = 0;
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
static void start_usb_receive_threads() {
  t_print("old_protocol starting USB receive thread\n");
  g_thread_new( "OZY", ozy_ep6_rx_thread, NULL);
}

//
// receive thread for USB EP6 (512 byte USB Ozy frames)
// this function loops reading 4 frames at a time through USB
// then processes them one at a time.
//
static gpointer ozy_ep6_rx_thread(gpointer arg) {
  t_print( "old_protocol: USB EP6 receive_thread\n");
  static unsigned char ep6_inbuffer[EP6_BUFFER_SIZE];

  for (;;) {
    int bytes = ozy_read(EP6_IN_ID, ep6_inbuffer, EP6_BUFFER_SIZE); // read a 2K buffer at a time

    //
    // If the protocol has been stopped, just swallow all incoming packets
    //
    if (!running) { continue; }

    //t_print("%s: read %d bytes\n",__FUNCTION__,bytes);
    //dump_buffer(ep6_inbuffer,bytes,__FUNCTION__);

    if (bytes == 0) {
      t_print("old_protocol_ep6_read: ozy_read returned 0 bytes... retrying\n");
      continue;
    } else if (bytes != EP6_BUFFER_SIZE) {
      t_print("old_protocol_ep6_read: OzyBulkRead failed %d bytes\n", bytes);
      t_perror("ozy_read(EP6 read failed");
    } else
      // process the received data normally
    {
      queue_two_ozy_input_buffers(&ep6_inbuffer[   0], &ep6_inbuffer[ 512]);
      queue_two_ozy_input_buffers(&ep6_inbuffer[1024], &ep6_inbuffer[1536]);
    }
  }

  return NULL;  /*NOTREACHED*/
}
#endif

static void open_udp_socket() {
  int tmp;

  if (data_socket >= 0) {
    tmp = data_socket;
    data_socket = -1;
    usleep(100000);
    close(tmp);
  }

  tmp = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP);

  if (tmp < 0) {
    t_perror("old_protocol: create socket failed for data_socket\n");
    exit(-1);
  }

  int optval = 1;
  socklen_t optlen = sizeof(optval);

  if (setsockopt(tmp, SOL_SOCKET, SO_REUSEADDR, &optval, optlen) < 0) {
    t_perror("data_socket: SO_REUSEADDR");
  }

  if (setsockopt(tmp, SOL_SOCKET, SO_REUSEPORT, &optval, optlen) < 0) {
    t_perror("data_socket: SO_REUSEPORT");
  }

  //#ifdef SET_SOCK_BUF_SIZE
  //
  // We need a receive buffer with a decent size, to be able to
  // store several incoming packets if they arrive in a burst.
  // My personal feeling is to let the kernel decide, but other
  // program explicitly specify the buffer sizes. What I  do here
  // is to query the buffer sizes after they have been set.
  // Note in the UDP case one normally does not need a large
  // send buffer because data is sent immediately.
  //
  // UDP RaspPi default values: RCVBUF: 0x34000, SNDBUF: 0x34000
  //            we set them to: RCVBUF: 0x40000, SNDBUF: 0x10000
  // then getsockopt() returns: RCVBUF: 0x68000, SNDBUF: 0x20000
  //
  // UDP MacOS  default values: RCVBUF: 0xC01D0, SNDBUF: 0x02400
  //            we set them to: RCVBUF: 0x40000, SNDBUF: 0x10000
  // then getsockopt() returns: RCVBUF: 0x40000, SNDBUF: 0x10000
  //
  optval = 0x40000;

  if (setsockopt(tmp, SOL_SOCKET, SO_RCVBUF, &optval, optlen) < 0) {
    t_perror("data_socket: set SO_RCVBUF");
  }

  optval = 0x10000;

  if (setsockopt(tmp, SOL_SOCKET, SO_SNDBUF, &optval, optlen) < 0) {
    t_perror("data_socket: set SO_SNDBUF");
  }

  optlen = sizeof(optval);

  if (getsockopt(tmp, SOL_SOCKET, SO_RCVBUF, &optval, &optlen) < 0) {
    t_perror("data_socket: get SO_RCVBUF");
  } else {
    if (optlen == sizeof(optval)) { t_print("UDP Socket RCV buf size=%d\n", optval); }
  }

  optlen = sizeof(optval);

  if (getsockopt(tmp, SOL_SOCKET, SO_SNDBUF, &optval, &optlen) < 0) {
    t_perror("data_socket: get SO_SNDBUF");
  } else {
    if (optlen == sizeof(optval)) { t_print("UDP Socket SND buf size=%d\n", optval); }
  }

  //#endif
#ifdef __APPLE__
  //optval = 0x10;  // IPTOS_LOWDELAY
  optval = 0xb8;  // DSCP EF
  optlen = sizeof(optval);

  if (setsockopt(tmp, IPPROTO_IP, IP_TOS, &optval, optlen) < 0) {
    t_perror("data_socket: IP_TOS");
  }

#endif
  //
  // set a timeout for receive
  // This is necessary because we might already "sit" in an UDP recvfrom() call while
  // instructing the radio to switch to TCP. Then this call has to finish eventually
  // and the next recvfrom() then uses the TCP socket.
  //
  struct timeval tv;
  tv.tv_sec = 0;
  tv.tv_usec = 100000;

  if (setsockopt(tmp, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0) {
    t_perror("data_socket: SO_RCVTIMEO");
  }

  // bind to the interface
  t_print("binding UDP socket to %s:%d\n", inet_ntoa(radio->info.network.interface_address.sin_addr),
          ntohs(radio->info.network.interface_address.sin_port));

  if (bind(tmp, (struct sockaddr * )&radio->info.network.interface_address, radio->info.network.interface_length) < 0) {
    t_perror("old_protocol: bind socket failed for data_socket\n");
    exit(-1);
  }

  memcpy(&data_addr, &radio->info.network.address, radio->info.network.address_length);
  data_addr.sin_port = htons(DATA_PORT);
  //
  // Set value of data_socket only after everything succeeded
  //
  data_socket = tmp;
  t_print("%s: UDP socket established: %d for %s:%d\n", __FUNCTION__, data_socket, inet_ntoa(data_addr.sin_addr),
          ntohs(data_addr.sin_port));
}

static void open_tcp_socket() {
  int tmp;

  if (tcp_socket >= 0) {
    tmp = tcp_socket;
    tcp_socket = -1;
    usleep(100000);
    close(tmp);
  }

  memcpy(&data_addr, &radio->info.network.address, radio->info.network.address_length);
  data_addr.sin_port = htons(DATA_PORT);
  data_addr.sin_family = AF_INET;
  t_print("Trying to open TCP connection to %s\n", inet_ntoa(radio->info.network.address.sin_addr));
  tmp = socket(AF_INET, SOCK_STREAM, 0);

  if (tmp < 0) {
    t_perror("tcp_socket: create socket failed for TCP socket");
    exit(-1);
  }

  int optval = 1;
  socklen_t optlen = sizeof(optval);

  if (setsockopt(tmp, SOL_SOCKET, SO_REUSEADDR, &optval, optlen) < 0) {
    t_perror("tcp_socket: SO_REUSEADDR");
  }

  if (setsockopt(tmp, SOL_SOCKET, SO_REUSEPORT, &optval, optlen) < 0) {
    t_perror("tcp_socket: SO_REUSEPORT");
  }

  if (connect(tmp, (const struct sockaddr *)&data_addr, sizeof(data_addr)) < 0) {
    t_perror("tcp_socket: connect");
  }

#ifdef SET_SOCK_BUF_SIZE
  //
  // We need a receive buffer with a decent size, to be able to
  // store several incoming packets if they arrive in a burst.
  // My personal feeling is to let the kernel decide, but other
  // program explicitly specify the buffer sizes. What I  do here
  // is to query the buffer sizes after they have been set.
  // Note in the UDP case one normally does not need a large
  // send buffer because data is sent immediately.
  //
  // TCP RaspPi default values: RCVBUF: 0x20000, SNDBUF: 0x15400
  //            we set them to: RCVBUF: 0x40000, SNDBUF: 0x10000
  // then getsockopt() returns: RCVBUF: 0x68000, SNDBUF: 0x20000
  //
  // TCP MacOS  default values: RCVBUF: 0x63AEC, SNDBUF: 0x23E2C
  //            we set them to: RCVBUF: 0x40000, SNDBUF: 0x10000
  // then getsockopt() returns: RCVBUF: 0x40000, SNDBUF: 0x10000
  //
  optval = 0x40000;

  if (setsockopt(tmp, SOL_SOCKET, SO_RCVBUF, &optval, optlen) < 0) {
    t_perror("tcp_socket: set SO_RCVBUF");
  }

  optval = 0x10000;

  if (setsockopt(tmp, SOL_SOCKET, SO_SNDBUF, &optval, optlen) < 0) {
    t_perror("tcp_socket: set SO_SNDBUF");
  }

  optlen = sizeof(optval);

  if (getsockopt(tmp, SOL_SOCKET, SO_RCVBUF, &optval, &optlen) < 0) {
    t_perror("tcp_socket: get SO_RCVBUF");
  } else {
    if (optlen == sizeof(optval)) { t_print("TCP Socket RCV buf size=%d\n", optval); }
  }

  optlen = sizeof(optval);

  if (getsockopt(tmp, SOL_SOCKET, SO_SNDBUF, &optval, &optlen) < 0) {
    t_perror("tcp_socket: get SO_SNDBUF");
  } else {
    if (optlen == sizeof(optval)) { t_print("TCP Socket SND buf size=%d\n", optval); }
  }

#endif
#ifdef __APPLE__
  //optval = 0x10;  // IPTOS_LOWDELAY
  optval = 0xb8;  // DSCP EF
  optlen = sizeof(optval);

  if (setsockopt(tmp, IPPROTO_IP, IP_TOS, &optval, optlen) < 0) {
    t_perror("tcp_socket: IP_TOS");
  }

#endif
  //
  // Set value of tcp_socket only after everything succeeded
  //
  tcp_socket = tmp;
  t_print("TCP socket established: %d\n", tcp_socket);
}

static gpointer receive_thread(gpointer arg) {
  struct sockaddr_in addr;
  socklen_t length;
  unsigned char buffer[2048];
  int bytes_read;
  int ret, left;
  int ep;
  uint32_t sequence;
  t_print( "old_protocol: receive_thread\n");
  length = sizeof(addr);

  for (;;) {
    switch (device) {
    case DEVICE_OZY:
      // should not happen
      break;

    default:
      for (;;) {
        if (tcp_socket >= 0) {
          // TCP messages may be split, so collect exactly 1032 bytes.
          // Remember, this is a STREAMING protocol.
          bytes_read = 0;
          left = 1032;

          while (left > 0) {
            ret = recvfrom(tcp_socket, buffer + bytes_read, (size_t)(left), 0, NULL, 0);

            if (ret < 0 && errno == EAGAIN) { continue; } // time-out

            if (ret < 0) { break; }                       // error

            bytes_read += ret;
            left -= ret;
          }

          if (ret < 0) {
            bytes_read = ret;                        // error case: discard whole packet
          }
        } else if (data_socket >= 0) {
          bytes_read = recvfrom(data_socket, buffer, sizeof(buffer), 0, (struct sockaddr*)&addr, &length);

          if (bytes_read < 0 && errno != EAGAIN) { t_perror("old_protocol recvfrom UDP:"); }

          //t_print("%s: bytes_read=%d\n",__FUNCTION__,bytes_read);
        } else {
          //
          // This could happen in METIS start/stop sequences when using TCP
          //
          usleep(100000);
          continue;
        }

        if (bytes_read >= 0 || errno != EAGAIN) { break; }
      }

      //
      // If the protocol has been stopped, just swallow all incoming packets
      //
      if (bytes_read <= 0 || !running) {
        continue;
      }

      if (buffer[0] == 0xEF && buffer[1] == 0xFE) {
        switch (buffer[2]) {
        case 1:
          // get the end point
          ep = buffer[3] & 0xFF;
          // get the sequence number
          sequence = ((buffer[4] & 0xFF) << 24) + ((buffer[5] & 0xFF) << 16) + ((buffer[6] & 0xFF) << 8) + (buffer[7] & 0xFF);

          // A sequence error with a seqnum of zero usually indicates a METIS restart
          // and is no error condition
          if (sequence != 0 && sequence != last_seq_num + 1) {
            t_print("SEQ ERROR: last %ld, recvd %ld\n", (long) last_seq_num, (long) sequence);
            sequence_errors++;
          }

          last_seq_num = sequence;

          switch (ep) {
          case 6: // EP6
            // process the data
            queue_two_ozy_input_buffers(&buffer[8], &buffer[520]);
            break;

          case 4: // EP4
            // not implemented
            break;

          default:
            t_print("unexpected EP %d length=%d\n", ep, bytes_read);
            break;
          }

          break;

        case 2:  // response to a discovery packet
          t_print("unexepected discovery response when not in discovery mode\n");
          break;

        default:
          t_print("unexpected packet type: 0x%02X\n", buffer[2]);
          break;
        }
      } else {
        t_print("received bad header bytes on data port %02X,%02X\n", buffer[0], buffer[1]);
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
  case DEVICE_OZY:
    ret = 0;
    break;

  case DEVICE_HERMES:
    // Note Anan-10E and Anan-100B behave like METIS
    ret = anan10E ? 0 : 2;
    break;

  case DEVICE_STEMLAB:
  case DEVICE_STEMLAB_Z20:
  case DEVICE_HERMES_LITE2:
    ret = 2;
    break;

  case DEVICE_ANGELIA:
  case DEVICE_ORION:
  case DEVICE_ORION2:
    ret = 3;
    break;

  default:
    ret = 0;
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
  case DEVICE_OZY:
    ret = 1;
    break;

  case DEVICE_HERMES:
    // Note Anan-10E and Anan-100B behave like METIS
    ret = anan10E ? 1 : 3;
    break;

  case DEVICE_STEMLAB:
  case DEVICE_STEMLAB_Z20:
  case DEVICE_HERMES_LITE2:
    ret = 3;
    break;

  case DEVICE_ANGELIA:
  case DEVICE_ORION:
  case DEVICE_ORION2:
    ret = 4;
    break;

  default:
    ret = 1;
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
    vfonum = receiver[0]->id;
    break;

  case 1:
    if (diversity_enabled) {
      vfonum = receiver[0]->id;
    } else {
      vfonum = receiver[1]->id;
    }

    break;

  default:   // TX frequency used for all other channels
    vfonum = -1;
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
    vfonum = get_tx_vfo();
    freq = vfo[vfonum].frequency - vfo[vfonum].lo;

    if (vfo[vfonum].ctun) { freq += vfo[vfonum].offset; }

    if (vfo[vfonum].xit_enabled) {
      freq += vfo[vfonum].xit;
    }

    if (!cw_is_on_vfo_freq) {
      if (vfo[vfonum].mode == modeCWU) {
        freq += (long long)cw_keyer_sidetone_frequency;
      } else if (vfo[vfonum].mode == modeCWL) {
        freq -= (long long)cw_keyer_sidetone_frequency;
      }
    }
  } else {
    //
    // determine RX frequency associated with VFO #vfonum
    // This is the center freq in CTUN mode.
    //
    freq = vfo[vfonum].frequency - vfo[vfonum].lo;

    if (vfo[vfonum].rit_enabled) {
      freq += vfo[vfonum].rit;
    }

    if (cw_is_on_vfo_freq) {
      if (vfo[vfonum].mode == modeCWU) {
        freq -= (long long)cw_keyer_sidetone_frequency;
      } else if (vfo[vfonum].mode == modeCWL) {
        freq += (long long)cw_keyer_sidetone_frequency;
      }
    }
  }

  freq += frequency_calibration;
  return freq;
}

static int how_many_receivers() {
  //
  // For DIVERSITY, we need at least two RX channels
  // When PureSignal is active, we need to include the TX DAC channel.
  //
  int ret = receivers;          // 1 or 2

  if (diversity_enabled) { ret = 2; } // need both RX channels, even if there is only one RX

  //
  // Always return 2 so the number of HPSDR-RX is NEVER changed.
  // With 2 RX you can do 1RX or 2RX modes, and it is
  // also OK for doing PureSignal on OZY.
  // Rationale: Rick reported that OZY's tend to hang if the
  //            number of receivers is changed while running.
  // OK it wastes bandwidth and this might be a problem at higher
  //    sample rates but this is simply safer.
  //
  if (device == DEVICE_OZY) { return 2; }

  // for PureSignal, the number of receivers needed is hard-coded below.
  // we need at least 2, and up to 5 for Orion2 boards. This is so because
  // the TX DAC is hard-wired to RX for limited-capacity FPGAS, to
  // RX4 for HERMES, STEMLAB and to RX5 for ANGELIA
  // and beyond.
  if (transmitter->puresignal) {
    switch (device) {
    case DEVICE_METIS:
    case DEVICE_HERMES_LITE:
    case DEVICE_OZY:
      ret = 2; // TX feedback hard-wired to RX2
      break;

    case DEVICE_HERMES:
      // Note Anan-10E and Anan-100B behave like METIS
      ret = anan10E ? 2 : 4;
      break;

    case DEVICE_STEMLAB:
    case DEVICE_STEMLAB_Z20:
    case DEVICE_HERMES_LITE2:
      ret = 4; // TX feedback hard-wired to RX4
      break;

    case DEVICE_ANGELIA:
    case DEVICE_ORION:
    case DEVICE_ORION2:
      ret = 5; // TX feedback hard-wired to RX5
      break;

    default:
      ret = 2; // This is the minimum for PureSignal
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
  previous_ptt = local_ptt;
  previous_dot = dot;
  previous_dash = dash;
  local_ptt = (control_in[0] & 0x01) == 0x01;
  dash = (control_in[0] & 0x02) == 0x02;
  dot = (control_in[0] & 0x04) == 0x04;

  // Stops CAT cw transmission if radio reports "CW action"
  if (dash || dot) {
    cw_key_hit = 1;
    CAT_cw_is_active = 0;
  }

  if (!cw_keyer_internal) {
    if (dash != previous_dash) { keyer_event(0, dash); }

    if (dot  != previous_dot ) { keyer_event(1, dot ); }
  }

  if (previous_ptt != local_ptt) {
    g_idle_add(ext_mox_update, (gpointer)(long)(local_ptt));
  }

  switch ((control_in[0] >> 3) & 0x1F) {
  case 0:
    adc_overload = control_in[1] & 0x01;

    if (device != DEVICE_HERMES_LITE2) {
      //
      // HL2 uses these bits of the protocol for a different purpose:
      // C1 unused except the ADC overload bit
      // C2/C3 contains underflow/overflow and TX FIFO count
      //
      IO1 = (control_in[1] & 0x02) ? 0 : 1;
      IO2 = (control_in[1] & 0x04) ? 0 : 1;
      IO3 = (control_in[1] & 0x08) ? 0 : 1;

      if (mercury_software_version != control_in[2]) {
        mercury_software_version = control_in[2];
        t_print("  Mercury Software version: %d (0x%0X)\n", mercury_software_version, mercury_software_version);
      }

      if (penelope_software_version != control_in[3]) {
        penelope_software_version = control_in[3];
        t_print("  Penelope Software version: %d (0x%0X)\n", penelope_software_version, penelope_software_version);
      }
    }

    //
    // HermesLite-II TX-FIFO overflow/underrun detection.
    //
    // Measured on HL2 software version 7.2:
    // multiply FIFO value with 32 to get sample count
    // multiply FIFO value with 0.67 to get FIFO length in milli-seconds
    // Overflow at about 3600 samples (75 msec).
    //
    // As a result, we set the "TX latency" to 40 msec (see below).
    //
    // Note after an RX/TX transition, "underflow" is reported
    // until the TX fifo begins to fill, so we ignore these underflows
    // until the first packet reporting "no underflow" after each
    // RX/TX transition.
    //
    if (device == DEVICE_HERMES_LITE2) {
      if (!isTransmitting()) {
        // during RX: set flag to zero
        tx_fifo_flag = 0;
        tx_fifo_underrun = 0;
      } else {
        // after RX/TX transition: ignore underflow condition
        // until it first vanishes. tx_fifo_flag becomes "true"
        // as soon as a "no underflow" condition is seen.
        //
        if ((control_in[3] & 0xC0) != 0x80) { tx_fifo_flag = 1; }

        if ((control_in[3] & 0xC0) == 0x80 && tx_fifo_flag) { tx_fifo_underrun = 1; }

        if ((control_in[3] & 0xC0) == 0xC0) { tx_fifo_overrun = 1; }
      }
    }

    if (ozy_software_version != control_in[4]) {
      ozy_software_version = control_in[4];
      t_print("FPGA firmware version: %d.%d\n", ozy_software_version / 10, ozy_software_version % 10);
    }

    break;

  case 1:
    if (device != DEVICE_HERMES_LITE2) {
      //
      // HL2 uses C1/C2 for measuring the temperature
      //
      exciter_power = ((control_in[1] & 0xFF) << 8) | (control_in[2] & 0xFF); // from Penelope or Hermes
    } else {
      exciter_power = 0;
      unsigned int temperature = ((control_in[1] & 0xFF) << 8) | (control_in[2] & 0xFF); // HL2
      // moving average
      average_temperature = (temperature + 7 * average_temperature) >> 3;
    }

    //
    //  calculate moving averages of fwd and rev voltages to have a correct SWR
    //  at the edges of an RF pulse. Otherwise a false trigger of the SWR
    //  alarm may occur.
    //
    alex_forward_power = ((control_in[3] & 0xFF) << 8) | (control_in[4] & 0xFF); // from Alex or Apollo
    alex_forward_power_average = (alex_forward_power + 3 * alex_forward_power_average) >> 2;
    break;

  case 2:
    alex_reverse_power = ((control_in[1] & 0xFF) << 8) | (control_in[2] & 0xFF); // from Alex or Apollo
    alex_reverse_power_average = (alex_reverse_power + 3 * alex_reverse_power_average) >> 2;

    if (device != DEVICE_HERMES_LITE2) {
      AIN3 = ((control_in[3] & 0xFF) << 8) | (control_in[4] & 0xFF); // For Penelope or Hermes
    } else {
      unsigned int current = ((control_in[3] & 0xFF) << 8) | (control_in[4] & 0xFF); // HL2
      // moving average
      average_current = (3 * average_current + current) >> 2;
    }

    break;

  case 3:
    AIN4 = ((control_in[1] & 0xFF) << 8) | (control_in[2] & 0xFF); // For Penelope or Hermes
    AIN6 = ((control_in[3] & 0xFF) << 8) | (control_in[4] & 0xFF); // For Penelope or Hermes
    break;
  }
}

//
// These static variables are set at the beginning
// of process_ozy_input_buffer() and "do" the communication
// with process_ozy_byte()
//
static int st_num_hpsdr_receivers;
static int st_rxfdbk;
static int st_txfdbk;
static int st_rx1channel;
static int st_rx2channel;

static void process_ozy_byte(int b) {
  switch (state) {
  case SYNC_0:
    if (b == SYNC) {
      state++;
    }

    break;

  case SYNC_1:
    if (b == SYNC) {
      state++;
    }

    break;

  case SYNC_2:
    if (b == SYNC) {
      state++;
    }

    break;

  case CONTROL_0:
    control_in[0] = b;
    state++;
    break;

  case CONTROL_1:
    control_in[1] = b;
    state++;
    break;

  case CONTROL_2:
    control_in[2] = b;
    state++;
    break;

  case CONTROL_3:
    control_in[3] = b;
    state++;
    break;

  case CONTROL_4:
    control_in[4] = b;
    process_control_bytes();
    nreceiver = 0;
    iq_samples = (512 - 8) / ((st_num_hpsdr_receivers * 6) + 2);
    nsamples = 0;
    state++;
    break;

  case LEFT_SAMPLE_HI:
    left_sample = (int)((signed char)b << 16);
    state++;
    break;

  case LEFT_SAMPLE_MID:
    left_sample |= (int)((((unsigned char)b) << 8) & 0xFF00);
    state++;
    break;

  case LEFT_SAMPLE_LOW:
    left_sample |= (int)((unsigned char)b & 0xFF);
    left_sample_double = (double)left_sample * 1.1920928955078125E-7;
    state++;
    break;

  case RIGHT_SAMPLE_HI:
    right_sample = (int)((signed char)b << 16);
    state++;
    break;

  case RIGHT_SAMPLE_MID:
    right_sample |= (int)((((unsigned char)b) << 8) & 0xFF00);
    state++;
    break;

  case RIGHT_SAMPLE_LOW:
    right_sample |= (int)((unsigned char)b & 0xFF);
    right_sample_double = (double)right_sample * 1.1920928955078125E-7;

    if (isTransmitting() && transmitter->puresignal) {
      //
      // transmitting with PureSignal. Get sample pairs and feed to pscc
      //
      if (nreceiver == st_rxfdbk) {
        left_sample_double_rx = left_sample_double;
        right_sample_double_rx = right_sample_double;
      } else if (nreceiver == st_txfdbk) {
        left_sample_double_tx = left_sample_double;
        right_sample_double_tx = right_sample_double;
      }

      // this is pure paranoia, it allows for st_txfdbk < st_rxfdbk
      if (nreceiver + 1 == st_num_hpsdr_receivers) {
        add_ps_iq_samples(transmitter, left_sample_double_tx, right_sample_double_tx, left_sample_double_rx,
                          right_sample_double_rx);
      }
    }

    if (!isTransmitting() && diversity_enabled) {
      //
      // receiving with DIVERSITY. Get sample pairs and feed to diversity mixer
      //
      if (nreceiver == st_rx1channel) {
        left_sample_double_main = left_sample_double;
        right_sample_double_main = right_sample_double;
      } else if (nreceiver == st_rx2channel) {
        left_sample_double_aux = left_sample_double;
        right_sample_double_aux = right_sample_double;
      }

      // this is pure paranoia, it allows for st_rx2channel < st_rx1channel
      if (nreceiver + 1 == st_num_hpsdr_receivers) {
        add_div_iq_samples(receiver[0], left_sample_double_main, right_sample_double_main, left_sample_double_aux,
                           right_sample_double_aux);

        // if we have a second receiver, display "auxiliary" receiver as well
        if (receivers > 1) { add_iq_samples(receiver[1], left_sample_double_aux, right_sample_double_aux); }
      }
    }

    if ((!isTransmitting() || duplex) && !diversity_enabled) {
      //
      // RX without DIVERSITY. Feed samples to RX1 and RX2
      //
      if (nreceiver == st_rx1channel) {
        add_iq_samples(receiver[0], left_sample_double, right_sample_double);
      } else if (nreceiver == st_rx2channel && receivers > 1) {
        add_iq_samples(receiver[1], left_sample_double, right_sample_double);
      }
    }

    nreceiver++;

    if (nreceiver == st_num_hpsdr_receivers) {
      state++;
    } else {
      state = LEFT_SAMPLE_HI;
    }

    break;

  case MIC_SAMPLE_HI:
    mic_sample = (short)(b << 8);
    state++;
    break;

  case MIC_SAMPLE_LOW:
    mic_sample |= (short)(b & 0xFF);
    mic_samples++;

    if (mic_samples >= mic_sample_divisor) { // reduce to 48000
      //
      // if local_ptt is set, this usually means the PTT at the microphone connected
      // to the SDR is pressed. In this case, we take audio from BOTH sources
      // then we can use a "voice keyer" on some loop-back interface but at the same
      // time use our microphone.
      // In most situations only one source will be active so we just add.
      //
      float fsample;

      if (local_ptt) {
        fsample = (float) mic_sample * 0.00003051;

        if (transmitter->local_microphone) { fsample += audio_get_next_mic_sample(); }
      } else {
        fsample = transmitter->local_microphone ? audio_get_next_mic_sample() : (float) mic_sample * 0.00003051;
      }

      add_mic_sample(transmitter, fsample);
      mic_samples = 0;
    }

    nsamples++;

    if (nsamples == iq_samples) {
      state = SYNC_0;
    } else {
      nreceiver = 0;
      state = LEFT_SAMPLE_HI;
    }

    break;
  }
}

static void queue_two_ozy_input_buffers(unsigned const char *buf1,
                                        unsigned const char *buf2) {
  //
  // To achieve minimum overhead in the RX thread, the data is
  // simply put into a large ring buffer. We queue two buffers
  // in one shot since this halves the number of semamphore operations
  // at no cost (buffer fly in in pairs anyway)
  //
  if (rxring_count < 0) {
    rxring_count++;
    return;
  }

  int nptr = rxring_inptr + 1024;

  if (nptr >= RXRINGBUFLEN) { nptr = 0; }

  if (nptr != rxring_outptr) {
    memcpy((void *)(&RXRINGBUF[rxring_inptr    ]), buf1, 512);
    memcpy((void *)(&RXRINGBUF[rxring_inptr + 512]), buf2, 512);
    MEMORY_BARRIER;
    rxring_inptr = nptr;
#ifdef __APPLE__
    sem_post(rxring_sem);
#else
    sem_post(&rxring_sem);
#endif
  } else {
    t_print("%s: input buffer overflow.\n", __FUNCTION__);
    // if an overflow is encountered, skip the next 256 input buffers
    // to allow a "fresh start"
    rxring_count = -256;
  }
}

static gpointer process_ozy_input_buffer_thread(gpointer arg) {
  //
  // This thread constantly monitors the input ring buffer and
  // processes the data whenever a bunch is available. Note this
  // thread does all the fexchange() with WDSP, since it calls
  // (via process_ozy_byte)
  //
  // add_iq_samples   ==> RX engine(s)
  // add_mic_sample   ==> TX engine
  //
  for (;;) {
#ifdef __APPLE__
    sem_wait(rxring_sem);
#else
    sem_wait(&rxring_sem);
#endif
    int nptr = rxring_outptr + 1024;

    if (nptr >= RXRINGBUFLEN) { nptr = 0; }

    //
    // This data can change while processing one buffer
    //
    st_num_hpsdr_receivers = how_many_receivers();
    st_rxfdbk = rx_feedback_channel();
    st_txfdbk = tx_feedback_channel();
    st_rx1channel = first_receiver_channel();
    st_rx2channel = second_receiver_channel();

    for (int i = 0; i < 1024; i++) {
      process_ozy_byte(RXRINGBUF[rxring_outptr + i] & 0xFF);
    }

    MEMORY_BARRIER;
    rxring_outptr = nptr;
  }

  return NULL;
}

void old_protocol_audio_samples(RECEIVER *rx, short left_audio_sample, short right_audio_sample) {
  if (!isTransmitting()) {
    pthread_mutex_lock(&send_audio_mutex);

    if (txring_count < 0) {
      txring_count++;
      pthread_mutex_unlock(&send_audio_mutex);
      return;
    }

    if (txring_flag) {
      //
      // First time we arrive here after a TX->RX transition:
      // set the "drain" flag, wait 10 msec, clear it
      // This should drain the txiq ring buffer
      //
      txring_drain = 1;
      usleep(10000);
      txring_drain = 0;
      txring_flag = 0;
    }

    int iptr = txring_inptr + 8 * txring_count;

    //
    // The HL2 makes no use of audio samples, but instead
    // uses them to write to extended addrs which we do not
    // want to do un-intentionally, therefore send zeros.
    // Note special variants of the HL2 *do* have an audio codec!
    //
    if (device == DEVICE_HERMES_LITE2 && !hl2_audio_codec) {
      TXRINGBUF[iptr++] = 0;
      TXRINGBUF[iptr++] = 0;
      TXRINGBUF[iptr++] = 0;
      TXRINGBUF[iptr++] = 0;
    } else {
      TXRINGBUF[iptr++] = left_audio_sample >> 8;
      TXRINGBUF[iptr++] = left_audio_sample;
      TXRINGBUF[iptr++] = right_audio_sample >> 8;
      TXRINGBUF[iptr++] = right_audio_sample;
    }

    TXRINGBUF[iptr++] = 0;
    TXRINGBUF[iptr++] = 0;
    TXRINGBUF[iptr++] = 0;
    TXRINGBUF[iptr++] = 0;
    txring_count++;

    if (txring_count >= 126) {
      int nptr = txring_inptr + 1008;

      if (nptr >= TXRINGBUFLEN) { nptr = 0; }

      if (nptr != txring_outptr) {
#ifdef __APPLE__
        sem_post(txring_sem);
#else
        sem_post(&txring_sem);
#endif
        txring_inptr = nptr;
        txring_count = 0;
      } else {
        t_print("%s: output buffer overflow.\n", __FUNCTION__);
        txring_count = -1260;
      }
    }

    pthread_mutex_unlock(&send_audio_mutex);
  }
}

void old_protocol_iq_samples(int isample, int qsample, int side) {
  if (isTransmitting()) {
    pthread_mutex_lock(&send_audio_mutex);

    if (txring_count < 0) {
      txring_count++;
      pthread_mutex_unlock(&send_audio_mutex);
      return;
    }

    if (!txring_flag) {
      //
      // First time we arrive here after a RX->TX transition:
      // set the "drain" flag, wait 10 msec, clear it
      // This should drain the txiq ring buffer (which also
      // contains the audio samples) for minimum CW side tone latency.
      //
      txring_drain = 1;
      usleep(10000);
      txring_drain = 0;
      txring_flag = 1;
    }

    int iptr = txring_inptr + 8 * txring_count;

    //
    // The HL2 makes no use of audio samples, but instead
    // uses them to write to extended addrs which we do not
    // want to do un-intentionally, therefore send zeros.
    // Note special variants of the HL2 *do* have an audio codec!
    //
    if (device == DEVICE_HERMES_LITE2 && !hl2_audio_codec) {
      TXRINGBUF[iptr++] = 0;
      TXRINGBUF[iptr++] = 0;
      TXRINGBUF[iptr++] = 0;
      TXRINGBUF[iptr++] = 0;
    } else {
      TXRINGBUF[iptr++] = side  >> 8;
      TXRINGBUF[iptr++] = side;
      TXRINGBUF[iptr++] = side >> 8;
      TXRINGBUF[iptr++] = side;
    }

    TXRINGBUF[iptr++] = isample >> 8;
    TXRINGBUF[iptr++] = isample;
    TXRINGBUF[iptr++] = qsample >> 8;
    TXRINGBUF[iptr++] = qsample;
    txring_count++;

    if (txring_count >= 126) {
      int nptr = txring_inptr + 1008;

      if (nptr >= TXRINGBUFLEN) { nptr = 0; }

      if (nptr != txring_outptr) {
#ifdef __APPLE__
        sem_post(txring_sem);
#else
        sem_post(&txring_sem);
#endif
        txring_inptr = nptr;
        txring_count = 0;
      } else {
        t_print("%s: output buffer overflow.\n", __FUNCTION__);
        txring_count = -1260;
      }
    }

    pthread_mutex_unlock(&send_audio_mutex);
  }
}

void ozy_send_buffer() {
  int txmode = get_tx_mode();
  int txvfo = get_tx_vfo();
  int rxvfo = active_receiver->id;
  int i;
  const BAND *rxband = band_get_band(vfo[rxvfo].band);
  const BAND *txband = band_get_band(vfo[txvfo].band);
  int power;
  int num_hpsdr_receivers = how_many_receivers();
  int rx1channel = first_receiver_channel();
  int rx2channel = second_receiver_channel();
  int rxfdbkchan = rx_feedback_channel();
  output_buffer[SYNC0] = SYNC;
  output_buffer[SYNC1] = SYNC;
  output_buffer[SYNC2] = SYNC;

  if (metis_offset == 8) {
    //
    // Every second packet is a "C0=0" packet
    // (for JANUS, *every* packet is a "C0=0" packet
    //
    output_buffer[C0] = 0x00;
    output_buffer[C1] = 0x00;

    switch (active_receiver->sample_rate) {
    case 48000:
      output_buffer[C1] |= SPEED_48K;
      break;

    case 96000:
      output_buffer[C1] |= SPEED_96K;
      break;

    case 192000:
      output_buffer[C1] |= SPEED_192K;
      break;

    case 384000:
      output_buffer[C1] |= SPEED_384K;
      break;
    }

    // set more bits for Atlas based device
    // CONFIG_BOTH seems to be critical to getting ozy to respond
    if ((device == DEVICE_OZY) || (device == DEVICE_METIS)) {
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
        output_buffer[C1] |= MERCURY_122_88MHZ_SOURCE;  // Mercury provides 122 Mhz
      } else {
        output_buffer[C1] |= PENELOPE_122_88MHZ_SOURCE; // Penelope provides 122 Mhz
        output_buffer[C1] |= CONFIG_PENELOPE;
      }

      switch (atlas_clock_source_10mhz) {
      case 0:
        output_buffer[C1] |= ATLAS_10MHZ_SOURCE;      // ATLAS provides 10 Mhz
        break;

      case 1:
        output_buffer[C1] |= PENELOPE_10MHZ_SOURCE;   // Penelope provides 10 Mhz
        output_buffer[C1] |= CONFIG_PENELOPE;
        break;

      case 2:
        output_buffer[C1] |= MERCURY_10MHZ_SOURCE;    // Mercury provides 10 MHz
        break;
      }
    }

#ifdef USBOZY

    //
    // This is for "Janus only" operation
    //
    if (device == DEVICE_OZY && atlas_janus) {
      output_buffer[C2] = 0x00;
      output_buffer[C3] = 0x00;
      output_buffer[C4] = 0x00;
      ozyusb_write(output_buffer, OZY_BUFFER_SIZE);
      metis_offset = 8; // take care next packet is a C0=0 packet
      return;
    }

#endif
    output_buffer[C2] = 0x00;

    if (classE) {
      output_buffer[C2] |= 0x01;
    }

    if (isTransmitting()) {
      output_buffer[C2] |= txband->OCtx << 1;

      if (tune) {
        if (OCmemory_tune_time != 0) {
          struct timeval te;
          gettimeofday(&te, NULL);
          long long now = te.tv_sec * 1000LL + te.tv_usec / 1000;

          if (tune_timeout > now) {
            output_buffer[C2] |= OCtune << 1;
          }
        } else {
          output_buffer[C2] |= OCtune << 1;
        }
      }
    } else {
      output_buffer[C2] |= rxband->OCrx << 1;
    }

    output_buffer[C3] = (receiver[0]->alex_attenuation) & 0x03;  // do not set higher bits

    if (active_receiver->random) {
      output_buffer[C3] |= LT2208_RANDOM_ON;
    }

    if (active_receiver->dither) {
      output_buffer[C3] |= LT2208_DITHER_ON;
    }

    //
    // Some  HL2 firmware variants (ab-) uses this bit for indicating an audio codec is present
    // We also  accept explicit use  of the "dither" box
    //
    if (device == DEVICE_HERMES_LITE2 && hl2_audio_codec) {
      output_buffer[C3] |= LT2208_DITHER_ON;
    }

    if (filter_board == CHARLY25 && active_receiver->preamp) {
      output_buffer[C3] |= LT2208_GAIN_ON;
    }

    //
    // Set ALEX RX1_ANT and RX1_OUT
    //
    i = receiver[0]->alex_antenna;

    //
    // Upon TX, we might have to activate a different RX path for the
    // attenuated feedback signal. Use alex_antenna == 0, if
    // the feedback signal is routed automatically/internally
    // If feedback is to the second ADC, leave RX1 ANT settings untouched
    //
    if (isTransmitting() && transmitter->puresignal) { i = receiver[PS_RX_FEEDBACK]->alex_antenna; }

    if (device == DEVICE_ORION2) {
      i += 100;
    } else if (new_pa_board) {
      // New-PA setting invalid on ANAN-7000,8000
      i += 1000;
    }

    //
    // There are several combination which do not exist (no jacket present)
    // or which do not work (using EXT1-on-TX with ANAN-7000).
    // In these cases, fall back to a "reasonable" case (e.g. use EXT1 if
    // there is no EXT2).
    // As a result, the "New PA board" setting is overriden for PureSignal
    // feedback: EXT1 assumes old PA board and ByPass assumes new PA board.
    //
    switch (i) {
    case 3:           // EXT1 with old pa board
    case 6:           // EXT1-on-TX: assume old pa board
    case 1006:
      output_buffer[C3] |= 0xC0;
      break;

    case 4:           // EXT2 with old pa board
      output_buffer[C3] |= 0xA0;
      break;

    case 5:           // XVTR with old pa board
      output_buffer[C3] |= 0xE0;
      break;

    case 104:         // EXT2 with ANAN-7000: does not exist, use EXT1
    case 103:         // EXT1 with ANAN-7000
      output_buffer[C3] |= 0x40;
      break;

    case 105:         // XVTR with ANAN-7000
      output_buffer[C3] |= 0x60;
      break;

    case 106:         // EXT1-on-TX with ANAN-7000: does not exist, use ByPass
    case 107:         // Bypass-on-TX with ANAN-7000
      output_buffer[C3] |= 0x20;
      break;

    case 1003:        // EXT1 with new PA board
      output_buffer[C3] |= 0x40;
      break;

    case 1004:        // EXT2 with new PA board
      output_buffer[C3] |= 0x20;
      break;

    case 1005:        // XVRT with new PA board
      output_buffer[C3] |= 0x60;
      break;

    case 7:           // Bypass-on-TX: assume new PA board
    case 1007:
      output_buffer[C3] |= 0x80;
      break;
    }

    //
    // ALWAYS set the duplex bit "on". This bit indicates to the
    // FPGA that the TX frequency can be different from the RX
    // frequency, which is the case with Split, XIT, CTUN
    //
    output_buffer[C4] = 0x04;

    //
    // This is used to phase-synchronize RX1 and RX2 on some boards
    // and enforces that the RX1 and RX2 frequencies are the same.
    //
    if (diversity_enabled) { output_buffer[C4] |= 0x80; }

    // 0 ... 7 maps on 1 ... 8 receivers
    output_buffer[C4] |= (num_hpsdr_receivers - 1) << 3;

    //
    //  Now we set the bits for Ant1/2/3 (RX and TX may be different)
    //  ATTENTION:
    //  When doing CW handled in radio, the radio may start TXing
    //  before piHPSDR has slewn down the receivers, slewn up the
    //  transmitter and goes TX. Then, if different Ant1/2/3
    //  antennas are chosen for RX and TX, parts of the first
    //  RF dot may arrive at the RX antenna and do bad things
    //  there. While we cannot exclude this completely, we will
    //  switch the Ant1/2/3 selection to TX as soon as we see
    //  a PTT signal from the radio.
    //  Measurements have shown that we can reduce the time
    //  from when the radio send PTT to the time when the
    //  radio receives the new Ant1/2/2 setup from about
    //  40 (2 RX active) or 20 (1 RX active) to 4 milli seconds,
    // and this should be
    //  enough.
    //
    if (isTransmitting() || local_ptt) {
      i = transmitter->alex_antenna;

      //
      // TX antenna outside allowd range: this cannot happen.
      // Out of paranoia: print warning and choose ANT1
      //
      if (i < 0 || i > 2) {
        t_print("WARNING: illegal TX antenna chosen, using ANT1\n");
        transmitter->alex_antenna = 0;
        i = 0;
      }
    } else {
      i = receiver[0]->alex_antenna;

      //
      // Not using ANT1,2,3: can leave relais in TX state unless using new PA board
      //
      if (i > 2 && !new_pa_board) { i = transmitter->alex_antenna; }
    }

    switch (i) {
    case 0:  // ANT 1
      output_buffer[C4] |= 0x00;
      break;

    case 1:  // ANT 2
      output_buffer[C4] |= 0x01;
      break;

    case 2:  // ANT 3
      output_buffer[C4] |= 0x02;
      break;

    default:
      // this happens only with the new pa board and using EXT1/EXT2/XVTR
      // here we have to disconnect ANT1,2,3
      output_buffer[C4] |= 0x03;
      break;
    }

    // end of "C0=0" packet
  } else {
    //
    // metis_offset !=8: send the other C&C packets in round-robin
    // RX frequency commands are repeated for each RX
    output_buffer[C1] = 0x00;
    output_buffer[C2] = 0x00;
    output_buffer[C3] = 0x00;
    output_buffer[C4] = 0x00;

    switch (command) {
    case 1: { // tx frequency
      output_buffer[C0] = 0x02;
      long long txFrequency = channel_freq(-1);
      output_buffer[C1] = txFrequency >> 24;
      output_buffer[C2] = txFrequency >> 16;
      output_buffer[C3] = txFrequency >> 8;
      output_buffer[C4] = txFrequency;
      command = 2;
    }
    break;

    case 2: // rx frequency
      if (current_rx < num_hpsdr_receivers) {
        output_buffer[C0] = 0x04 + (current_rx * 2);
        long long rxFrequency = channel_freq(current_rx);
        output_buffer[C1] = rxFrequency >> 24;
        output_buffer[C2] = rxFrequency >> 16;
        output_buffer[C3] = rxFrequency >> 8;
        output_buffer[C4] = rxFrequency;
        current_rx++;
      }

      // if we have reached the last RX channel, wrap around
      // and proceed with the next "command"
      if (current_rx >= num_hpsdr_receivers) {
        current_rx = 0;
        command = 3;
      }

      break;

    case 3:
      power = 0;

      //
      // Some HPSDR apps for the RedPitaya generate CW inside the FPGA, but while
      // doing this, DriveLevel changes are processed by the server, but do not become effective.
      // If the CW paddle is hit, the new PTT state is sent to piHPSDR, then the TX drive
      // is sent the next time "command 3" is performed, but this often is too late and
      // CW is generated with zero DriveLevel.
      // Therefore, when in CW mode, send the TX drive level also when receiving.
      // (it would be sufficient to do so only with internal CW).
      //
      if (isTransmitting() || (txmode == modeCWU) || (txmode == modeCWL)) {
        power = transmitter->drive_level;
      }

      output_buffer[C0] = 0x12;
      output_buffer[C1] = power & 0xFF;

      if (mic_boost) {
        output_buffer[C2] |= 0x01;
      }

      if (mic_linein) {
        output_buffer[C2] |= 0x02;
      }

      if (filter_board == APOLLO) {
        output_buffer[C2] |= 0x2C;
      }

      if ((filter_board == APOLLO) && tune) {
        output_buffer[C2] |= 0x10;
      }

      if (band_get_current() == band6) {
        output_buffer[C3] = output_buffer[C3] | 0x40; // Alex 6M low noise amplifier
      }

      if (txband->disablePA || !pa_enabled) {
        output_buffer[C3] |= 0x80; // disable Alex T/R relay

        if (isTransmitting()) {
          output_buffer[C2] |= 0x40; // Manual Filter Selection
          output_buffer[C3] |= 0x20; // bypass all RX filters
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

        if (txFrequency > 35600000L) {            // > 10m so use 6m LPF
          output_buffer[C4] = 0x10;
        } else if (txFrequency > 24000000L)  {    // > 15m so use 10/12m LPF
          output_buffer[C4] = 0x20;
        } else if (txFrequency > 16500000L) {             // > 20m so use 17/15m LPF
          output_buffer[C4] = 0x40;
        } else if (txFrequency >  8000000L) {             // > 40m so use 30/20m LPF
          output_buffer[C4] = 0x01;
        } else if (txFrequency >  5000000L) {             // > 80m so use 60/40m LPF
          output_buffer[C4] = 0x02;
        } else if (txFrequency >  2500000L) {             // > 160m so use 80m LPF
          output_buffer[C4] = 0x04;
        } else {                                  // < 2.5 MHz use 160m LPF
          output_buffer[C4] = 0x08;
        }
      }

      if (device == DEVICE_HERMES_LITE2) {
        // do not set any Apollo/Alex bits (ADDR=0x09 bits 0:23)
        // ADDR=0x09 bit 19 follows "PA enable" state
        // ADDR=0x09 bit 20 follows "TUNE" state
        // ADDR=0x09 bit 18 always cleared (external tuner enabled)
        output_buffer[C2] = 0x00;
        output_buffer[C3] = 0x00;
        output_buffer[C4] = 0x00;

        if (pa_enabled && !txband->disablePA) { output_buffer[C2] |= 0x08; }

        if (tune) { output_buffer[C2] |= 0x10; }
      }

      command = 4;
      break;

    case 4:
      output_buffer[C0] = 0x14;

      if (have_preamp) {
        for (i = 0; i < receivers; i++) {
          output_buffer[C1] |= (receiver[i]->preamp << i);
        }
      }

      if (mic_ptt_enabled == 0) {
        output_buffer[C1] |= 0x40;
      }

      if (mic_bias_enabled) {
        output_buffer[C1] |= 0x20;
      }

      if (mic_ptt_tip_bias_ring) {
        output_buffer[C1] |= 0x10;
      }

      // map input value -34 ... +12 onto 0 ... 31
      output_buffer[C2] |=  (int)((linein_gain + 34.0) * 0.6739 + 0.5);

      if (transmitter->puresignal) {
        output_buffer[C2] |= 0x40;
      }

      // upon TX, use transmitter->attenuation
      // Usually the firmware takes care of this, but it is no
      // harm to do this here as well
      if (device == DEVICE_HERMES_LITE2) {
        //
        // HERMESlite has a RXgain value in the range 0-60 that
        // is stored in rx_gain_slider. The firmware uses bit 6
        // of C4 to allow using the full range in bits 0-5.
        //
        int rxgain = adc[active_receiver->adc].gain + 12; // -12..48 to 0..60

        if (isTransmitting()) {
          //
          // If have_rx_gain, the "TX attenuation range" is extended from
          // -29 to +31 which is then mapped to 60 ... 0
          //
          rxgain = 31 - transmitter->attenuation;
        }

        if (rxgain <  0) { rxgain = 0; }

        if (rxgain > 60) { rxgain = 60; }

        output_buffer[C4] = 0x40 | rxgain;
      } else {
        if (isTransmitting()) {
          output_buffer[C4] = 0x20 | (transmitter->attenuation & 0x1F);
        } else {
          output_buffer[C4] = 0x20 | (adc[0].attenuation & 0x1F);
        }
      }

      command = 5;
      break;

    case 5:
      output_buffer[C0] = 0x16;

      if (n_adc == 2) {
        // must set bit 5 ("Att enable") all the time
        // upon transmitting, use high attenuation, since this is
        // the best thing you can do when using DIVERSITY to protect
        // the second ADC from strong signals from the auxiliary antenna.
        // (ANAN-7000 firmware does this automatically).
        if (isTransmitting()) {
          output_buffer[C1] = 0x3F;
        } else {
          // if diversity is enabled, use RX1 att value for RX2
          if (diversity_enabled) {
            output_buffer[C1] = 0x20 | (adc[0].attenuation & 0x1F);
          } else {
            output_buffer[C1] = 0x20 | (adc[1].attenuation & 0x1F);
          }
        }
      }

      if (cw_keys_reversed != 0) {
        output_buffer[C2] |= 0x40;
      }

      output_buffer[C3] = cw_keyer_speed | (cw_keyer_mode << 6);
      output_buffer[C4] = cw_keyer_weight | (cw_keyer_spacing << 7);
      command = 6;
      break;

    case 6:
      // need to add tx attenuation and rx ADC selection
      output_buffer[C0] = 0x1C;

      // if n_adc == 1, there is only a single ADC, so we can leave everything
      // set to zero
      if (n_adc  > 1) {
        // set adc of the two RX associated with the two piHPSDR receivers
        if (diversity_enabled) {
          // use ADC0 for RX1 and ADC1 for RX2 (fixed setting)
          output_buffer[C1] |= 0x04;
        } else {
          output_buffer[C1] |= (receiver[0]->adc << (2 * rx1channel));
          output_buffer[C1] |= (receiver[1]->adc << (2 * rx2channel));
        }

        //
        // This is probably never needed. It allows to assign ADC1
        // to the RX feedback channel
        //
        if (rxfdbkchan > rx2channel && transmitter->puresignal) {
          output_buffer[C1] |= (receiver[PS_RX_FEEDBACK]->adc << (2 * rxfdbkchan));
        }
      }

      if (device == DEVICE_HERMES_LITE2) {
        // bit7: enable TX att, bit6: enable 6-bit value, bit5:0 value
        int rxgain = 31 - transmitter->attenuation;

        if (rxgain <  0) { rxgain = 0; }

        if (rxgain > 60) { rxgain = 60; }

        output_buffer[C3] = 0xC0 | rxgain;
      } else {
        output_buffer[C3] = transmitter->attenuation & 0x1F; // Step attenuator of first ADC, value used when TXing
      }

      command = 7;
      break;

    case 7:
      output_buffer[C0] = 0x1E;

      if ((txmode == modeCWU || txmode == modeCWL) && !tune && cw_keyer_internal
          && !transmitter->twotone
          && !CAT_cw_is_active) {
        output_buffer[C1] |= 0x01;
      }

      //
      // This is a quirk working around a bug in the
      // FPGA iambic keyer
      //
      uint8_t rfdelay = cw_keyer_ptt_delay;
      uint8_t rfmax = 900 / cw_keyer_speed;
  
      if (rfdelay > rfmax) { rfdelay = rfmax; }

      output_buffer[C2] = cw_keyer_sidetone_volume;
      output_buffer[C3] = rfdelay;
      command = 8;
      break;

    case 8:
      output_buffer[C0] = 0x20;
      output_buffer[C1] = (cw_keyer_hang_time >> 2) & 0xFF;
      output_buffer[C2] = cw_keyer_hang_time & 0x03;
      output_buffer[C3] = (cw_keyer_sidetone_frequency >> 4) & 0xFF;
      output_buffer[C4] = cw_keyer_sidetone_frequency & 0x0F;
      command = 9;
      break;

    case 9:
      output_buffer[C0] = 0x22;
      output_buffer[C1] = (eer_pwm_min >> 2) & 0xFF;
      output_buffer[C2] = eer_pwm_min & 0x03;
      output_buffer[C3] = (eer_pwm_max >> 3) & 0xFF;
      output_buffer[C4] = eer_pwm_max & 0x03;
      command = 10;
      break;

    case 10:
      //
      // This is possibly only relevant for Orion-II boards
      //
      output_buffer[C0] = 0x24;

      if (isTransmitting()) {
        output_buffer[C1] |= 0x80; // ground RX2 on transmit, bit0-6 are Alex2 filters
      }

      if (receiver[0]->alex_antenna == 5) { // XVTR
        output_buffer[C2] |= 0x02;          // Alex2 XVTR enable
      }

      if (transmitter->puresignal) {
        output_buffer[C2] |= 0x40;       // Synchronize RX5 and TX frequency on transmit (ANAN-7000)
      }

      //
      // This was the last command defined in the HPSDR document so we
      // roll back to the first command.
      // The HermesLite-II uses an extended command set so in this case
      // we proceed.
      if (device == DEVICE_HERMES_LITE2) {
        command = 11;
      } else {
        command = 1;
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
      output_buffer[C0] = 0x2E;
      output_buffer[C3] = 20; // 20 msec PTT hang time, only bits 4:0
      output_buffer[C4] = 40; // 40 msec TX latency,    only bits 6:0
      //
      // This was the last command we use out of the extended HL2 command set,
      // so roll back to the first one. It is obvious how to extend this
      // to cover more of the HL2 extended command set.
      //
      command = 1;
      break;
    }
  }

  // set mox
  if (isTransmitting()) {
    if (txmode == modeCWU || txmode == modeCWL) {
      //
      //    For "internal" CW, we should not set
      //    the MOX bit, everything is done in the FPGA.
      //
      //    However, if we are doing CAT CW, local CW or tuning/TwoTone,
      //    we must put the SDR into TX mode *here*.
      //
      if (tune || CAT_cw_is_active || !cw_keyer_internal || transmitter->twotone) {
        output_buffer[C0] |= 0x01;
      }
    } else {
      // not doing CW? always set MOX if transmitting
      output_buffer[C0] |= 0x01;
    }
  }

  //
  // if we have a USB interfaced Ozy device:
  //
  if (device == DEVICE_OZY) {
#ifdef USBOZY
    ozyusb_write(output_buffer, OZY_BUFFER_SIZE);
#endif
  } else {
    metis_write(0x02, output_buffer, OZY_BUFFER_SIZE);
  }

  //t_print("C0=%02X C1=%02X C2=%02X C3=%02X C4=%02X\n",
  //                output_buffer[C0],output_buffer[C1],output_buffer[C2],output_buffer[C3],output_buffer[C4]);
}

#ifdef USBOZY
static void ozyusb_write(unsigned char* buffer, int length) {
  int i;
  //static unsigned char usb_output_buffer[EP6_BUFFER_SIZE];
  //static unsigned char usb_buffer_block = 0;
  i = ozy_write(EP2_OUT_ID, buffer, length);

  if (i != length) {
    if (i == USB_TIMEOUT) {
      t_print("%s: ozy_write timeout for %d bytes\n", __FUNCTION__, length);
    } else {
      t_print("%s: ozy_write for %d bytes returned %d\n", __FUNCTION__, length, i);
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

        //t_print("%s: written %d\n",__FUNCTION__,i);
        //dump_buffer(usb_output_buffer,EP6_BUFFER_SIZE);

        if(i != EP6_BUFFER_SIZE)
        {
          if(i==USB_TIMEOUT) {
            while(i==USB_TIMEOUT) {
              t_print("%s: USB_TIMEOUT: ozy_write ...\n",__FUNCTION__);
              i = ozy_write(EP2_OUT_ID,usb_output_buffer,EP6_BUFFER_SIZE);
            }
            t_print("%s: ozy_write TIMEOUT\n",__FUNCTION__);
          } else {
            t_perror("old_protocol: OzyWrite ozy failed");
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

static int metis_write(unsigned char ep, unsigned const char* buffer, int length) {
  int i;

  // copy the buffer over
  for (i = 0; i < 512; i++) {
    metis_buffer[i + metis_offset] = buffer[i];
  }

  if (metis_offset == 8) {
    metis_offset = 520;
  } else {
    metis_buffer[0] = 0xEF;
    metis_buffer[1] = 0xFE;
    metis_buffer[2] = 0x01;
    metis_buffer[3] = ep;
    metis_buffer[4] = (send_sequence >> 24) & 0xFF;
    metis_buffer[5] = (send_sequence >> 16) & 0xFF;
    metis_buffer[6] = (send_sequence >> 8) & 0xFF;
    metis_buffer[7] = (send_sequence) & 0xFF;
    send_sequence++;
    metis_send_buffer(&metis_buffer[0], 1032);
    metis_offset = 8;
  }

  return length;
}

static void metis_restart() {
  int i;
  t_print("%s\n", __FUNCTION__);

  //
  // In TCP-ONLY mode, we possibly need to re-connect
  // since if we come from a METIS-stop, the server
  // has closed the socket. Note that the UDP socket, once
  // opened is never closed.
  //
  if (radio->use_tcp && tcp_socket < 1) { open_tcp_socket(); }

  // reset metis frame
  metis_offset = 8;
  // reset current rx
  current_rx = 0;

  //
  // When restarting, clear the IQ and audio samples
  //
  for (i = 8; i < OZY_BUFFER_SIZE; i++) {
    output_buffer[i] = 0;
  }

  //
  // Some (older) HPSDR apps on the RedPitaya have very small
  // buffers that over-run if too much data is sent
  // to the RedPitaya *before* sending a METIS start packet.
  // We fill the DUC FIFO here with about 500 samples before
  // starting. This also sends some vital C&C data.
  // Note we send 8 OZY buffers, that is, 4 METIS buffers
  // containing 504 samples.
  //
  command = 1;

  for (i = 0; i < 8; i++) {
    ozy_send_buffer();
  }

  usleep(100000);

  // start the data flowing
  // No mutex here, since metis_restart() is mutex protected
  if (device != DEVICE_OZY) {
    metis_start_stop(1);
    usleep(100000);
  }
}

static void metis_start_stop(int command) {
  int i;
  unsigned char buffer[1032];
  t_print("%s: %d\n", __FUNCTION__, command);
  running = command;

  if (device == DEVICE_OZY) { return; }

  buffer[0] = 0xEF;
  buffer[1] = 0xFE;
  buffer[2] = 0x04;     // start/stop command
  buffer[3] = command;  // send EP6 and EP4 data (0x00=stop)

  if (tcp_socket < 0) {
    // use UDP  -- send a short packet
    for (i = 4; i < 64; i++) {
      buffer[i] = 0x00;
    }

    metis_send_buffer(buffer, 64);
  } else {
    // use TCP -- send a long packet
    //
    // Stop the sending of TX/audio packets (1032-byte-length) and wait a while
    // Then, send the start/stop buffer with a length of 1032
    //
    usleep(100000);

    for (i = 4; i < 1032; i++) {
      buffer[i] = 0x00;
    }

    metis_send_buffer(buffer, 1032);
    //
    // Wait a while before resuming sending TX/audio packets.
    // This prevents mangling of data from TX/audio and Start/Stop packets.
    //
    usleep(100000);
  }

  if (command == 0 && tcp_socket >= 0) {
    // We just have sent a METIS stop in TCP
    // Radio will close the TCP connection, therefore we do this as well
    int tmp = tcp_socket;
    tcp_socket = -1;
    usleep(100000);  // give some time to swallow incoming TCP packets
    close(tmp);
    t_print("TCP socket closed\n");
  }
}

static void metis_send_buffer(unsigned char* buffer, int length) {
  int bytes_sent;
  //
  // Send using either the UDP or TCP socket. Do not use TCP for
  // packets that are not 1032 bytes long
  //

  //t_print("%s: length=%d\n",__FUNCTION__,length);

  if (tcp_socket >= 0) {
    if (length != 1032) {
      t_print("PROGRAMMING ERROR: TCP LENGTH != 1032\n");
      exit(-1);
    }

    if (sendto(tcp_socket, buffer, length, 0, NULL, 0) != length) {
      t_perror("sendto socket failed for TCP metis_send_data\n");
    }
  } else if (data_socket >= 0) {
    //t_print("%s: sendto %d for %s:%d length=%d\n",__FUNCTION__,data_socket,inet_ntoa(data_addr.sin_addr),ntohs(data_addr.sin_port),length);
    bytes_sent = sendto(data_socket, buffer, length, 0, (struct sockaddr*)&data_addr, sizeof(data_addr));

    if (bytes_sent != length) {
      t_print("%s: UDP sendto failed: %d: %s\n", __FUNCTION__, errno, strerror(errno));
    }
  } else {
    // This should not happen
    t_print("METIS send: neither UDP nor TCP socket available!\n");
    exit(-1);
  }
}
