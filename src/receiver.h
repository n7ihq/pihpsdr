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
#ifndef _RECEIVER_H
#define _RECEIVER_H

#include <gtk/gtk.h>
#ifdef PORTAUDIO
  #include "portaudio.h"
#endif
#ifdef ALSA
  #include <alsa/asoundlib.h>
#endif
#ifdef PULSEAUDIO
  #include <pulse/pulseaudio.h>
  #include <pulse/simple.h>
#endif

enum _audio_t {
  STEREO = 0,
  LEFT,
  RIGHT
};

typedef enum _audio_t audio_t;

typedef struct _receiver {
  gint id;
  GMutex mutex;
  GMutex display_mutex;

  gint ddc;
  gint adc;

  gdouble volume;  // in dB

  int dsp_size;
  int buffer_size;
  int fft_size;
  int low_latency;

  gint agc;
  gdouble agc_gain;
  gdouble agc_slope;
  gdouble agc_hang_threshold;
  gdouble agc_hang;
  gdouble agc_thresh;
  gint fps;
  gint displaying;
  audio_t audio_channel;
  gint sample_rate;
  gint pixels;
  gint samples;
  gint output_samples;
  gdouble *iq_input_buffer;
  gdouble *audio_output_buffer;
  gint audio_index;
  gfloat *pixel_samples;
  gint display_panadapter;
  gint display_waterfall;
  guint update_timer_id;
  gdouble meter;

  gdouble hz_per_pixel;

  gint dither;
  gint random;
  gint preamp;

  //
  // Encodings for "QRM fighters"
  //
  // nr = 0:         No noise reduction
  // nr = 1:         Variable-Leak LMS Algorithm, "NR", "ANR"
  // nr = 2:         Spectral Noise Reduction, "NR2", "AEMNR"
  // nr = 3:         non-standard extension to WDSP
  // nr = 4:         non-standard extension to WDSP
  //
  // nb = 0:         No noise blanker
  // nb = 1:         Preemptive Wideband Blanker, "NB", "ANB"
  // nb = 2:         Interpolating Wideband Blanker, "NB2", "NOB"
  //
  // anf= 0/1:       Automatic notch filter off/on
  // snb= 0/1:       Spectral noise blanker off/on
  //
  gint nb;
  gint nr;
  gint anf;
  gint snb;

  //
  // NR/NR2/ANF: position
  // 0: execute NR/NR2/ANF before AGC
  // 1: execute NR/NR2/ANF after AGC
  //
  int nr_agc;

  //
  // Noise reduction parameters for "NR2"
  // To be modified in the DSP menu.
  //
  //  Gain method: 0=GaussianSpeechLin, 1=GaussianSpeechLog, 2=GammaSpeech
  //  NPE  method: 0=OSMS, 1=MMSE
  //  AE         : Artifact elimination filter on(1)/off(0)
  //
  int nr2_gain_method;
  int nr2_npe_method;
  int nr2_ae;

  //
  // Noise blanker parameters. These parameters have
  // similar meanings for NB/NB2 so we only take one set.
  // To be modified in the DSP menu. Note the "times"
  // are stored internally in seconds, while in the DSP menu they
  // are specified in milli-seconds.
  // The "NB value" nb_thresh, as obtained from the DSP menu,
  // is multiplied with 0.165 merely since this is done in other
  // SDR programs as well.
  // The comments indicate the names of the parameters in the Thetis menu
  // as well as the internal name used in Thetis.
  //
  gdouble nb_tau;       // "Slew",                         NBTransision
  gdouble nb_advtime;   // "Lead",                         NBLead
  gdouble nb_hang;      // "Lag",                          NBLag
  gdouble nb_thresh;    // "Threshold",                    NBThreshold
  gint    nb2_mode;     // NB mode, only NB2
  //
  // nb2_mode = 0:  zero mode
  // nb2_mode = 1:  sample-hold
  // nb2_mode = 2:  mean-hold
  // nb2_mode = 3:  hold-sample
  // nb2_mode = 4:  interpolate

#ifdef EXTNR
  //
  // NR4 parameters
  //
  gdouble nr4_reduction_amount;
  gdouble nr4_smoothing_factor;
  gdouble nr4_whitening_factor;
  gdouble nr4_noise_rescale;
  gdouble nr4_post_filter_threshold;
#endif

  gint alex_antenna;
  gint alex_attenuation;

  gint filter_low;
  gint filter_high;

  gint width;
  gint height;

  GtkWidget *panel;
  GtkWidget *panadapter;
  GtkWidget *waterfall;

  gint panadapter_low;
  gint panadapter_high;
  gint panadapter_step;

  gint waterfall_low;
  gint waterfall_high;
  gint waterfall_automatic;
  cairo_surface_t *panadapter_surface;
  GdkPixbuf *pixbuf;
  gint local_audio;
  gint mute_when_not_active;
  gint audio_device;
  gchar audio_name[128];
#ifdef PORTAUDIO
  PaStream *playstream;
  volatile gint local_audio_buffer_inpt;    // pointer in audio ring-buffer
  volatile gint local_audio_buffer_outpt;   // pointer in audio ring-buffer
  float *local_audio_buffer;
#endif
#ifdef ALSA
  snd_pcm_t *playback_handle;
  snd_pcm_format_t local_audio_format;
  void *local_audio_buffer;        // different formats possible, so void*
#endif
#ifdef PULSEAUDIO
  pa_simple *playstream;
  gboolean output_started;
  float *local_audio_buffer;
#endif
  gint local_audio_buffer_offset;
  GMutex local_audio_mutex;

  gint squelch_enable;
  gdouble squelch;

  gint binaural;

  gint deviation;

  gint64 waterfall_frequency;
  gint waterfall_sample_rate;
  gint waterfall_pan;
  gint waterfall_zoom;

  gint mute_radio;

  gdouble *buffer;
  void *resampler;
  gdouble *resample_buffer;
  gint resample_buffer_size;

  gint zoom;
  gint pan;

  gint x;
  gint y;

  // two variables that implement the new
  // "mute first RX IQ samples after TX/RX transition"
  // feature that is relevant for HermesLite-II and STEMlab
  // (and possibly some other radios)
  //
  guint txrxcount;
  guint txrxmax;


  int display_gradient;
  int display_filled;
  int display_detector_mode;
  int display_average_mode;
  double display_average_time;

} RECEIVER;

extern RECEIVER *create_pure_signal_receiver(int id, int sample_rate, int pixels);
extern RECEIVER *create_receiver(int id, int pixels, int width, int height);
extern void receiver_change_sample_rate(RECEIVER *rx, int sample_rate);
extern void receiver_change_adc(RECEIVER *rx, int adc);
extern void receiver_set_frequency(RECEIVER *rx, long long frequency);
extern void receiver_frequency_changed(RECEIVER *rx);
extern void receiver_mode_changed(RECEIVER *rx);
extern void receiver_filter_changed(RECEIVER *rx);
extern void receiver_vfo_changed(RECEIVER *rx);
extern void receiver_update_zoom(RECEIVER *rx);

extern void set_mode(RECEIVER* rx, int m);
extern void set_filter(RECEIVER *rx);
extern void set_agc(RECEIVER *rx, int agc);
extern void set_offset(RECEIVER *rx, long long offset);
extern void set_deviation(RECEIVER *rx);

extern void add_iq_samples(RECEIVER *rx, double i_sample, double q_sample);
extern void add_div_iq_samples(RECEIVER *rx, double i0, double q0, double i1, double q1);

extern void reconfigure_receiver(RECEIVER *rx, int height);

extern void receiverSaveState(RECEIVER *rx);
extern void receiverRestoreState(RECEIVER *rx);

extern gboolean receiver_button_press_event(GtkWidget *widget, GdkEventButton *event, gpointer data);
extern gboolean receiver_button_release_event(GtkWidget *widget, GdkEventButton *event, gpointer data);
extern gboolean receiver_motion_notify_event(GtkWidget *widget, GdkEventMotion *event, gpointer data);
extern gboolean receiver_scroll_event(GtkWidget *widget, const GdkEventScroll *event, gpointer data);

extern void set_displaying(RECEIVER *rx, int state);

extern void receiver_set_active(RECEIVER *rx);

#ifdef CLIENT_SERVER
  extern void receiver_create_remote(RECEIVER *rx);
  extern void receiver_remote_update_display(RECEIVER *rx);
#endif

#endif
