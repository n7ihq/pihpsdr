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

#ifndef _RADIO_H
#define _RADIO_H

#include <stdint.h>

#include "adc.h"
#include "dac.h"
#include "discovered.h"
#include "receiver.h"
#include "transmitter.h"

#define NEW_MIC_IN 0x00
#define NEW_LINE_IN 0x01
#define NEW_MIC_BOOST 0x02
#define NEW_ORION_MIC_PTT_ENABLED 0x00
#define NEW_ORION_MIC_PTT_DISABLED 0x04
#define NEW_ORION_MIC_PTT_RING_BIAS_TIP 0x00
#define NEW_ORION_MIC_PTT_TIP_BIAS_RING 0x08
#define NEW_ORION_MIC_BIAS_DISABLED 0x00
#define NEW_ORION_MIC_BIAS_ENABLED 0x10

#define OLD_MIC_IN 0x00
#define OLD_LINE_IN 0x02
#define OLD_MIC_BOOST 0x01
#define OLD_ORION_MIC_PTT_ENABLED 0x40
#define OLD_ORION_MIC_PTT_DISABLED 0x00
#define OLD_ORION_MIC_PTT_RING_BIAS_TIP 0x00
#define OLD_ORION_MIC_PTT_TIP_BIAS_RING 0x08
#define OLD_ORION_MIC_BIAS_DISABLED 0x00
#define OLD_ORION_MIC_BIAS_ENABLED 0x20

enum {
  PA_1W = 0,
  PA_5W,
  PA_10W,
  PA_30W,
  PA_50W,
  PA_100W,
  PA_200W,
  PA_500W,
  PA_1KW
};

// TOGGLE takes a boolean expression and inverts it, using the numerical values 0 for FALSE and 1 for TRUE
#define TOGGLE(a) a = (a) ? 0 : 1
// NOT takes a boolean value and produces the inverse, using the numerical values 0 for FALSE and 1 for TRUE
#define NOT(a) (a) ? 0 : 1
// SET converts a boolean to an int, using the numerical values 0 for FALSE and 1 for TRUE
#define SET(a) (a) ? 1 : 0

extern DISCOVERED *radio;
extern uint8_t radio_is_remote;

extern GtkWidget *fixed;

extern long long frequency_calibration;

extern char property_path[];

enum {
  NO_FILTER_BOARD = 0,
  ALEX,
  APOLLO,
  CHARLY25,
  N2ADR
};

enum {
 REGION_OTHER=0,
 REGION_UK,
 REGION_WRC15   // 60m band allocation for countries implementing WRC15
};

extern int region;

extern int RECEIVERS;
extern int PS_TX_FEEDBACK;
extern int PS_RX_FEEDBACK;

extern RECEIVER *receiver[];
extern RECEIVER *active_receiver;

extern TRANSMITTER *transmitter;

enum {
  KEYER_STRAIGHT = 0,
  KEYER_MODE_A,
  KEYER_MODE_B
};

enum {
  MIC3P55MM = 0,
  MICXLR
};

enum {
  SAT_NONE,
  SAT_MODE,
  RSAT_MODE
};

extern int sat_mode;

extern int radio_sample_rate;
extern uint8_t iqswap;

extern uint8_t atlas_penelope;
extern uint8_t atlas_clock_source_10mhz;
extern uint8_t atlas_clock_source_128mhz;
extern uint8_t atlas_config;
extern uint8_t atlas_mic_source;
extern uint8_t atlas_janus;

extern uint8_t classE;

extern uint8_t tx_out_of_band;

extern int filter_board;
extern uint8_t pa_enabled;
extern int pa_power;
extern double pa_trim[11];
extern const int pa_power_list[];

extern uint8_t display_zoompan;
extern uint8_t display_sliders;
extern uint8_t display_toolbar;

extern double mic_gain;

extern uint8_t mic_linein;
extern double linein_gain;
extern uint8_t mic_boost;
extern uint8_t mic_bias_enabled;
extern uint8_t mic_ptt_enabled;
extern uint8_t mic_ptt_tip_bias_ring;
extern uint8_t mic_input_xlr;

//
// digital inputs from radio
//
extern uint8_t radio_ptt;
extern uint8_t radio_dash;
extern uint8_t radio_dot;
extern uint8_t radio_io1;  // P1 only
extern uint8_t radio_io2;  // P1 and P2
extern uint8_t radio_io3;  // P1 only
extern uint8_t radio_io4;  // P1 and P2
extern uint8_t radio_io5;  // P2 only
extern uint8_t radio_io6;  // P2 only
extern uint8_t radio_io8;  // P2 only

extern int receivers;

extern ADC adc[2];
extern DAC dac[2];

extern uint8_t locked;

extern int rit_increment;

extern uint8_t duplex;
extern uint8_t mute_rx_while_transmitting;
extern int rx_height;

extern uint8_t cw_keys_reversed;
extern uint8_t cw_keyer_speed;
extern int cw_keyer_mode;
extern uint8_t cw_keyer_weight;
extern uint8_t cw_keyer_spacing;
extern uint8_t cw_keyer_internal;
extern uint8_t cw_keyer_sidetone_volume;
extern uint8_t cw_keyer_ptt_delay;
extern unsigned int cw_keyer_hang_time;
extern unsigned int cw_keyer_sidetone_frequency;
extern uint8_t cw_breakin;

extern uint8_t auto_tune_flag;
extern uint8_t auto_tune_end;

extern int vfo_encoder_divisor;

extern int protocol;
extern int device;
extern uint8_t new_pa_board;
extern uint8_t ozy_software_version;
extern uint8_t mercury_software_version[2];
extern uint8_t penelope_software_version;
extern uint8_t ptt;
extern uint8_t mox;
extern uint8_t tune;
extern uint8_t memory_tune;
extern uint8_t full_tune;

extern uint8_t adc0_overload;
extern uint8_t adc1_overload;
extern uint8_t tx_fifo_underrun;
extern uint8_t tx_fifo_overrun;
extern uint8_t high_swr_seen;
extern uint8_t sequence_errors;

extern unsigned int exciter_power;
extern unsigned int alex_forward_power;
extern unsigned int alex_reverse_power;
extern unsigned int ADC1;
extern unsigned int ADC0;

extern uint8_t split;

extern uint8_t OCtune;
extern unsigned int OCfull_tune_time;
extern unsigned int OCmemory_tune_time;
extern long long tune_timeout;

extern uint8_t analog_meter;
extern int smeter;
extern int alc;

extern int eer_pwm_min;
extern int eer_pwm_max;

extern int tx_filter_low;
extern int tx_filter_high;

extern uint8_t enable_tx_equalizer;
extern int tx_equalizer[4];

extern uint8_t enable_rx_equalizer;
extern int rx_equalizer[4];

extern uint8_t pre_emphasize;

extern uint8_t vox_enabled;
extern double vox_threshold;
extern double vox_hang;
extern uint8_t vox;
extern uint8_t CAT_cw_is_active;
extern uint8_t cw_key_hit;
extern int n_adc;

extern uint8_t diversity_enabled;
extern double div_cos, div_sin;
extern double div_gain, div_phase;

extern uint8_t can_transmit;

extern uint8_t have_rx_gain;         // programmable RX gain available
extern uint8_t have_rx_att;          // step attenuator available -31 ... 0 dB
extern uint8_t have_preamp;          // switchable preamp
extern uint8_t have_alex_att;        // ALEX board does have 0/10/20/30 dB attenuator
extern uint8_t have_saturn_xdma;     // Saturn can use Network or XDMA interface
extern int     rx_gain_calibration;  // used to calibrate the input signal

extern double drive_max;         // maximum value of the drive slider
extern double drive_digi_max;    // maximum value allowed in DIGU/DIGL

extern uint8_t display_warnings;
extern uint8_t display_pacurr;

extern GMutex property_mutex;

extern uint8_t hl2_audio_codec;
extern uint8_t anan10E;

extern uint8_t adc0_filter_bypass;   // Bypass ADC0 filters on receive
extern uint8_t adc1_filter_bypass;   // Bypass ADC1 filters on receiver  (ANAN-7000/8000/G2)
extern uint8_t mute_spkr_amp;        // Mute audio amplifier in radio    (ANAN-7000, G2)

extern unsigned int VFO_WIDTH;
extern unsigned int VFO_HEIGHT;
extern unsigned int METER_WIDTH;
extern unsigned int METER_HEIGHT;
extern unsigned int MENU_WIDTH;
extern uint8_t rx_stack_horizontal;

extern void radio_stop(void);
extern void reconfigure_radio(void);
extern void reconfigure_screen(void);
extern void start_radio(void);
extern void radio_change_receivers(int r);
extern void radio_change_sample_rate(int rate);
extern void set_alex_antennas(void);
extern void tx_vfo_changed(void);
extern void radio_split_toggle(void);
extern void radio_set_split(int v);
extern void setMox(int state);
extern int getMox(void);
extern void setTune(int state);
extern int getTune(void);
extern void vox_changed(int state);
extern double getDrive(void);
extern void setDrive(double d);
extern void calcDriveLevel(void);
extern void calcTuneDriveLevel(void);
extern void setSquelch(RECEIVER *rx);

extern void radio_set_rf_gain(RECEIVER *rx);

extern void set_attenuation(int value);
extern void set_alex_attenuation(int v);

extern int isTransmitting(void);

extern void radioRestoreState(void);
extern void radioSaveState(void);

extern void calculate_display_average(RECEIVER *rx);

extern void radio_change_region(int region);
extern void radio_set_satmode(int mode);

extern void disable_rigctl(void);

#ifdef CLIENT_SERVER
  extern int remote_start(void *data);
#endif

extern uint8_t optimize_for_touchscreen;
extern void my_combo_attach(GtkGrid *grid, GtkWidget *combo, int row, int col, int spanrow, int spancol);
extern int max_band(void);
extern void protocol_run(void);
extern void protocol_stop(void);
extern void protocol_restart(void);
extern void *auto_tune_thread(void * data);

//
// Macro to flag an unimplemented client/server feature
//
#define CLIENT_MISSING if (radio_is_remote) { t_print("%s: TODO: CLIENT/SERVER\n", __FUNCTION__); return; }

//
// Macro for a memory barrier, preventing changing the execution order
// or memory accesses (used for ring buffers)
//
#define MEMORY_BARRIER asm volatile ("" ::: "memory")
#endif
