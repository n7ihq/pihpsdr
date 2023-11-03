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
extern gboolean radio_is_remote;

extern GtkWidget *fixed;

extern long long frequency_calibration;

extern char property_path[];

#define NONE 0

#define ALEX 1
#define APOLLO 2
#define CHARLY25 3
#define N2ADR 4

#define REGION_OTHER 0
#define REGION_UK 1
#define REGION_WRC15 2  // 60m band allocation for countries implementing WRC15

extern int region;

extern int RECEIVERS;
extern int PS_TX_FEEDBACK;
extern int PS_RX_FEEDBACK;

extern RECEIVER *receiver[];
extern RECEIVER *active_receiver;

extern TRANSMITTER *transmitter;

enum {
  PA_DISABLED = 0,
  PA_ENABLED
};

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

extern gint sat_mode;

extern int radio_sample_rate;
extern gboolean iqswap;

#define MAX_BUFFER_SIZE 2048

extern int atlas_penelope;
extern int atlas_clock_source_10mhz;
extern int atlas_clock_source_128mhz;
extern int atlas_config;
extern int atlas_mic_source;
extern int atlas_janus;

extern int classE;

extern int tx_out_of_band;

extern int filter_board;
extern int pa_enabled;
extern int pa_power;
extern double pa_trim[11];
extern const int pa_power_list[];
extern int apollo_tuner;

extern int display_zoompan;
extern int display_sliders;
extern int display_toolbar;

extern double mic_gain;

extern int mic_linein;
extern double linein_gain;
extern int mic_boost;
extern int mic_bias_enabled;
extern int mic_ptt_enabled;
extern int mic_ptt_tip_bias_ring;
extern int mic_input_xlr;

extern int receivers;

extern ADC adc[2];
extern DAC dac[2];

extern int locked;

extern int rit_increment;

extern gboolean duplex;
extern gboolean mute_rx_while_transmitting;
extern gint rx_height;

extern int cw_keys_reversed;
extern int cw_keyer_speed;
extern int cw_keyer_mode;
extern int cw_keyer_weight;
extern int cw_keyer_spacing;
extern int cw_keyer_internal;
extern int cw_keyer_sidetone_volume;
extern int cw_keyer_ptt_delay;
extern int cw_keyer_hang_time;
extern int cw_keyer_sidetone_frequency;
extern int cw_breakin;

extern int vfo_encoder_divisor;

extern int protocol;
extern int device;
extern int new_pa_board;
extern int ozy_software_version;
extern int mercury_software_version;
extern int penelope_software_version;
extern int ptt;
extern int mox;
extern int tune;
extern int memory_tune;
extern int full_tune;
extern int adc_overload;
extern int pll_locked;
extern unsigned int exciter_power;
extern unsigned int average_temperature;
extern unsigned int average_current;
extern unsigned int tx_fifo_underrun;
extern unsigned int tx_fifo_overrun;
extern unsigned int alex_forward_power;
extern unsigned int alex_reverse_power;
extern unsigned int alex_forward_power_average;
extern unsigned int alex_reverse_power_average;
extern unsigned int IO1;
extern unsigned int IO2;
extern unsigned int IO3;
extern unsigned int AIN3;
extern unsigned int AIN4;
extern unsigned int AIN6;
extern int supply_volts;

extern int split;

extern unsigned char OCtune;
extern int OCfull_tune_time;
extern int OCmemory_tune_time;
extern long long tune_timeout;

extern int analog_meter;
extern int smeter;
extern int alc;

extern int eer_pwm_min;
extern int eer_pwm_max;

extern int tx_filter_low;
extern int tx_filter_high;

extern int ctun;

extern int enable_tx_equalizer;
extern int tx_equalizer[4];

extern int enable_rx_equalizer;
extern int rx_equalizer[4];

extern int pre_emphasize;

extern int vox_setting;
extern int vox_enabled;
extern double vox_threshold;
extern double vox_hang;
extern int vox;
extern int CAT_cw_is_active;
extern int cw_key_hit;
extern int n_adc;

extern int diversity_enabled;
extern double div_cos, div_sin;
extern double div_gain, div_phase;

extern int can_transmit;

extern int have_rx_gain;         // programmable RX gain available
extern int have_rx_att;          // step attenuator available -31 ... 0 dB
extern int have_preamp;          // switchable preamp
extern int have_alex_att;        // ALEX board does have 0/10/20/30 dB attenuator
extern int have_saturn_xdma;     // Saturn can use Network or XDMA interface
extern int rx_gain_calibration;  // used to calibrate the input signal

extern double drive_max;         // maximum value of the drive slider
extern double drive_digi_max;    // maximum value allowed in DIGU/DIGL

extern gboolean display_sequence_errors;
extern gboolean display_swr_protection;
extern gint sequence_errors;
extern GMutex property_mutex;

extern int hl2_audio_codec;
extern int anan10E;

extern int adc0_filter_bypass;   // Bypass ADC0 filters on receive
extern int adc1_filter_bypass;   // Bypass ADC1 filters on receiver  (ANAN-7000/8000/G2)
extern int mute_spkr_amp;        // Mute audio amplifier in radio    (ANAN-7000, G2)

extern int VFO_WIDTH;
extern int VFO_HEIGHT;
extern int METER_WIDTH;
extern int METER_HEIGHT;
extern int MENU_WIDTH;
extern int rx_stack_horizontal;

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

extern void disable_rigctl(void);

#ifdef CLIENT_SERVER
  extern int remote_start(void *data);
#endif

extern int optimize_for_touchscreen;
extern void my_combo_attach(GtkGrid *grid, GtkWidget *combo, int row, int col, int spanrow, int spancol);
extern int max_band(void);
extern void protocol_run(void);
extern void protocol_stop(void);
extern void protocol_restart(void);

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
