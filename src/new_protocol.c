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

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <net/if_arp.h>
#include <net/if.h>
#include <ifaddrs.h>
#include <semaphore.h>
#include <math.h>
#include <sys/select.h>
#include <signal.h>

#include <wdsp.h>

#include "alex.h"
#include "audio.h"
#include "band.h"
#include "new_protocol.h"
#include "discovered.h"
#include "mode.h"
#include "filter.h"
#include "radio.h"
#include "receiver.h"
#include "transmitter.h"
#include "vfo.h"
#include "toolbar.h"
#include "vox.h"
#include "ext.h"
#include "iambic.h"
#include "message.h"
#ifdef SATURN
  #include "saturnmain.h"
#endif

#define min(x,y) (x<y?x:y)

#define PI 3.1415926535897932F

/*
 * A new 'action table' defines what to to
 * with a sample packet received from a DDC
 */

#define RXACTION_SKIP   0    // skip samples
#define RXACTION_NORMAL 1    // deliver 238 samples to a receiver
#define RXACTION_PS     2    // deliver 2*119 samples to PS engine
#define RXACTION_DIV    3    // take 2*119 samples, mix them, deliver to a receiver

static int rxcase[MAX_DDC];
static int rxid[MAX_DDC];

int data_socket = -1;

static volatile int running;

static int dash = 0;
static int dot = 0;

static struct sockaddr_in base_addr;
static int base_addr_length;

static struct sockaddr_in receiver_addr;
static int receiver_addr_length;

static struct sockaddr_in transmitter_addr;
static int transmitter_addr_length;

static struct sockaddr_in high_priority_addr;
static int high_priority_addr_length;

static struct sockaddr_in audio_addr;
static int audio_addr_length;

static struct sockaddr_in iq_addr;
static int iq_addr_length;

static struct sockaddr_in data_addr[MAX_DDC];
static int data_addr_length[MAX_DDC];

static GThread *new_protocol_thread_id;
static GThread *new_protocol_rxaudio_thread_id;
static GThread *new_protocol_txiq_thread_id;
static GThread *new_protocol_timer_thread_id;

static long high_priority_sequence = 0;
static long general_sequence = 0;
static long rx_specific_sequence = 0;
static long tx_specific_sequence = 0;
static long ddc_sequence[MAX_DDC];

static long tx_iq_sequence = 0;

static long highprio_rcvd_sequence = 0;
static long micsamples_sequence = 0;

#ifdef __APPLE__
  static sem_t *high_priority_sem_ready;
  static sem_t *high_priority_sem_buffer;
  static sem_t *mic_line_sem;
  static sem_t *iq_sem[MAX_DDC];
  static sem_t *txiq_sem;
  static sem_t *rxaudio_sem;
#else
  static sem_t high_priority_sem_ready;
  static sem_t high_priority_sem_buffer;
  static sem_t mic_line_sem;
  static sem_t iq_sem[MAX_DDC];
  static sem_t txiq_sem;
  static sem_t rxaudio_sem;
#endif

static GThread *high_priority_thread_id;
static GThread *mic_line_thread_id;
static GThread *iq_thread_id[MAX_DDC];

static long audio_sequence = 0;

// Use this to determine the source port of messages received
static struct sockaddr_in addr;
static socklen_t length = sizeof(addr);

/////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Ring buffer for outgoing samples.
// Samples going to the radio are produced in big chunks.
// The TX engine receives bunches of mic samples (e.g. 512),
// and produces bunches of TX IQ samples (2048 in this case).
// During RX, audio samples are also created in chunks although
// they are smaller, namely 1024 / (sample_rate/48).
//
// So the idea is to put all the samples that go to the radio into
// a large ring buffer (about 4k samples), and send them to the
// radio following the pace of incoming mic samples.
//
// TXIQRINGBUF must contain a multiple of 1440 bytes (240 samples).
// RXAUDIORINGBUF must contain a multiple of 256 bytes (64 samples).
//
// The ring buffers must be thread-safe.
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////

#define TXIQRINGBUFLEN    97920  // (85 msec)
#define RXAUDIORINGBUFLEN 16384  // (85 msec)

static unsigned char *RXAUDIORINGBUF = NULL;
static unsigned char *TXIQRINGBUF = NULL;

static volatile int txiq_inptr        = 0;  // pointer updated when writing into the ring buffer
static volatile int txiq_outptr       = 0;  // pointer updated when reading from the ring buffer
static volatile int txiq_count        = 0;  // number of samples queued since last sem_post

static volatile int rxaudio_inptr     = 0;  // pointer updated when writing into the ring buffer
static volatile int rxaudio_outptr    = 0;  // pointer updated when reading from the ring buffer
static volatile int rxaudio_count     = 0;  // number of samples queued since last sem_post
static volatile int rxaudio_drain     = 0;  // a flag for draining the RX audio buffer
static volatile int rxaudio_flag      = 0;  // 0: RX, 1: TX

static pthread_mutex_t send_rxaudio_mutex   = PTHREAD_MUTEX_INITIALIZER;

/////////////////////////////////////////////////////////////////////////////
//
// PEDESTRIAN BUFFER MANAGEMENT
//
////////////////////////////////////////////////////////////////////////////
//
// Instead of allocating and free-ing (malloc/free) the network buffers
// at a very high rate, we do it the "pedestrian" way, which may
// alleviate the system load a little.
//
// Therefore we allocate a pool of network buffers *once*, make
// them a linked list, and simply maintain a "free" flag.
//
// This ONLY applies to the network buffers filled with data in
// new_protocol_thread(), so this need not be thread-safe.
//
////////////////////////////////////////////////////////////////////////////

//
// number of buffers allocated (for statistics)
//
static int num_buf = 0;

//
// head of buffer list
//
static mybuffer *buflist = NULL;

//
// The buffers used by new_protocol_thread
//
#define RXIQRINGBUFLEN 512
static volatile mybuffer *iq_buffer[MAX_DDC][RXIQRINGBUFLEN];
static volatile int iq_inptr[MAX_DDC] = { 0 };
static volatile int iq_outptr[MAX_DDC] = { 0 };
static volatile int iq_count[MAX_DDC] = { 0 };

static mybuffer *high_priority_buffer;

#define MICRINGBUFLEN 64
static volatile mybuffer *mic_line_buffer[MICRINGBUFLEN];
static volatile int mic_inptr = 0;
static volatile int mic_outptr = 0;
static volatile int mic_count = 0;

static unsigned char general_buffer[60];
static unsigned char high_priority_buffer_to_radio[1444];
static unsigned char transmit_specific_buffer[60];
static unsigned char receive_specific_buffer[1444];

//
// new_protocol_receive_specific and friends are not thread-safe, but called
// periodically from  timer thread *and* asynchronously from everywhere else
// therefore we need to implement a critical section for each of these functions.
// The audio buffer needs a mutex since both RX and TX threads may write to
// this one (CW side tone).
//

static pthread_mutex_t rx_spec_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t tx_spec_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t hi_prio_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t general_mutex = PTHREAD_MUTEX_INITIALIZER;

static int local_ptt = 0;

static void new_protocol_start(void);
static void new_protocol_high_priority(void);
static void new_protocol_general(void);
static void new_protocol_receive_specific(void);
static void new_protocol_transmit_specific(void);
static gpointer new_protocol_thread(gpointer data);
static gpointer new_protocol_rxaudio_thread(gpointer data);
static gpointer new_protocol_txiq_thread(gpointer data);
static gpointer new_protocol_timer_thread(gpointer data);
static gpointer high_priority_thread(gpointer data);
static gpointer mic_line_thread(gpointer data);
static gpointer iq_thread(gpointer data);
static void  process_iq_data(unsigned char *buffer, RECEIVER *rx);
static void  process_ps_iq_data(unsigned char *buffer);
static void process_div_iq_data(unsigned char *buffer);
static void  process_high_priority(void);
static void  process_mic_data(unsigned char *buffer);

//
// Obtain a free buffer. If no one is available allocate
// 5 new ones. Note these buffer "live" as long as the
// program lives. They are never released. Measurements show
// that in typical runs, only a handful of buffers is ever
// allocated.
//
static mybuffer *get_my_buffer() {
  int i;
  mybuffer *bp = buflist;

  while (bp) {
    if (bp->free) {
      // found free buffer. Mark as used and return that one.
      bp->free = 0;
      return bp;
    }

    bp = bp->next;
  }

  //
  // no free buffer found, allocate some extra ones
  // and add to the head of the list
  //
  for (i = 0; i < 25; i++) {
    bp = malloc(sizeof(mybuffer));
    bp->free = 1;
    bp->next = buflist;
    buflist = bp;
    num_buf++;
  }

  t_print("NewProtocol: number of buffers increased to %d\n", num_buf);
  // Mark the first buffer in list as used and return that one.
  buflist->free = 0;
  return buflist;
}

void schedule_high_priority() {
  new_protocol_high_priority();
}

void schedule_general() {
  new_protocol_general();
}

void schedule_receive_specific() {
  new_protocol_receive_specific();
}

void schedule_transmit_specific() {
  new_protocol_transmit_specific();
}

void update_action_table() {
  //
  // Depending on the values of mox, puresignal, and diversity,
  // determine the actions to be taken when a DDC packet arrives
  //
  int flag = 0;
  int xmit = isTransmitting(); // store such that it cannot change while building the flag
  int newdev = (device == NEW_DEVICE_ANGELIA  || device == NEW_DEVICE_ORION ||
                device == NEW_DEVICE_ORION2 || device == NEW_DEVICE_SATURN);

  if (duplex && xmit) { flag += 10000; }

  if (newdev) { flag += 1000; }

  if (xmit) { flag += 100; }

  if (transmitter->puresignal && xmit) { flag += 10; }

  if (diversity_enabled && !xmit) { flag += 1; }

  // Note that the PureSignal and DUPLEX flags are only set in the TX cases, since they
  // make no difference upon RXing
  // Note further, we do not use the diversity mixer upon transmitting.
  //
  // Therefore, the following 12 values for flag are possible:
  // flag=     0
  // flag=     1
  // flag=   100
  // flag=   110
  // flag=  1000
  // flag=  1001
  // flag=  1100
  // flag=  1110
  // flag= 10100
  // flag= 10110
  // flag= 11100
  // flag= 11110
  //
  // Set up rxcase and rxid for each of the 12 cases
  // note that rxid[i] can be left unspecified if rxcase[i] == RXACTION_SKIP
  //
  rxcase[0] = RXACTION_SKIP;
  rxcase[1] = RXACTION_SKIP;
  rxcase[2] = RXACTION_SKIP;
  rxcase[3] = RXACTION_SKIP;

  switch (flag) {
  case       0:                                                       // HERMES, RX, no DIVERSITY
  case   10100:                                                       // HERMES, TX, no PureSignal, DUPLEX
    rxid[0] = 0;
    rxcase[0] = RXACTION_NORMAL;

    if (receivers > 1) {
      rxid[1] = 1;
      rxcase[1] = RXACTION_NORMAL;
    }

    break;

  case     1:                                                         // never occurs since HERMES has only 1 ADC
  case  1001:                                                         // ORION, RX, DIVERSITY
    rxid[0] = 0;
    rxcase[0] = RXACTION_DIV;
    break;

  case  100:                                                          // HERMES or ORION, TX, no PureSignal, no DUPLEX
  case 1100:
    // just skip samples
    break;

  case  110:                                                          // HERMES or ORION, TX, PureSignal, no DUPLEX
  case 1110:
  case 10110:                                                         // HERMES, TX, DUPLEX, PS: duplex is ignored
    rxcase[0] = RXACTION_PS;
    break;

  case 11110:                                                         // ORION, TX, PureSignal, DUPLEX
    rxcase[0] = RXACTION_PS;

  /* FALLTHROUGH */
  case 1000:                                                          // ORION, RX, no DIVERSITY
  case 11100:                                                         // ORION, TX, no PureSignal, DUPLEX
    rxid[2] = 0;
    rxcase[2] = RXACTION_NORMAL;

    if (receivers > 1) {
      rxid[3] = 1;
      rxcase[3] = RXACTION_NORMAL;
    }

    break;

  default:
    t_print("ACTION TABLE: case not handled: %d\n", flag);
    break;
  }
}

void new_protocol_init(int pixels) {
  int i;

  //
  // This is allocated once and forever
  //
  if (TXIQRINGBUF == NULL) {
    TXIQRINGBUF = g_new(unsigned char, TXIQRINGBUFLEN);
  }

  if (RXAUDIORINGBUF == NULL) {
    RXAUDIORINGBUF = g_new(unsigned char, RXAUDIORINGBUFLEN);
  }

  memset(rxcase, 0, sizeof(rxcase));
  memset(rxid, 0, sizeof(rxid));
  memset(ddc_sequence, 0, sizeof(ddc_sequence));
  update_action_table();

  if (transmitter->local_microphone) {
    if (audio_open_input() != 0) {
      t_print("audio_open_input failed\n");
      transmitter->local_microphone = 0;
    }
  }

  //
  // Initialize semaphores for the never-finishing threads
  // (HighPrio, Mic, rxIQ)
  // and spawn the threads.
  //
#ifdef __APPLE__
  high_priority_sem_ready = apple_sem(0);
  high_priority_sem_buffer = apple_sem(0);
  mic_line_sem = apple_sem(0);

  for (i = 0; i < MAX_DDC; i++) {
    iq_sem[i] = apple_sem(0);
  }

#else
  (void)sem_init(&high_priority_sem_ready, 0, 0); // check return value!
  (void)sem_init(&high_priority_sem_buffer, 0, 0); // check return value!
  (void)sem_init(&mic_line_sem, 0, 0); // check return value!

  for (i = 0; i < MAX_DDC; i++) {
    (void)sem_init(&iq_sem[i], 0, 0); // check return value!
  }

#endif
  running = 1;
  high_priority_thread_id = g_thread_new( "P2 HP", high_priority_thread, NULL);
  mic_line_thread_id = g_thread_new( "P2 MIC", mic_line_thread, NULL);

  for (i = 0; i < MAX_DDC; i++) {
    char text[16];
    snprintf(text, 16, "P2 DDC%d", i);
    iq_thread_id[i] = g_thread_new(text, iq_thread, GINT_TO_POINTER(i));
  }

  //
  // start RX audio and TXIQ sending threads
  //
#ifdef __APPLE__
  txiq_sem = apple_sem(0);
  rxaudio_sem = apple_sem(0);
#else
  (void)sem_init(&txiq_sem, 0, 0); // check return value!
  (void)sem_init(&rxaudio_sem, 0, 0); // check return value!
#endif
  new_protocol_rxaudio_thread_id = g_thread_new( "P2 SPKR", new_protocol_rxaudio_thread, NULL);
  new_protocol_txiq_thread_id = g_thread_new( "P2 TXIQ", new_protocol_txiq_thread, NULL);

  if (have_saturn_xdma) {
#ifdef SATURN
    saturn_init();
#endif
  } else {
    data_socket = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP);

    if (data_socket < 0) {
      t_perror("NewProtocol: create data_socket:");
      exit(-1);
    }

    int optval = 1;
    socklen_t optlen = sizeof(optval);
    setsockopt(data_socket, SOL_SOCKET, SO_REUSEADDR, &optval, optlen);
    setsockopt(data_socket, SOL_SOCKET, SO_REUSEPORT, &optval, optlen);
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

    if (setsockopt(data_socket, SOL_SOCKET, SO_RCVBUF, &optval, optlen) < 0) {
      t_perror("data_socket: set SO_RCVBUF");
    }

    optval = 0x10000;

    if (setsockopt(data_socket, SOL_SOCKET, SO_SNDBUF, &optval, optlen) < 0) {
      t_perror("data_socket: set SO_SNDBUF");
    }

    optlen = sizeof(optval);

    if (getsockopt(data_socket, SOL_SOCKET, SO_RCVBUF, &optval, &optlen) < 0) {
      t_perror("data_socket: get SO_RCVBUF");
    } else {
      if (optlen == sizeof(optval)) { t_print("UDP Socket RCV buf size=%d\n", optval); }
    }

    optlen = sizeof(optval);

    if (getsockopt(data_socket, SOL_SOCKET, SO_SNDBUF, &optval, &optlen) < 0) {
      t_perror("data_socket: get SO_SNDBUF");
    } else {
      if (optlen == sizeof(optval)) { t_print("UDP Socket SND buf size=%d\n", optval); }
    }

    //#endif
#ifdef __APPLE__
    //optval = 0x10;  // IPTOS_LOWDELAY
    optval = 0xb8;  // DSCP EF

    if (setsockopt(data_socket, IPPROTO_IP, IP_TOS, &optval, sizeof(optval)) < 0) {
      t_perror("data_socket: IP_TOS");
    }

#endif

    // bind to the interface
    if (bind(data_socket, (struct sockaddr * )&radio->info.network.interface_address,
             radio->info.network.interface_length) < 0) {
      t_perror("bind socket failed for data_socket:");
      exit(-1);
    }

    t_print("new_protocol_init: data_socket %d bound to interface %s:%d\n", data_socket,
            inet_ntoa(radio->info.network.interface_address.sin_addr), ntohs(radio->info.network.interface_address.sin_port));
    memcpy(&base_addr, &radio->info.network.address, radio->info.network.address_length);
    base_addr_length = radio->info.network.address_length;
    base_addr.sin_port = htons(GENERAL_REGISTERS_FROM_HOST_PORT);
    //t_print("base_addr=%s\n",inet_ntoa(radio->info.network.address.sin_addr));
    memcpy(&receiver_addr, &radio->info.network.address, radio->info.network.address_length);
    receiver_addr_length = radio->info.network.address_length;
    receiver_addr.sin_port = htons(RECEIVER_SPECIFIC_REGISTERS_FROM_HOST_PORT);
    //t_print("receive_addr=%s\n",inet_ntoa(radio->info.network.address.sin_addr));
    memcpy(&transmitter_addr, &radio->info.network.address, radio->info.network.address_length);
    transmitter_addr_length = radio->info.network.address_length;
    transmitter_addr.sin_port = htons(TRANSMITTER_SPECIFIC_REGISTERS_FROM_HOST_PORT);
    //t_print("transmit_addr=%s\n",inet_ntoa(radio->info.network.address.sin_addr));
    memcpy(&high_priority_addr, &radio->info.network.address, radio->info.network.address_length);
    high_priority_addr_length = radio->info.network.address_length;
    high_priority_addr.sin_port = htons(HIGH_PRIORITY_FROM_HOST_PORT);
    //t_print("high_priority_addr=%s\n",inet_ntoa(radio->info.network.address.sin_addr));
    //t_print("new_protocol_thread: high_priority_addr setup for port %d\n",HIGH_PRIORITY_FROM_HOST_PORT);
    memcpy(&audio_addr, &radio->info.network.address, radio->info.network.address_length);
    audio_addr_length = radio->info.network.address_length;
    audio_addr.sin_port = htons(AUDIO_FROM_HOST_PORT);
    //t_print("audio_addr=%s\n",inet_ntoa(radio->info.network.address.sin_addr));
    memcpy(&iq_addr, &radio->info.network.address, radio->info.network.address_length);
    iq_addr_length = radio->info.network.address_length;
    iq_addr.sin_port = htons(TX_IQ_FROM_HOST_PORT);
    //t_print("iq_addr=%s\n",inet_ntoa(radio->info.network.address.sin_addr));

    for (i = 0; i < MAX_DDC; i++) {
      memcpy(&data_addr[i], &radio->info.network.address, radio->info.network.address_length);
      data_addr_length[i] = radio->info.network.address_length;
      data_addr[i].sin_port = htons(RX_IQ_TO_HOST_PORT_0 + i);
    }

    new_protocol_thread_id = g_thread_new( "P2 main", new_protocol_thread, NULL);
  }

  new_protocol_general();
  new_protocol_start();
  new_protocol_high_priority();
}

static void new_protocol_general() {
  BAND *band;
  int rc;
  int txvfo = get_tx_vfo();
  pthread_mutex_lock(&general_mutex);
  band = band_get_band(vfo[txvfo].band);
  memset(general_buffer, 0, sizeof(general_buffer));
  general_buffer[0] = general_sequence >> 24;
  general_buffer[1] = general_sequence >> 16;
  general_buffer[2] = general_sequence >> 8;
  general_buffer[3] = general_sequence;
  // use defaults apart from
  general_buffer[37] = 0x08; //  phase word (not frequency)
  general_buffer[38] = 0x01; //  enable hardware timer

  if (!pa_enabled || band->disablePA) {
    general_buffer[58] = 0x00;
  } else {
    general_buffer[58] = 0x01; // enable PA
  }

  // t_print("new_protocol_general: PA Enable=%02X\n",general_buffer[58]);

  if (filter_board == APOLLO) {
    general_buffer[58] |= 0x02; // enable APOLLO tuner
  }

  if (filter_board == ALEX) {
    if (device == NEW_DEVICE_ORION2 || device == NEW_DEVICE_SATURN) {
      general_buffer[59] = 0x03; // enable Alex 0 and 1
    } else {
      general_buffer[59] = 0x01; // enable Alex 0
    }
  }

  //t_print("Alex Enable=%02X\n",general_buffer[59]);
  //t_print("new_protocol_general: %s:%d\n",inet_ntoa(base_addr.sin_addr),ntohs(base_addr.sin_port));

  if (have_saturn_xdma) {
#ifdef SATURN
    saturn_handle_general_packet(false, general_buffer);
#endif
  } else {
    if ((rc = sendto(data_socket, general_buffer, sizeof(general_buffer), 0, (struct sockaddr * )&base_addr,
                     base_addr_length)) < 0) {
      t_perror("sendto socket failed for general:");
      exit(1);
    }

    if (rc != sizeof(general_buffer)) {
      t_print("sendto socket for general: %d rather than %ld", rc, (long)sizeof(general_buffer));
    }
  }

  general_sequence++;
  pthread_mutex_unlock(&general_mutex);
}

static void new_protocol_high_priority() {
  int i;
  BAND *band;
  long long rx1Frequency;
  long long rx2Frequency;
  long long txFrequency;
  long long HPFfreq;  // frequency determining the HPF filters
  long long LPFfreq;  // frequency determining the LPF filters
  unsigned long phase;

  if (data_socket == -1 && !have_saturn_xdma) {
    return;
  }

  pthread_mutex_lock(&hi_prio_mutex);
  memset(high_priority_buffer_to_radio, 0, sizeof(high_priority_buffer_to_radio));
  int xmit    = isTransmitting();
  int txvfo   = get_tx_vfo();
  int rxvfo   = active_receiver->id;
  int txmode  = get_tx_mode();
  high_priority_buffer_to_radio[0] = high_priority_sequence >> 24;
  high_priority_buffer_to_radio[1] = high_priority_sequence >> 16;
  high_priority_buffer_to_radio[2] = high_priority_sequence >> 8;
  high_priority_buffer_to_radio[3] = high_priority_sequence;
  high_priority_buffer_to_radio[4] = running;

  if (xmit) {
    if (txmode == modeCWU || txmode == modeCWL) {
      //
      // For "internal" CW, we should not set
      // the MOX bit, everything is done in the FPGA.
      //
      // However, if we are doing CAT CW, local CW or tuning/TwoTone,
      // we must put the SDR into TX mode
      //
      if (tune || CAT_cw_is_active || !cw_keyer_internal || transmitter->twotone) {
        high_priority_buffer_to_radio[4] |= 0x02;
      }
    } else {
      // not doing CW? always set MOX if transmitting
      high_priority_buffer_to_radio[4] |= 0x02;
    }
  }

  //
  //  Set DDC frequencies
  //  If there is only one DDC, rx2Frequency is un-used
  //
  rx1Frequency = vfo[VFO_A].frequency - vfo[VFO_A].lo;
  rx2Frequency = vfo[VFO_B].frequency - vfo[VFO_B].lo;

  if (vfo[VFO_A].rit_enabled) {
    rx1Frequency += vfo[VFO_A].rit;
  }

  if (vfo[VFO_B].rit_enabled) {
    rx2Frequency += vfo[VFO_B].rit;
  }

  if (cw_is_on_vfo_freq) {
    if (vfo[VFO_A].mode == modeCWU) {
      rx1Frequency -= (long long)cw_keyer_sidetone_frequency;
    } else if (vfo[VFO_A].mode == modeCWL) {
      rx1Frequency += (long long)cw_keyer_sidetone_frequency;
    }

    if (vfo[VFO_B].mode == modeCWU) {
      rx2Frequency -= (long long)cw_keyer_sidetone_frequency;
    } else if (vfo[VFO_B].mode == modeCWL) {
      rx2Frequency += (long long)cw_keyer_sidetone_frequency;
    }
  }

  rx1Frequency += frequency_calibration;
  rx2Frequency += frequency_calibration;

  if (diversity_enabled && !xmit) {
    //
    // Use frequency of first receiver for both DDC0 and DDC1
    // This is overridden later if we do PureSignal TX
    // The "obscure" constant 34.952533333333333333333333333333 is 4294967296/122880000
    //
    phase = (unsigned long)(((double)rx1Frequency) * 34.952533333333333333333333333333);
    high_priority_buffer_to_radio[9] = phase >> 24;
    high_priority_buffer_to_radio[10] = phase >> 16;
    high_priority_buffer_to_radio[11] = phase >> 8;
    high_priority_buffer_to_radio[12] = phase;
    high_priority_buffer_to_radio[13] = phase >> 24;
    high_priority_buffer_to_radio[14] = phase >> 16;
    high_priority_buffer_to_radio[15] = phase >> 8;
    high_priority_buffer_to_radio[16] = phase;
  } else {
    //
    // Set frequencies for all receivers
    //
    // note that for HERMES, receiver[i] is associated with DDC(i) but beyond
    // (that is, ANGELIA, ORION, ORION2, SATURN) receiver[i] is associated with DDC(i+2)
    int ddc = 0;

    if (device == NEW_DEVICE_ANGELIA  || device == NEW_DEVICE_ORION ||
        device == NEW_DEVICE_ORION2 || device == NEW_DEVICE_SATURN) { ddc = 2; }

    phase = (unsigned long)(((double)rx1Frequency) * 34.952533333333333333333333333333);
    high_priority_buffer_to_radio[9 + (ddc * 4)] = phase >> 24;
    high_priority_buffer_to_radio[10 + (ddc * 4)] = phase >> 16;
    high_priority_buffer_to_radio[11 + (ddc * 4)] = phase >> 8;
    high_priority_buffer_to_radio[12 + (ddc * 4)] = phase;

    if (receivers > 1) {
      phase = (unsigned long)(((double)rx2Frequency) * 34.952533333333333333333333333333);
      high_priority_buffer_to_radio[13 + (ddc * 4)] = phase >> 24;
      high_priority_buffer_to_radio[14 + (ddc * 4)] = phase >> 16;
      high_priority_buffer_to_radio[15 + (ddc * 4)] = phase >> 8;
      high_priority_buffer_to_radio[16 + (ddc * 4)] = phase;
    }
  }

  //
  //  Set DUC frequency
  //
  txFrequency = vfo[txvfo].frequency - vfo[txvfo].lo;

  if (vfo[txvfo].ctun) { txFrequency += vfo[txvfo].offset; }

  if (vfo[txvfo].xit_enabled) {
    txFrequency += vfo[txvfo].xit;
  }

  if (!cw_is_on_vfo_freq) {
    if (txmode == modeCWU) {
      txFrequency += (long long)cw_keyer_sidetone_frequency;
    } else if (txmode == modeCWL) {
      txFrequency -= (long long)cw_keyer_sidetone_frequency;
    }
  }

  txFrequency += frequency_calibration;
  phase = (unsigned long)(((double)txFrequency) * 34.952533333333333333333333333333);

  if (xmit && transmitter->puresignal) {
    //
    // Set DDC0 and DDC1 (synchronized) to the transmit frequency
    //
    high_priority_buffer_to_radio[9] = phase >> 24;
    high_priority_buffer_to_radio[10] = phase >> 16;
    high_priority_buffer_to_radio[11] = phase >> 8;
    high_priority_buffer_to_radio[12] = phase;
    high_priority_buffer_to_radio[13] = phase >> 24;
    high_priority_buffer_to_radio[14] = phase >> 16;
    high_priority_buffer_to_radio[15] = phase >> 8;
    high_priority_buffer_to_radio[16] = phase;
  }

  high_priority_buffer_to_radio[329] = phase >> 24;
  high_priority_buffer_to_radio[330] = phase >> 16;
  high_priority_buffer_to_radio[331] = phase >> 8;
  high_priority_buffer_to_radio[332] = phase;
  int power = transmitter->drive_level;
  high_priority_buffer_to_radio[345] = power & 0xFF;

  if (xmit) {
    band = band_get_band(vfo[txvfo].band);
    high_priority_buffer_to_radio[1401] = band->OCtx << 1;

    if (tune) {
      if (OCmemory_tune_time != 0) {
        struct timeval te;
        gettimeofday(&te, NULL);
        long long now = te.tv_sec * 1000LL + te.tv_usec / 1000;

        if (tune_timeout > now) {
          high_priority_buffer_to_radio[1401] |= OCtune << 1;
        }
      } else {
        high_priority_buffer_to_radio[1401] |= OCtune << 1;
      }
    }
  } else {
    band = band_get_band(vfo[rxvfo].band);
    high_priority_buffer_to_radio[1401] = band->OCrx << 1;
  }

  //
  //  ANAN-7000/8000 and G2:
  //                  route TXout to XvtrOut out when using XVTR input
  //                  (this is the condition also implemented in old_protocol)
  //                  Note: the firmware does a logical AND with the T/R bit
  //                  such that upon RX, Xvtr port is input, and on TX, Xvrt port
  //                  is output if the XVTR_OUT bit is set.
  //
  if ((device == NEW_DEVICE_ORION2 || device == NEW_DEVICE_SATURN) && receiver[0]->alex_antenna == 5) {
    high_priority_buffer_to_radio[1400] |= ANAN7000_XVTR_OUT;
  }

  //
  //  ALEX bits
  //
  unsigned long alex0 = 0x00000000;
  unsigned long alex1 = 0x00000000;

  if (have_alex_att) {
    //
    // ANAN7000/8000 and SATURN do not have ALEX attenuators.
    //
    switch (receiver[0]->alex_attenuation) {
    case 0:
      alex0 |= ALEX_ATTENUATION_0dB;
      break;

    case 1:
      alex0 |= ALEX_ATTENUATION_10dB;
      break;

    case 2:
      alex0 |= ALEX_ATTENUATION_20dB;
      break;

    case 3:
      alex0 |= ALEX_ATTENUATION_30dB;
      break;
    }
  }

  if (xmit) {
    //
    //    Do not switch TR relay to "TX" if PA is disabled.
    //    This is necessary because the "PA enable flag" in the GeneralPacket
    //    had no effect in the Orion-II firmware up to 2.1.18
    //    (meanwhile it works: thanks to Rick N1GP)
    //    But we have to keep this "safety belt" for some time.
    //
    if (!band->disablePA  && pa_enabled) {
      alex0 |= ALEX_TX_RELAY;
    }

    if (transmitter->puresignal) {
      alex0 |= ALEX_PS_BIT;            // Bit 18
    }
  }

  //
  //  The following code is based upon the assumption that
  //  the frequency of VFO_A is used with ADC0, and that the
  //  frequency of VFO_B can safely be used to control the
  //  filters of ADC1 (if there are any).
  //

  switch (device) {
  case NEW_DEVICE_SATURN:
  case NEW_DEVICE_ORION2:

    //
    //      new ANAN-7000/8000/G2 band-pass RX filters
    //
    //      To support the ANAN-8000 we
    //      should bypass BPFs while transmitting in PureSignal,
    //      but this causes unnecessary "relay chatter" on ANAN-7000
    //      So if it should be done, 20 lines below it is shown how.
    //
    if (rx1Frequency < 1500000LL) {
      alex0 |= ALEX_ANAN7000_RX_BYPASS_BPF;
    } else if (rx1Frequency < 2100000LL) {
      alex0 |= ALEX_ANAN7000_RX_160_BPF;
    } else if (rx1Frequency < 5500000LL) {
      alex0 |= ALEX_ANAN7000_RX_80_60_BPF;
    } else if (rx1Frequency < 11000000LL) {
      alex0 |= ALEX_ANAN7000_RX_40_30_BPF;
    } else if (rx1Frequency < 22000000LL) {
      alex0 |= ALEX_ANAN7000_RX_20_15_BPF;
    } else if (rx1Frequency < 35000000LL) {
      alex0 |= ALEX_ANAN7000_RX_12_10_BPF;
    } else {
      alex0 |= ALEX_ANAN7000_RX_6_PRE_BPF;
    }

    //
    // Note that while using DIVERSITY, the second RX filter settings must match
    // those of the first RX
    //
    if (diversity_enabled) {
      rx2Frequency = rx1Frequency;
    }

    //
    //      new ANAN-7000/8000/G2 "Alex1" band-pass RX filters
    //
    if (rx2Frequency < 1500000LL) {
      alex1 |= ALEX_ANAN7000_RX_BYPASS_BPF;
    } else if (rx2Frequency < 2100000LL) {
      alex1 |= ALEX_ANAN7000_RX_160_BPF;
    } else if (rx2Frequency < 5500000LL) {
      alex1 |= ALEX_ANAN7000_RX_80_60_BPF;
    } else if (rx2Frequency < 11000000LL) {
      alex1 |= ALEX_ANAN7000_RX_40_30_BPF;
    } else if (rx2Frequency < 22000000LL) {
      alex1 |= ALEX_ANAN7000_RX_20_15_BPF;
    } else if (rx2Frequency < 35000000LL) {
      alex1 |= ALEX_ANAN7000_RX_12_10_BPF;
    } else {
      alex1 |= ALEX_ANAN7000_RX_6_PRE_BPF;
    }

    //
    //      The main purpose of RX2 is DIVERSITY. Therefore,
    //      ground RX2 upon TX *always*
    //
    if (xmit) {
      alex1 |= ALEX1_ANAN7000_RX_GNDonTX;
    }

    break;

  default:
    //
    //      Old (ANAN-100/200) high-pass filters
    //      If the second RX is active, and its ADC is ADC0, then
    //      RX HPF filter settings depend on MIN(rx1freq,rx2freq)
    //
    HPFfreq = rx1Frequency;

    if (receivers > 1) {
      if (receiver[1]->adc == 0 && rx2Frequency < rx1Frequency) {
        HPFfreq = rx2Frequency;
      }
    }

    i = 0; // flag used here for "filter bypass"

    if (HPFfreq < 1800000L) { i = 1; }

    // Bypass HPFs if using EXT1 for PureSignal feedback!
    if (xmit && transmitter->puresignal && receiver[PS_RX_FEEDBACK]->alex_antenna == 6) { i = 1; }

    if (i) {
      alex0 |= ALEX_BYPASS_HPF;
    } else if (HPFfreq < 6500000LL) {
      alex0 |= ALEX_1_5MHZ_HPF;
    } else if (HPFfreq < 9500000LL) {
      alex0 |= ALEX_6_5MHZ_HPF;
    } else if (HPFfreq < 13000000LL) {
      alex0 |= ALEX_9_5MHZ_HPF;
    } else if (HPFfreq < 20000000LL) {
      alex0 |= ALEX_13MHZ_HPF;
    } else if (HPFfreq < 50000000LL) {
      alex0 |= ALEX_20MHZ_HPF;
    } else {
      alex0 |= ALEX_6M_PREAMP;
    }

    break;
  }

  //
  //   Pre-Orion2 boards: If using Ant1/2/3, the RX signal goes through the TX low-pass
  //                      filters. Therefore we must set these according to the ADC0
  //                      (receive) frequency while RXing, according  to the Max
  //                      of rx1freq and rx2freq. If TXing, the TX freq governs the LPF
  //                      in either case.
  //
  LPFfreq = txFrequency;

  if (!xmit && (device != NEW_DEVICE_ORION2 && device != NEW_DEVICE_SATURN) && receiver[0]->alex_antenna < 3) {
    LPFfreq = rx1Frequency;

    if (receivers > 1) {
      if (receiver[1]->adc == 0 && rx2Frequency > rx1Frequency) {
        LPFfreq = rx2Frequency;
      }
    }
  }

  if (LPFfreq > 35600000LL) {
    alex0 |= ALEX_6_BYPASS_LPF;
  } else if (LPFfreq > 24000000LL) {
    alex0 |= ALEX_12_10_LPF;
  } else if (LPFfreq > 16500000LL) {
    alex0 |= ALEX_17_15_LPF;
  } else if (LPFfreq > 8000000LL) {
    alex0 |= ALEX_30_20_LPF;
  } else if (LPFfreq > 5000000LL) {
    alex0 |= ALEX_60_40_LPF;
  } else if (LPFfreq > 2500000LL) {
    alex0 |= ALEX_80_LPF;
  } else {
    alex0 |= ALEX_160_LPF;
  }

  //
  //  Set bits that route Ext1/Ext2/XVRTin to the RX
  //
  //  If transmitting with PureSignal, we must use the alex_antenna
  //  settings of the PS_RX_FEEDBACK receiver
  //
  //  ANAN-7000 routes signals differently (these bits have no function on ANAN-80000)
  //            and uses ALEX0(14) to connnect Ext/XvrtIn to the RX.
  //
  i = receiver[0]->alex_antenna;                      // 0,1,2  or 3,4,5

  if (xmit && transmitter->puresignal) {
    i = receiver[PS_RX_FEEDBACK]->alex_antenna;     // 0, 6, or 7
  }

  if (device == NEW_DEVICE_ORION2 || device == NEW_DEVICE_SATURN) {
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
    alex0 |= ALEX_RX_ANTENNA_EXT1 | ALEX_RX_ANTENNA_BYPASS;
    break;

  case 4:           // EXT2 with old pa board
    alex0 |= ALEX_RX_ANTENNA_EXT2 | ALEX_RX_ANTENNA_BYPASS;
    break;

  case 5:           // XVTR with old pa board
    alex0 |= ALEX_RX_ANTENNA_XVTR | ALEX_RX_ANTENNA_BYPASS;
    break;

  case 104:         // EXT2 with ANAN-7000: does not exist, use EXT1
  case 103:         // EXT1 with ANAN-7000
    alex0 |= ALEX_RX_ANTENNA_EXT1 | ANAN7000_RX_SELECT;
    break;

  case 105:         // XVTR with ANAN-7000
    alex0 |= ALEX_RX_ANTENNA_XVTR | ANAN7000_RX_SELECT;
    break;

  case 106:         // EXT1-on-TX with ANAN-7000: does not exist, use ByPass
  case 107:         // Bypass-on-TX with ANAN-7000
    alex0 |= ALEX_RX_ANTENNA_BYPASS;
    break;

  case 1003:        // EXT1 with new PA board
    alex0 |= ALEX_RX_ANTENNA_EXT1;
    break;

  case 1004:        // EXT2 with new PA board
    alex0 |= ALEX_RX_ANTENNA_EXT2;
    break;

  case 1005:        // XVRT with new PA board
    alex0 |= ALEX_RX_ANTENNA_XVTR;
    break;

  case 7:           // Bypass-on-TX: assume new PA board
  case 1007:
    alex0 |= ALEX_RX_ANTENNA_BYPASS;
    break;
  }

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

  if (xmit || local_ptt) {
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
    alex0 |= ALEX_TX_ANTENNA_1;
    break;

  case 1:  // ANT 2
    alex0 |= ALEX_TX_ANTENNA_2;
    break;

  case 2:  // ANT 3
    alex0 |= ALEX_TX_ANTENNA_3;
    break;
  }

  high_priority_buffer_to_radio[1432] = (alex0 >> 24) & 0xFF;
  high_priority_buffer_to_radio[1433] = (alex0 >> 16) & 0xFF;
  high_priority_buffer_to_radio[1434] = (alex0 >> 8) & 0xFF;
  high_priority_buffer_to_radio[1435] = alex0 & 0xFF;

  //t_print("ALEX0 bits:  %02X %02X %02X %02X for rx=%lld tx=%lld\n",high_priority_buffer_to_radio[1432],high_priority_buffer_to_radio[1433],high_priority_buffer_to_radio[1434],high_priority_buffer_to_radio[1435],rxFrequency,txFrequency);

  if (device == NEW_DEVICE_ORION2 || device == NEW_DEVICE_SATURN) {
    high_priority_buffer_to_radio[1430] = (alex1 >> 8) & 0xFF;
    high_priority_buffer_to_radio[1431] = alex1 & 0xFF;
    //t_print("ALEX1 bits: rx1: %02X %02X for rx=%lld\n",high_priority_buffer_to_radio[1430],high_priority_buffer_to_radio[1431],rxFrequency);
  }

  //
  //  Upon transmitting, set the attenuator of ADC0 to the "transmitter attenuation"
  //  (used in PureSignal signal strength adjustment) and the attenuator of ADC1
  //  to the maximum value (to protect RX2 in DIVERSITY setups).
  //

  if (xmit) {
    high_priority_buffer_to_radio[1443] = transmitter->attenuation;
    high_priority_buffer_to_radio[1442] = 31;
  } else {
    high_priority_buffer_to_radio[1443] = adc[0].attenuation;

    if (diversity_enabled) {
      high_priority_buffer_to_radio[1442] = adc[0].attenuation; // DIVERSITY: ADC0 att value for ADC1 as well
    } else {
      high_priority_buffer_to_radio[1442] = adc[1].attenuation;
    }
  }

  //
  //  Voila mes amis. Envoyons les 1444 octets "high priority" au radio
  //
  //t_print("new_protocol_high_priority: %s:%d\n",inet_ntoa(high_priority_addr.sin_addr),ntohs(high_priority_addr.sin_port));

  if (have_saturn_xdma) {
#ifdef SATURN
    saturn_handle_high_priority(false, high_priority_buffer_to_radio);
#endif
  } else {
    int rc;

    if ((rc = sendto(data_socket, high_priority_buffer_to_radio, sizeof(high_priority_buffer_to_radio), 0,
                     (struct sockaddr * )&high_priority_addr, high_priority_addr_length)) < 0) {
      t_perror("sendto socket failed for high priority:");
      exit(-1);
    }

    if (rc != sizeof(high_priority_buffer_to_radio)) {
      t_print("sendto socket for high_priority: %d rather than %ld", rc, (long)sizeof(high_priority_buffer_to_radio));
    }
  }

  high_priority_sequence++;
  update_action_table();
  pthread_mutex_unlock(&hi_prio_mutex);
}

static void new_protocol_transmit_specific() {
  int txmode = get_tx_mode();
  pthread_mutex_lock(&tx_spec_mutex);
  memset(transmit_specific_buffer, 0, sizeof(transmit_specific_buffer));
  transmit_specific_buffer[0] = tx_specific_sequence >> 24;
  transmit_specific_buffer[1] = tx_specific_sequence >> 16;
  transmit_specific_buffer[2] = tx_specific_sequence >> 8;
  transmit_specific_buffer[3] = tx_specific_sequence;
  transmit_specific_buffer[4] = 1; // 1 DAC
  transmit_specific_buffer[5] = 0; //  default no CW

  if ((txmode == modeCWU || txmode == modeCWL) && cw_keyer_internal && !CAT_cw_is_active) {
    //
    // Set this byte only if in CW, and if using the "internal" keyer
    //
    transmit_specific_buffer[5] |= 0x02;

    if (cw_keys_reversed) {
      transmit_specific_buffer[5] |= 0x04;
    }

    if (cw_keyer_mode == KEYER_MODE_A) {
      transmit_specific_buffer[5] |= 0x08;
    }

    if (cw_keyer_mode == KEYER_MODE_B) {
      transmit_specific_buffer[5] |= 0x28;
    }

    if (cw_keyer_sidetone_volume != 0) {
      transmit_specific_buffer[5] |= 0x10;
    }

    if (cw_keyer_spacing) {
      transmit_specific_buffer[5] |= 0x40;
    }

    if (cw_breakin) {
      transmit_specific_buffer[5] |= 0x80;
    }
  }

  //
  // This is a quirk working around a bug in the
  // FPGA iambic keyer
  //
  uint8_t rfdelay = cw_keyer_ptt_delay;
  uint8_t rfmax = 900 / cw_keyer_speed;

  if (rfdelay > rfmax) { rfdelay = rfmax; }

  transmit_specific_buffer[6] = cw_keyer_sidetone_volume & 0x7F;
  transmit_specific_buffer[7] = cw_keyer_sidetone_frequency >> 8;
  transmit_specific_buffer[8] = cw_keyer_sidetone_frequency;
  transmit_specific_buffer[9] = cw_keyer_speed;
  transmit_specific_buffer[10] = cw_keyer_weight;
  transmit_specific_buffer[11] = cw_keyer_hang_time >> 8;
  transmit_specific_buffer[12] = cw_keyer_hang_time;
  transmit_specific_buffer[13] = rfdelay;
  transmit_specific_buffer[50] = 0;

  if (mic_linein) {
    transmit_specific_buffer[50] |= 0x01;
  }

  if (mic_boost) {
    transmit_specific_buffer[50] |= 0x02;
  }

  if (mic_ptt_enabled == 0) { // set if disabled
    transmit_specific_buffer[50] |= 0x04;
  }

  if (mic_ptt_tip_bias_ring) {
    transmit_specific_buffer[50] |= 0x08;
  }

  if (mic_bias_enabled) {
    transmit_specific_buffer[50] |= 0x10;
  }

  if (mic_input_xlr) {
    transmit_specific_buffer[50] |= 0x20;
  }

  // 0..31
  transmit_specific_buffer[51] = (int)((linein_gain + 34.0) * 0.6739 + 0.5);
  // Attenuator for ADC0 upon TX
  transmit_specific_buffer[59] = transmitter->attenuation;

  //t_print("new_protocol_transmit_specific: %s:%d\n",inet_ntoa(transmitter_addr.sin_addr),ntohs(transmitter_addr.sin_port));

  if (have_saturn_xdma) {
#ifdef SATURN
    saturn_handle_duc_specific(false, transmit_specific_buffer);
#endif
  } else {
    int rc;

    if ((rc = sendto(data_socket, transmit_specific_buffer, sizeof(transmit_specific_buffer), 0,
                     (struct sockaddr * )&transmitter_addr, transmitter_addr_length)) < 0) {
      t_perror("sendto socket failed for tx specific:");
      exit(1);
    }

    if (rc != sizeof(transmit_specific_buffer)) {
      t_print("sendto socket for transmit_specific: %d rather than %ld", rc, (long)sizeof(transmit_specific_buffer));
    }
  }

  tx_specific_sequence++;
  pthread_mutex_unlock(&tx_spec_mutex);
}

static void new_protocol_receive_specific() {
  int i;
  int xmit;
  pthread_mutex_lock(&rx_spec_mutex);
  memset(receive_specific_buffer, 0, sizeof(receive_specific_buffer));
  xmit = isTransmitting();
  receive_specific_buffer[0] = rx_specific_sequence >> 24;
  receive_specific_buffer[1] = rx_specific_sequence >> 16;
  receive_specific_buffer[2] = rx_specific_sequence >> 8;
  receive_specific_buffer[3] = rx_specific_sequence;
  receive_specific_buffer[4] = n_adc; // number of ADCs

  for (i = 0; i < receivers; i++) {
    // note that for HERMES, receiver[i] is associated with DDC(i) but beyond
    // (that is, ANGELIA, ORION, ORION2, G2) receiver[i] is associated with DDC(i+2)
    int ddc = i;

    if (device == NEW_DEVICE_ANGELIA  || device == NEW_DEVICE_ORION ||
        device == NEW_DEVICE_ORION2 || device == NEW_DEVICE_SATURN) { ddc = 2 + i; }

    //
    // If there is at least one RX which has the dither or random bit set,
    // this bit is set for the corresponding ADC
    //
    receive_specific_buffer[5] |= receiver[i]->dither << receiver[i]->adc; // dither enable
    receive_specific_buffer[6] |= receiver[i]->random << receiver[i]->adc; // random enable

    if (!xmit && !diversity_enabled) {
      // normal RX without diversity
      receive_specific_buffer[7] |= (1 << ddc); // DDC enable
    }

    if (xmit && duplex) {
      // transmitting with duplex
      receive_specific_buffer[7] |= (1 << ddc); // DDC enable
    }

    receive_specific_buffer[17 + (ddc * 6)] = receiver[i]->adc;
    receive_specific_buffer[18 + (ddc * 6)] = ((receiver[i]->sample_rate / 1000) >> 8) & 0xFF;
    receive_specific_buffer[19 + (ddc * 6)] = (receiver[i]->sample_rate / 1000) & 0xFF;
    receive_specific_buffer[22 + (ddc * 6)] = 24;
  }

  if (transmitter->puresignal && xmit) {
    //
    //    Some things are fixed.
    //    the sample rate is always 192.
    //    the DDC for PS_RX_FEEDBACK is always DDC0, and ADC is taken from PS_RX_FEEDBACK
    //    the DDC for PS_TX_FEEDBACK is always DDC1, and the ADC is nadc (ADC1 for HERMES, ADC2 beyond)
    //    dither and random are always off
    //    there are 24 bits per sample
    //
    receive_specific_buffer[17] = receiver[PS_RX_FEEDBACK]->adc; // ADC0 associated with DDC0
    receive_specific_buffer[18] = 0;                             // sample rate MSB
    receive_specific_buffer[19] = 192;                           // sample rate LSB
    receive_specific_buffer[22] = 24;                            // bits per sample
    receive_specific_buffer[23] = n_adc;                         // TX-DAC (last ADC + 1) associated with DDC1
    receive_specific_buffer[24] = 0;                             // sample rate MSB
    receive_specific_buffer[25] = 192;                           // sample rate LSB
    receive_specific_buffer[26] = 24;                            // bits per sample
    receive_specific_buffer[1363] = 0x02;                        // sync DDC1 to DDC0
    receive_specific_buffer[7] |= 1;                             // enable  DDC0
  }

  if (diversity_enabled && !xmit) {
    //
    //    Some things are fixed.
    //    We always use DDC0 for the signals from ADC0, and DDC1 for the signals from ADC1
    //    The sample rate of both DDCs is that of receiver[0].
    //    Boths ADCs take the dither/random setting from receiver[0]
    //
    receive_specific_buffer[5] |= receiver[0]->dither;                             // dither DDC0: take value from RX1
    receive_specific_buffer[5] |= (receiver[0]->dither) << 1;                      // dither DDC1: take value from RX1
    receive_specific_buffer[6] |= receiver[0]->random;                             // random DDC0: take value from RX1
    receive_specific_buffer[6] |= (receiver[0]->random) << 1;                      // random DDC1: take value from RX1
    receive_specific_buffer[17] = 0;                                               // ADC0 associated with DDC0
    receive_specific_buffer[18] = ((receiver[0]->sample_rate / 1000) >> 8) & 0xFF; // sample rate MSB
    receive_specific_buffer[19] = (receiver[0]->sample_rate / 1000) & 0xFF;        // sample rate LSB
    receive_specific_buffer[22] = 24;                                              // bits per sample
    receive_specific_buffer[23] = 1;                                               // ADC1 associated with DDC1
    receive_specific_buffer[24] = ((receiver[0]->sample_rate / 1000) >> 8) & 0xFF; // sample rate MSB
    receive_specific_buffer[25] = (receiver[0]->sample_rate / 1000) & 0xFF;;       // sample rate LSB
    receive_specific_buffer[26] = 24;                                              // bits per sample
    receive_specific_buffer[1363] = 0x02;                                          // sync DDC1 to DDC0
    receive_specific_buffer[7] = 1;                                                // enable  DDC0 but disable all others
  }

  //t_print("new_protocol_receive_specific: %s:%d enable=%02X\n",inet_ntoa(receiver_addr.sin_addr),ntohs(receiver_addr.sin_port),receive_specific_buffer[7]);

  if (have_saturn_xdma) {
#ifdef SATURN
    saturn_handle_ddc_specific(false, receive_specific_buffer);
#endif
  } else {
    int rc;

    if ((rc = sendto(data_socket, receive_specific_buffer, sizeof(receive_specific_buffer), 0,
                     (struct sockaddr * )&receiver_addr, receiver_addr_length)) < 0) {
      t_perror("sendto socket failed for receive_specific:");
      exit(1);
    }

    if (rc != sizeof(receive_specific_buffer)) {
      t_print("sendto socket for receive_specific: %d rather than %ld", rc, (long)sizeof(receive_specific_buffer));
    }
  }

  rx_specific_sequence++;
  update_action_table();
  pthread_mutex_unlock(&rx_spec_mutex);
}

static void new_protocol_start() {
  new_protocol_transmit_specific();
  new_protocol_receive_specific();
  new_protocol_timer_thread_id = g_thread_new( "P2 task", new_protocol_timer_thread, NULL);
}

//
// Function available to e.g. rigctl to stop the protocol
//
void new_protocol_menu_stop() {
  fd_set fds;
  struct timeval tv;
  char *buffer;
  running = 0;
  //
  // Wait 50 msec so we know that the TX IQ and RX audio
  // threads block on the semaphore. Then, post the semaphores
  // such that the threads can check "running" and terminate
  //
  usleep(50000);
#ifdef __APPLE__
  sem_post(txiq_sem);
  sem_post(rxaudio_sem);
#else
  sem_post(&txiq_sem);
  sem_post(&rxaudio_sem);
#endif
  g_thread_join(new_protocol_rxaudio_thread_id);
  g_thread_join(new_protocol_txiq_thread_id);
#ifdef __APPLE__
  sem_close(txiq_sem);
  sem_close(rxaudio_sem);
#else
  sem_destroy(&txiq_sem);
  sem_destroy(&rxaudio_sem);
#endif

  if (!have_saturn_xdma) {
    g_thread_join(new_protocol_thread_id);
  }

  g_thread_join(new_protocol_timer_thread_id);
  new_protocol_high_priority();
  // let the FPGA rest a while
  usleep(200000); // 200 ms

  if (!have_saturn_xdma) {
    //
    // drain all data that might still wait in the data_socket.
    // (use select() and read until nothing is left)
    //
    FD_ZERO(&fds);
    FD_SET(data_socket, &fds);
    tv.tv_usec = 50000;
    tv.tv_sec = 0;
    buffer = malloc(NET_BUFFER_SIZE);

    while (select(data_socket + 1, &fds, NULL, NULL, &tv) > 0) {
      recvfrom(data_socket, buffer, NET_BUFFER_SIZE, 0, (struct sockaddr*)&addr, &length);
    }

    free(buffer);
  }
}

//
// Function available e.g. to rigctl to (re-) start the new protocol
//
void new_protocol_menu_start() {
  //
  // reset sequence numbers, action table, etc.
  //
  high_priority_sequence = 0;
  rx_specific_sequence = 0;
  tx_specific_sequence = 0;
  highprio_rcvd_sequence = 0;
  micsamples_sequence = 0;
  audio_sequence = 0;
  tx_iq_sequence = 0;
  memset(rxcase, 0, sizeof(rxcase));
  memset(rxid, 0, sizeof(rxid));
  memset(ddc_sequence, 0, sizeof(ddc_sequence));
  update_action_table();

  //
  // Mark all buffers free.
  //
  if (have_saturn_xdma) {
#ifdef SATURN
    saturn_free_buffers();
#endif
  } else {
    mybuffer *mybuf = buflist;

    while (mybuf) {
      mybuf->free = 1;
      mybuf = mybuf->next;
    }
  }

  running = 1;
#ifdef __APPLE__
  txiq_sem = apple_sem(0);
  rxaudio_sem = apple_sem(0);
#else
  (void)sem_init(&txiq_sem, 0, 0); // check return value!
  (void)sem_init(&rxaudio_sem, 0, 0); // check return value!
#endif
  new_protocol_rxaudio_thread_id = g_thread_new( "P2 SPKR", new_protocol_rxaudio_thread, NULL);
  new_protocol_txiq_thread_id = g_thread_new( "P2 TXIQ", new_protocol_txiq_thread, NULL);

  if (!have_saturn_xdma) {
    new_protocol_thread_id = g_thread_new( "P2 main", new_protocol_thread, NULL);
  }

  new_protocol_general();
  new_protocol_start();
  new_protocol_high_priority();
}

static gpointer new_protocol_rxaudio_thread(gpointer data) {
  int nptr;
  unsigned char audiobuffer[260];

  //
  // Ideally, a RX audio buffer with 64 samples is sent every 1333 usecs.
  // We thus wait until we have 64 samples, and then send a packet
  // (in network mode) or start DMA (in xdma mode).
  // After sending a packet in network mode, wait 1000 usecs before
  // attempting to send the next one.
  //
  while (running) {
#ifdef __APPLE__
    sem_wait(rxaudio_sem);
#else
    sem_wait(&rxaudio_sem);
#endif

    if (!running) { break; }

    nptr = rxaudio_outptr + 256;

    if (nptr >= RXAUDIORINGBUFLEN) { nptr = 0; }

    if (rxaudio_drain) {
      rxaudio_outptr = nptr;
      continue;
    }

    audiobuffer[0] = audio_sequence >> 24;
    audiobuffer[1] = audio_sequence >> 16;
    audiobuffer[2] = audio_sequence >> 8;
    audiobuffer[3] = audio_sequence;
    audio_sequence++;
    memcpy(&audiobuffer[4], &RXAUDIORINGBUF[rxaudio_outptr], 256);
    MEMORY_BARRIER;
    rxaudio_outptr = nptr;

    if (have_saturn_xdma) {
#ifdef SATURN
      saturn_handle_speaker_audio(audiobuffer);
#endif
    } else {
      int rc = sendto(data_socket, audiobuffer, sizeof(audiobuffer), 0, (struct sockaddr*)&audio_addr, audio_addr_length);

      if (rc != sizeof(audiobuffer)) {
        t_print("sendto socket failed for %ld bytes of audio: %d\n", (long)sizeof(audiobuffer), rc);
      }

      usleep(1000);
    }
  }

  return NULL;
}

static gpointer new_protocol_txiq_thread(gpointer data) {
  int nptr;
  unsigned char iqbuffer[1444];

  //
  // Ideally, a TX IQ buffer with 240 sample is sent every 1250 usecs.
  // We thus wait until we have 240 samples, and then send
  // a packet (in network mode) or start DMA (in xdma mode).
  // After sending a packet in network mode, take care that
  // after sending a packet, there is a delay of 1000 usec before
  // sending the next one.
  //
  while (running) {
#ifdef __APPLE__
    sem_wait(txiq_sem);
#else
    sem_wait(&txiq_sem);
#endif

    if (!running) { break; }

    iqbuffer[0] = tx_iq_sequence >> 24;
    iqbuffer[1] = tx_iq_sequence >> 16;
    iqbuffer[2] = tx_iq_sequence >> 8;
    iqbuffer[3] = tx_iq_sequence;
    tx_iq_sequence++;
    nptr = txiq_outptr + 1440;

    if (nptr >= TXIQRINGBUFLEN) { nptr = 0; }

    memcpy(&iqbuffer[4], &TXIQRINGBUF[txiq_outptr], 1440);
    MEMORY_BARRIER;
    txiq_outptr = nptr;

    if (have_saturn_xdma) {
#ifdef SATURN
      saturn_handle_duc_iq(false, iqbuffer);
#endif
    } else {
      if (sendto(data_socket, iqbuffer, sizeof(iqbuffer), 0, (struct sockaddr * )&iq_addr, iq_addr_length) < 0) {
        t_perror("sendto socket failed for iq:");
        exit(1);
      }

      usleep(1000);
    }
  }

  return NULL;
}

static gpointer new_protocol_thread(gpointer data) {
  int ddc;
  short sourceport;
  unsigned char *buffer;
  int bytesread;
  mybuffer *mybuf;
  t_print("new_protocol_thread\n");

  //
  // This thread should do as little work as possible and avoid any blocking.
  // Ideally, all data is just copied into ring buffers, and other threads
  // then take care of processing the data. At least, this should apply to the
  // DDC-IQ and Microphone packets since they eventually get stuck in WDSP
  // (fexchange calls).
  //
  while (running) {
    mybuf = get_my_buffer();
    buffer = mybuf->buffer;
    bytesread = recvfrom(data_socket, buffer, NET_BUFFER_SIZE, 0, (struct sockaddr*)&addr, &length);

    if (!running) {
      //
      // When leaving piHPSDR, it may happen that the protocol has been stopped while
      // we were doing "recvfrom". In this case, we want to let the main
      // thread terminate gracefully, including writing the props files.
      //
      mybuf->free = 1;
      break;
    }

    if (bytesread < 0) {
      t_perror("recvfrom socket failed for new_protocol_thread:");
      exit(-1);
    }

    sourceport = ntohs(addr.sin_port);

    //t_print("new_protocol_thread: recvd %d bytes on port %d\n",bytesread,sourceport);

    switch (sourceport) {
    case RX_IQ_TO_HOST_PORT_0:
    case RX_IQ_TO_HOST_PORT_1:
    case RX_IQ_TO_HOST_PORT_2:
    case RX_IQ_TO_HOST_PORT_3:
    case RX_IQ_TO_HOST_PORT_4:
    case RX_IQ_TO_HOST_PORT_5:
    case RX_IQ_TO_HOST_PORT_6:
    case RX_IQ_TO_HOST_PORT_7:
      ddc = sourceport - RX_IQ_TO_HOST_PORT_0;
      saturn_post_iq_data(ddc, mybuf);
      break;

    case COMMAND_RESPONSE_TO_HOST_PORT:
      //
      // Ignore these packets silently. They occur when
      // flashing a new firmware using the new protocol
      // programmer. But this should be done in a separate
      // program.
      //
      mybuf->free = 1;
      break;

    case HIGH_PRIORITY_TO_HOST_PORT:
      saturn_post_high_priority(mybuf);
      break;

    case MIC_LINE_TO_HOST_PORT:
      saturn_post_micaudio(bytesread, mybuf);
      break;

    default:
      t_print("new_protocol_thread: Unknown port %d\n", sourceport);
      mybuf->free = 1;
      break;
    }
  }

  return NULL;
}

static gpointer high_priority_thread(gpointer data) {
  t_print("high_priority_thread\n");

  while (1) {
#ifdef __APPLE__
    sem_post(high_priority_sem_ready);
    sem_wait(high_priority_sem_buffer);
#else
    sem_post(&high_priority_sem_ready);
    sem_wait(&high_priority_sem_buffer);
#endif
    process_high_priority();
    high_priority_buffer->free = 1;
  }

  return NULL;
}

static gpointer mic_line_thread(gpointer data) {
  t_print("mic_line_thread\n");
  mybuffer *mybuf;
  int nptr;

  //
  // Ideally, a mic sample buffer with 64 samples arrives
  // every 1333 usec, but they may come in bursts
  //
  while (1) {
#ifdef __APPLE__
    sem_wait(mic_line_sem);
#else
    sem_wait(&mic_line_sem);
#endif
    nptr = mic_outptr + 1;

    if (nptr >= MICRINGBUFLEN) { nptr = 0; }

    mybuf = (mybuffer *) mic_line_buffer[mic_outptr];
    MEMORY_BARRIER;
    mic_outptr = nptr;

    // This can happen when restarting the protocol
    if (mybuf->free) { continue; }

    process_mic_data(mybuf->buffer);
    mybuf->free = 1;
  }

  return NULL;
}

//
// Despite the name, these "saturn post" routines are
// also used from within the new_protocol_thread
// to avoid code duplication. Their name stems from the
// fact that Rick first wrote them to support the XDMA
// interface.
//
void saturn_post_high_priority(mybuffer *buffer) {
#ifdef __APPLE__
  sem_wait(high_priority_sem_ready);
#else
  sem_wait(&high_priority_sem_ready);
#endif
  high_priority_buffer = buffer;
#ifdef __APPLE__
  sem_post(high_priority_sem_buffer);
#else
  sem_post(&high_priority_sem_buffer);
#endif
}

void saturn_post_micaudio(int bytesread, mybuffer *mybuf) {
  if (!running) {
    mybuf->free = 1;
    return;
  }

  if (mic_count < 0) {
    mic_count++;
    mybuf->free = 1;
    return;
  }

  int nptr = mic_inptr + 1;

  if (nptr >= MICRINGBUFLEN) { nptr = 0; }

  if (nptr != mic_outptr) {
    mic_line_buffer[mic_inptr] = mybuf;
    MEMORY_BARRIER;
#ifdef __APPLE__
    sem_post(mic_line_sem);
#else
    sem_post(&mic_line_sem);
#endif
    mic_inptr = nptr;
  } else {
    t_print("%s: buffer overflow.\n", __FUNCTION__);
    mybuf->free = 1;
    // skip 16 mic buffers (21 msec)
    mic_count = -16;
  }
}

void saturn_post_iq_data(int ddc, mybuffer *mybuf) {
  if (ddc < 0 || ddc >= MAX_DDC) {
    t_print("%s: invalid DDC(%d) seen!\n", __FUNCTION__, ddc);
    mybuf->free = 1;
    return;
  }

  if (!running) {
    mybuf->free = 1;
    return;
  }

  if (iq_count[ddc] < 0) {
    iq_count[ddc]++;
    mybuf->free = 1;
    return;
  }

  //
  // Check sequence HERE
  //
  unsigned char *buffer = mybuf->buffer;
  long sequence = ((buffer[0] & 0xFF) << 24) + ((buffer[1] & 0xFF) << 16) + ((buffer[2] & 0xFF) << 8)
                  + (buffer[3] & 0xFF);

  if (ddc_sequence[ddc] != sequence) {
    t_print("%s: DDC(%d) sequence error: expected %ld got %ld\n", __FUNCTION__, ddc, ddc_sequence[ddc], sequence);
    sequence_errors++;
  }

  ddc_sequence[ddc] = sequence + 1;
  int iptr = iq_inptr[ddc];
  int nptr = iptr + 1;

  if (nptr >= RXIQRINGBUFLEN) { nptr = 0; }

  if (nptr != iq_outptr[ddc]) {
    iq_buffer[ddc][iptr] = mybuf;
    MEMORY_BARRIER;
    iq_inptr[ddc] = nptr;
#ifdef __APPLE__
    sem_post(iq_sem[ddc]);
#else
    sem_post(&iq_sem[ddc]);
#endif
  } else {
    t_print("%s: DDC(%d) buffer overflow.\n", __FUNCTION__, ddc);
    mybuf->free = 1;
    // skip 128 incoming buffers
    iq_count[ddc] = -128;
  }
}

static gpointer iq_thread(gpointer data) {
  int ddc = GPOINTER_TO_INT(data);
  //
  // TEMPORARY: additional sequence check here
  //
  int nptr, optr;
  long sequence;
  long expected_sequence = 0;
  volatile mybuffer *mybuf;
  unsigned char *buffer;
  t_print("iq_thread: ddc=%d\n", ddc);

  //
  // At a regular pace, a buffer with 238 samples arrives
  // every 4960 usec at 48k and every 155 usec at 1536k,
  // but there may be bursts. Using Diversity the rate
  // is twice as high since 2 DDCs are packed into one
  // channel.
  //
  while (1) {
#ifdef __APPLE__
    sem_wait(iq_sem[ddc]);
#else
    sem_wait(&iq_sem[ddc]);
#endif
    optr = iq_outptr[ddc];
    nptr = optr + 1;

    if (nptr >= RXIQRINGBUFLEN) { nptr = 0; }

    mybuf = iq_buffer[ddc][optr];
    MEMORY_BARRIER;
    iq_outptr[ddc] = nptr;

    // This can happen when restarting the protocol
    if (mybuf->free) { continue; }

    buffer = (unsigned char *) mybuf->buffer;
    //
    //  TEMP: perform additional sequence check
    //
    sequence = ((buffer[0] & 0xFF) << 24) + ((buffer[1] & 0xFF) << 16) + ((buffer[2] & 0xFF) << 8) + (buffer[3] & 0xFF);

    if (expected_sequence == 0) { expected_sequence = sequence; }

    if (sequence != expected_sequence) {
      t_print("%s: DDC(%d) sequence error: expected %ld got %ld\n", __FUNCTION__, ddc, expected_sequence, sequence);
      sequence_errors++;
    }

    expected_sequence = sequence + 1;

    //
    //  Now comes the action table:
    //  for each DDC we have set up which action to be taken
    //  (and, possibly, for which receiver)
    //
    switch (rxcase[ddc]) {
    case RXACTION_SKIP:
      break;

    case RXACTION_NORMAL:
      process_iq_data(buffer, receiver[rxid[ddc]]);
      break;

    case RXACTION_PS:
      process_ps_iq_data(buffer);
      break;

    case RXACTION_DIV:
      process_div_iq_data(buffer);
      break;
    }

    mybuf->free = 1;
  }

  return NULL;
}

static void process_iq_data(unsigned char *buffer, RECEIVER *rx) {
  int b;
  int leftsample;
  int rightsample;
  double leftsampledouble;
  double rightsampledouble;
  int samplesperframe = ((buffer[14] & 0xFF) << 8) + (buffer[15] & 0xFF);
#ifdef P2IQDEBUG
  long long timestamp =
    ((long long)(buffer[4] & 0xFF) << 56)
    + ((long long)(buffer[5] & 0xFF) << 48)
    + ((long long)(buffer[6] & 0xFF) << 40)
    + ((long long)(buffer[7] & 0xFF) << 32)
    + ((long long)(buffer[8] & 0xFF) << 24)
    + ((long long)(buffer[9] & 0xFF) << 16)
    + ((long long)(buffer[10] & 0xFF) << 8)
    + ((long long)(buffer[11] & 0xFF)   );
  int bitspersample = ((buffer[12] & 0xFF) << 8) + (buffer[13] & 0xFF);
  t_print("%s: rx=%d bitspersample=%d samplesperframe=%d\n", __FUNCTION__, rx->id, bitspersample, samplesperframe);
#endif
  b = 16;
  int i;

  for (i = 0; i < samplesperframe; i++) {
    leftsample   = (int)((signed char) buffer[b++]) << 16;
    leftsample  |= (int)((((unsigned char)buffer[b++]) << 8) & 0xFF00);
    leftsample  |= (int)((unsigned char)buffer[b++] & 0xFF);
    rightsample  = (int)((signed char)buffer[b++]) << 16;
    rightsample |= (int)((((unsigned char)buffer[b++]) << 8) & 0xFF00);
    rightsample |= (int)((unsigned char)buffer[b++] & 0xFF);
    // The "obscure" constant 1.1920928955078125E-7 is 1/(2^23)
    leftsampledouble = (double)leftsample * 1.1920928955078125E-7;
    rightsampledouble = (double)rightsample * 1.1920928955078125E-7;
    add_iq_samples(rx, leftsampledouble, rightsampledouble);
  }
}

//
// This is the same as process_ps_iq_data except that add_div_iq_samples is called
// at the end
//
static void process_div_iq_data(unsigned char*buffer) {
  int b;
  int leftsample0;
  int rightsample0;
  double leftsampledouble0;
  double rightsampledouble0;
  int leftsample1;
  int rightsample1;
  double leftsampledouble1;
  double rightsampledouble1;
  int samplesperframe = ((buffer[14] & 0xFF) << 8) + (buffer[15] & 0xFF);
#ifdef P2IQDEBUG
  long long timestamp =
    ((long long)(buffer[4] & 0xFF) << 56)
    + ((long long)(buffer[5] & 0xFF) << 48)
    + ((long long)(buffer[6] & 0xFF) << 40)
    + ((long long)(buffer[7] & 0xFF) << 32)
    + ((long long)(buffer[8] & 0xFF) << 24)
    + ((long long)(buffer[9] & 0xFF) << 16)
    + ((long long)(buffer[10] & 0xFF) << 8)
    + ((long long)(buffer[11] & 0xFF)    );
  int bitspersample = ((buffer[12] & 0xFF) << 8) + (buffer[13] & 0xFF);
  t_print("%s: rx=%d bitspersample=%d samplesperframe=%d\n", __FUNCTION__, rx->id, bitspersample, samplesperframe);
#endif
  b = 16;
  int i;

  for (i = 0; i < samplesperframe; i += 2) {
    leftsample0   = (int)((signed char) buffer[b++]) << 16;
    leftsample0  |= (int)((((unsigned char)buffer[b++]) << 8) & 0xFF00);
    leftsample0  |= (int)((unsigned char)buffer[b++] & 0xFF);
    rightsample0  = (int)((signed char)buffer[b++]) << 16;
    rightsample0 |= (int)((((unsigned char)buffer[b++]) << 8) & 0xFF00);
    rightsample0 |= (int)((unsigned char)buffer[b++] & 0xFF);
    leftsampledouble0 = (double)leftsample0 * 1.1920928955078125E-7;
    rightsampledouble0 = (double)rightsample0 * 1.1920928955078125E-7;
    leftsample1   = (int)((signed char) buffer[b++]) << 16;
    leftsample1  |= (int)((((unsigned char)buffer[b++]) << 8) & 0xFF00);
    leftsample1  |= (int)((unsigned char)buffer[b++] & 0xFF);
    rightsample1  = (int)((signed char)buffer[b++]) << 16;
    rightsample1 |= (int)((((unsigned char)buffer[b++]) << 8) & 0xFF00);
    rightsample1 |= (int)((unsigned char)buffer[b++] & 0xFF);
    leftsampledouble1 = (double)leftsample1 * 1.1920928955078125E-7;
    rightsampledouble1 = (double)rightsample1 * 1.1920928955078125E-7;
    add_div_iq_samples(receiver[0], leftsampledouble0, rightsampledouble0, leftsampledouble1, rightsampledouble1);

    //
    // if both receivers share the sample rate, we can feed data to RX2
    //
    if (receivers > 1 && (receiver[0]->sample_rate == receiver[1]->sample_rate)) {
      add_iq_samples(receiver[1], leftsampledouble1, rightsampledouble1);
    }
  }
}

static void process_ps_iq_data(unsigned char *buffer) {
  int samplesperframe;
  int b;
  int leftsample0;
  int rightsample0;
  double leftsampledouble0;
  double rightsampledouble0;
  int leftsample1;
  int rightsample1;
  double leftsampledouble1;
  double rightsampledouble1;
  samplesperframe = ((buffer[14] & 0xFF) << 8) + (buffer[15] & 0xFF);
#ifdef P2IQDEBUG
  long long timestamp =
    ((long long)(buffer[4] & 0xFF) << 56)
    + ((long long)(buffer[5] & 0xFF) << 48)
    + ((long long)(buffer[6] & 0xFF) << 40)
    + ((long long)(buffer[7] & 0xFF) << 32)
    + ((long long)(buffer[8] & 0xFF) << 24)
    + ((long long)(buffer[9] & 0xFF) << 16)
    + ((long long)(buffer[10] & 0xFF) << 8)
    + ((long long)(buffer[11] & 0xFF)   );
  int bitspersample = ((buffer[12] & 0xFF) << 8) + (buffer[13] & 0xFF);
  t_print("%s: rx=%d bitspersample=%d samplesperframe=%d\n", __FUNCTION__, rx->id, bitspersample, samplesperframe);
#endif
  b = 16;
  int i;

  for (i = 0; i < samplesperframe; i += 2) {
    leftsample0   = (int)((signed char) buffer[b++]) << 16;
    leftsample0  |= (int)((((unsigned char)buffer[b++]) << 8) & 0xFF00);
    leftsample0  |= (int)((unsigned char)buffer[b++] & 0xFF);
    rightsample0  = (int)((signed char)buffer[b++]) << 16;
    rightsample0 |= (int)((((unsigned char)buffer[b++]) << 8) & 0xFF00);
    rightsample0 |= (int)((unsigned char)buffer[b++] & 0xFF);
    leftsampledouble0 = (double)leftsample0 * 1.1920928955078125E-7;
    rightsampledouble0 = (double)rightsample0 * 1.1920928955078125E-7;
    leftsample1   = (int)((signed char) buffer[b++]) << 16;
    leftsample1  |= (int)((((unsigned char)buffer[b++]) << 8) & 0xFF00);
    leftsample1  |= (int)((unsigned char)buffer[b++] & 0xFF);
    rightsample1  = (int)((signed char)buffer[b++]) << 16;
    rightsample1 |= (int)((((unsigned char)buffer[b++]) << 8) & 0xFF00);
    rightsample1 |= (int)((unsigned char)buffer[b++] & 0xFF);
    leftsampledouble1 = (double)leftsample1 * 1.1920928955078125E-7;
    rightsampledouble1 = (double)rightsample1 * 1.1920928955078125E-7;
    add_ps_iq_samples(transmitter, leftsampledouble1, rightsampledouble1, leftsampledouble0, rightsampledouble0);
    //t_print("%06x,%06x %06x,%06x\n",leftsample0,rightsample0,leftsample1,rightsample1);
  }
}

static void process_high_priority() {
  long sequence;
  int previous_ptt;
  int previous_dot;
  int previous_dash;
  const unsigned char *buffer = high_priority_buffer->buffer;
  sequence = ((buffer[0] & 0xFF) << 24) + ((buffer[1] & 0xFF) << 16) + ((buffer[2] & 0xFF) << 8) + (buffer[3] & 0xFF);

  if (sequence != highprio_rcvd_sequence) {
    t_print("HighPrio SeqErr Expected=%ld Seen=%ld\n", highprio_rcvd_sequence, sequence);
    highprio_rcvd_sequence = sequence;
    sequence_errors++;
  }

  highprio_rcvd_sequence++;
  previous_ptt = local_ptt;
  previous_dot = dot;
  previous_dash = dash;
  local_ptt = buffer[4] & 0x01;

  //
  // Do this as fast as possible in case of a RX/TX  transition
  // induced by the radio (in case different RX/TX settings
  // are valid for Ant1/2/3)
  //
  if (previous_ptt == 0 && local_ptt == 1) {
    new_protocol_high_priority();
  }

  dot = (buffer[4] >> 1) & 0x01;
  dash = (buffer[4] >> 2) & 0x01;
  pll_locked = (buffer[4] >> 4) & 0x01;
  adc_overload = buffer[5] & 0x01;
  exciter_power = ((buffer[6] & 0xFF) << 8) | (buffer[7] & 0xFF);
  alex_forward_power = ((buffer[14] & 0xFF) << 8) | (buffer[15] & 0xFF);
  alex_reverse_power = ((buffer[22] & 0xFF) << 8) | (buffer[23] & 0xFF);
  //
  //  calculate moving averages of fwd and rev voltages to have a correct SWR
  //  at the edges of an RF pulse. Otherwise a false trigger of the SWR
  //  protection may occur. Note that during TX, a HighPrio package from the radio
  //  is sent every milli-second.
  //  This exponential average means that the power drops to 1 percent within 16 hits
  //  (at most 16 msec).
  //
  alex_forward_power_average = (alex_forward_power + 3 * alex_forward_power_average) >> 2;
  alex_reverse_power_average = (alex_reverse_power + 3 * alex_reverse_power_average) >> 2;
  supply_volts = ((buffer[49] & 0xFF) << 8) | (buffer[50] & 0xFF);

  // Stops CAT cw transmission if radio reports "CW action"
  if (dash || dot) {
    CAT_cw_is_active = 0;
    cw_key_hit = 1;
  }

  if (!cw_keyer_internal) {
    if (dash != previous_dash) { keyer_event(0, dash); }

    if (dot  != previous_dot ) { keyer_event(1, dot ); }
  }

  if (previous_ptt != local_ptt) {
    g_idle_add(ext_mox_update, GINT_TO_POINTER(local_ptt));
  }
}

static void process_mic_data(unsigned char *buffer) {
  long sequence;
  int b;
  int i;
  float fsample;
  sequence = ((buffer[0] & 0xFF) << 24) + ((buffer[1] & 0xFF) << 16) + ((buffer[2] & 0xFF) << 8) + (buffer[3] & 0xFF);

  if (sequence != micsamples_sequence) {
    t_print("MicSample SeqErr Expected=%ld Seen=%ld\n", micsamples_sequence, sequence);
    sequence_errors++;
  }

  micsamples_sequence = sequence + 1;
  b = 4;

  for (i = 0; i < MIC_SAMPLES; i++) {
    short sample = (short)(buffer[b++] << 8);
    sample |= (short) (buffer[b++] & 0xFF);

    //
    // If PTT comes from the radio, possibly use audio from BOTH sources
    // we just add on since in most cases, only one souce will be "active"
    //
    if (local_ptt) {
      fsample = (float) sample * 0.00003051;

      if (transmitter->local_microphone) { fsample +=  audio_get_next_mic_sample(); }
    } else {
      fsample = transmitter->local_microphone ? audio_get_next_mic_sample() : (float) sample * 0.00003051;
    }

    add_mic_sample(transmitter, fsample);
  }
}

void new_protocol_cw_audio_samples(short left_audio_sample, short right_audio_sample) {
  int txmode = get_tx_mode();

  if (isTransmitting() && (txmode == modeCWU || txmode == modeCWL)) {
    //
    // Only process samples if transmitting in CW
    //
    pthread_mutex_lock(&send_rxaudio_mutex);

    if (rxaudio_count < 0) {
      rxaudio_count++;
      pthread_mutex_unlock(&send_rxaudio_mutex);
      return;
    }

    if (!rxaudio_flag) {
      //
      // First time we arrive here after a RX->TX(CW) transition:
      // set the "drain" flag, wait 10 msec, clear it
      // This should drain the audio ring buffer, to achieve
      // minimum latency for the CW side tone.
      //
      rxaudio_drain = 1;
      usleep(10000);
      rxaudio_drain = 0;
      rxaudio_flag = 1;
    }

    int iptr = rxaudio_inptr + 4 * rxaudio_count;
    RXAUDIORINGBUF[iptr++] = left_audio_sample >> 8;
    RXAUDIORINGBUF[iptr++] = left_audio_sample;
    RXAUDIORINGBUF[iptr++] = right_audio_sample >> 8;
    RXAUDIORINGBUF[iptr++] = right_audio_sample;
    rxaudio_count++;

    if (rxaudio_count >= 64) {
      int nptr = rxaudio_inptr + 256;

      if (nptr >= RXAUDIORINGBUFLEN) { nptr = 0; }

      if (nptr != rxaudio_outptr) {
        rxaudio_inptr = nptr;
#ifdef __APPLE__
        sem_post(rxaudio_sem);
#else
        sem_post(&rxaudio_sem);
#endif
        rxaudio_count = 0;
      } else {
        t_print("%s: buffer overflow\n", __FUNCTION__);
        // skip some audio samples
        rxaudio_count = -4096;
      }
    }

    pthread_mutex_unlock(&send_rxaudio_mutex);
  }
}

void new_protocol_audio_samples(RECEIVER *rx, short left_audio_sample, short right_audio_sample) {
  int txmode = get_tx_mode();

  //
  // Only process samples if NOT transmitting in CW
  //
  if (isTransmitting() && (txmode == modeCWU || txmode == modeCWL)) { return; }

  pthread_mutex_lock(&send_rxaudio_mutex);

  if (rxaudio_count < 0) {
    rxaudio_count++;
    pthread_mutex_unlock(&send_rxaudio_mutex);
    return;
  }

  if (rxaudio_flag) {
    //
    // First time we arrive here after a TX(CW)->RX transition:
    // set the "drain" flag, wait 10 msec, clear it
    // This should drain the audio ring buffer.
    //
    rxaudio_drain = 1;
    usleep(10000);
    rxaudio_drain = 0;
    rxaudio_flag = 0;
  }

  int iptr = rxaudio_inptr + 4 * rxaudio_count;
  RXAUDIORINGBUF[iptr++] = left_audio_sample >> 8;
  RXAUDIORINGBUF[iptr++] = left_audio_sample;
  RXAUDIORINGBUF[iptr++] = right_audio_sample >> 8;
  RXAUDIORINGBUF[iptr++] = right_audio_sample;
  rxaudio_count++;

  if (rxaudio_count >= 64) {
    int nptr = rxaudio_inptr + 256;

    if (nptr >= RXAUDIORINGBUFLEN) { nptr = 0; }

    if (nptr != rxaudio_outptr) {
      rxaudio_inptr = nptr;
#ifdef __APPLE__
      sem_post(rxaudio_sem);
#else
      sem_post(&rxaudio_sem);
#endif
      rxaudio_count = 0;
    } else {
      t_print("%s: buffer overflow\n", __FUNCTION__);
      // skip some audio samples
      rxaudio_count = -4096;
    }
  }

  pthread_mutex_unlock(&send_rxaudio_mutex);
}

void new_protocol_iq_samples(int isample, int qsample) {
  if (txiq_count < 0) {
    txiq_count++;
    return;
  }

  int iptr = txiq_inptr + 6 * txiq_count;
  TXIQRINGBUF[iptr++] = isample >> 16;
  TXIQRINGBUF[iptr++] = isample >> 8;
  TXIQRINGBUF[iptr++] = isample;
  TXIQRINGBUF[iptr++] = qsample >> 16;
  TXIQRINGBUF[iptr++] = qsample >> 8;
  TXIQRINGBUF[iptr++] = qsample;
  txiq_count++;

  if (txiq_count >= 240) {
    int nptr = txiq_inptr + 1440;

    if (nptr >= TXIQRINGBUFLEN) { nptr = 0; }

    if (nptr != txiq_outptr) {
      txiq_inptr = nptr;
      txiq_count = 0;
#ifdef __APPLE__
      sem_post(txiq_sem);
#else
      sem_post(&txiq_sem);
#endif
    } else {
      t_print("%s: output buffer overflow\n", __FUNCTION__);
      // skip 4800 samples ( 25 msec @ 192k )
      txiq_count = -4800;
    }
  }
}

void* new_protocol_timer_thread(void* arg) {
  //
  // Periodically send HighPriority as well as General packets.
  // A general packet is, for example,
  // required if the band changes (band->disblePA), and HighPrio
  // packets are necessary at very many instances when changing
  // something in the menus, and then a small delay does no harm
  //
  // Of course, in time-critical situations (RX-TX transition etc.)
  // it is still possible to explicitly send a packet.
  //
  // We send high prio packets every 100 msec
  //         RX spec   packets every 200 msec
  //         TX spec   packets every 200 msec
  //         General   packets every 800 msec
  //
  int cycling = 0;
  usleep(100000);                               // wait for things to settle down

  while (running) {
    cycling++;

    switch (cycling) {
    case 1:
    case 3:
    case 5:
    case 7:
      new_protocol_high_priority();           // every 100 msec
      new_protocol_transmit_specific();       // every 200 msec
      break;

    case 2:
    case 4:
    case 6:
      new_protocol_high_priority();           // every 100 msec
      new_protocol_receive_specific();        // every 200 msec
      break;

    case 8:
      new_protocol_high_priority();           // every 100 msec
      new_protocol_receive_specific();        // every 200 msec
      new_protocol_general();                 // every 800 msec
      cycling = 0;
      break;
    }

    usleep(100000);
  }

  return NULL;
}
