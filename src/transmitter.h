/* Copyright (C)
* 2017 - John Melton, G0ORX/N6LYT
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

#ifndef _TRANSMITTER_H
#define _TRANSMITTER_H

#include <stdint.h>
#include <gtk/gtk.h>

#define CTCSS_FREQUENCIES 38
extern double ctcss_frequencies[CTCSS_FREQUENCIES];

typedef struct _transmitter {
  uint8_t id;
  uint8_t dac;
  uint8_t displaying;
  uint8_t low_latency;
  uint8_t display_panadapter;
  uint8_t display_waterfall;
  uint8_t use_rx_filter;
  uint8_t out_of_band;
  uint8_t twotone;
  uint8_t puresignal;
  uint8_t feedback;
  uint8_t auto_on;
  uint8_t ctcss_enabled;
  uint8_t tune_use_drive;
  uint8_t do_scale;      // apply TX iq scaling
  uint8_t swr_protection;
  uint8_t display_filled;
  uint8_t local_microphone;
  uint8_t compressor;
  uint8_t alex_antenna;

  int fps;
  int mic_sample_rate;
  int mic_dsp_rate;
  int iq_output_rate;
  int buffer_size;
  int dsp_size;
  int fft_size;
  int pixels;
  int samples;
  int output_samples;
  int filter_low;
  int filter_high;
  int width;
  int height;
  int panadapter_low;
  int panadapter_high;
  int panadapter_step;
  int ctcss;
  int deviation;
  int attenuation;      // Attenuation in the RX ADC0 chain (!) if TX is active
  int drive;            // value of the drive slider
  int tune_drive;
  int drive_level;      // amplitude (0-255) corresponding to "drive"
  int x;
  int y;
  int dialog_x;
  int dialog_y;

  double *mic_input_buffer;
  double *iq_output_buffer;
  double am_carrier_level;
  double drive_scale;   // additional TX iq scaling required
  double drive_iscal;   // inverse of drive_scale
  double compressor_level;
  double fwd;
  double rev;
  double alc;
  double swr;
  double swr_alarm;

  float *pixel_samples;
  guint update_timer_id;
  GMutex display_mutex;

  GtkWidget *dialog;
  GtkWidget *panel;
  GtkWidget *panadapter;


  cairo_surface_t *panadapter_surface;

  char microphone_name[128];

  guint out_of_band_timer_id;

} TRANSMITTER;

extern TRANSMITTER *create_transmitter(int id, int width, int height);

void create_dialog(TRANSMITTER *tx);
void reconfigure_transmitter(TRANSMITTER *tx, int width, int height);

//
// CW pulse shaper variables
//
extern int cw_key_up;
extern int cw_key_down;
extern int cw_not_ready;

extern void tx_set_mode(TRANSMITTER* tx, int m);
extern void tx_set_filter(TRANSMITTER *tx);
extern void transmitter_set_am_carrier_level(TRANSMITTER *tx);
extern void tx_set_pre_emphasize(TRANSMITTER *tx, int state);
extern void transmitter_set_ctcss(TRANSMITTER *tx, int state, int i);

extern void add_mic_sample(TRANSMITTER *tx, float mic_sample);
extern void add_freedv_mic_sample(TRANSMITTER *tx, float mic_sample);

extern void transmitterSaveState(const TRANSMITTER *tx);
extern void transmitter_set_out_of_band(TRANSMITTER *tx);
extern void tx_set_displaying(TRANSMITTER *tx, int state);

extern void tx_set_ps(TRANSMITTER *tx, int state);
extern void tx_set_twotone(TRANSMITTER *tx, int state);

extern void transmitter_set_compressor_level(TRANSMITTER *tx, double level);
extern void transmitter_set_compressor(TRANSMITTER *tx, int state);

extern void tx_set_ps_sample_rate(TRANSMITTER *tx, int rate);
extern void add_ps_iq_samples(TRANSMITTER *tx, double i_sample_0, double q_sample_0, double i_sample_1,
                              double q_sample_1);

extern void cw_hold_key(int state);

extern float sine_generator(int *p1, int *p2, int freq);
#endif


