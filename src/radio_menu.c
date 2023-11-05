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
#include <semaphore.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "main.h"
#include "discovered.h"
#include "new_menu.h"
#include "radio_menu.h"
#include "adc.h"
#include "band.h"
#include "filter.h"
#include "radio.h"
#include "receiver.h"
#include "sliders.h"
#include "new_protocol.h"
#include "old_protocol.h"
#ifdef SOAPYSDR
  #include "soapy_protocol.h"
#endif
#include "actions.h"
#ifdef GPIO
  #include "gpio.h"
#endif
#include "vfo.h"
#include "ext.h"
#ifdef CLIENT_SERVER
  #include "client_server.h"
#endif

static GtkWidget *dialog = NULL;

static GtkWidget *duplex_b;
static GtkWidget *mute_rx_b;

static void cleanup() {
  if (dialog != NULL) {
    GtkWidget *tmp = dialog;
    dialog = NULL;
    gtk_widget_destroy(tmp);
    sub_menu = NULL;
    active_menu  = NO_MENU;
  }
}

static gboolean close_cb () {
  cleanup();
  return TRUE;
}

#ifdef SOAPYSDR
static void rx_gain_element_changed_cb(GtkWidget *widget, gpointer data) {
  ADC *myadc = (ADC *)data;

  if (device == SOAPYSDR_USB_DEVICE) {
    myadc->gain = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(widget));
    soapy_protocol_set_gain_element(receiver[0], (char *)gtk_widget_get_name(widget), myadc->gain);
  }
}

static void tx_gain_element_changed_cb(GtkWidget *widget, gpointer data) {
  if (can_transmit && device == SOAPYSDR_USB_DEVICE) {
    int gain = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(widget));
    soapy_protocol_set_tx_gain_element(transmitter, (char *)gtk_widget_get_name(widget), gain);
  }
}

static void iqswap_cb(GtkWidget *widget, gpointer data) {
  iqswap = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));
}

static void agc_changed_cb(GtkWidget *widget, gpointer data) {
  gboolean agc = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));
  soapy_protocol_set_automatic_gain(active_receiver, agc);

  if (!agc) {
    soapy_protocol_set_gain(active_receiver);
  }
}
#endif

static void calibration_value_changed_cb(GtkWidget *widget, gpointer data) {
  frequency_calibration = (long long)gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(widget));
}

static void rx_gain_calibration_value_changed_cb(GtkWidget *widget, gpointer data) {
  rx_gain_calibration = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(widget));
}

static void vfo_divisor_value_changed_cb(GtkWidget *widget, gpointer data) {
  vfo_encoder_divisor = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(widget));
}

static void ptt_cb(GtkWidget *widget, gpointer data) {
  mic_ptt_enabled = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));
  schedule_transmit_specific();
}

static void ptt_ring_cb(GtkWidget *widget, gpointer data) {
  if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget))) {
    mic_ptt_tip_bias_ring = 0;
  }
  schedule_transmit_specific();
}

static void ptt_tip_cb(GtkWidget *widget, gpointer data) {
  if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget))) {
    mic_ptt_tip_bias_ring = 1;
  }
  schedule_transmit_specific();
}

static void bias_cb(GtkWidget *widget, gpointer data) {
  mic_bias_enabled = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));
  schedule_transmit_specific();
}

static void touchscreen_cb(GtkWidget *widget, gpointer data) {
  optimize_for_touchscreen = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));
}

static void mute_spkr_amp_cb(GtkWidget *widget, gpointer data) {
  mute_spkr_amp = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));
  schedule_high_priority();
}

static void hl2audio_cb(GtkWidget *widget, gpointer data) {
  hl2_audio_codec = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));
}

static void anan10e_cb(GtkWidget *widget, gpointer data) {
  protocol_stop();
  usleep(200000);
  anan10E = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));
  protocol_run();
}

static void split_cb(GtkWidget *widget, gpointer data) {
  int new = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));
  radio_set_split(new);
}

//
// call-able from outside, e.g. toolbar or MIDI, through g_idle_add
//
void setDuplex() {
  if (!can_transmit) { return; }

  if (duplex) {
    // TX is in separate window, also in full-screen mode
    gtk_container_remove(GTK_CONTAINER(fixed), transmitter->panel);
    reconfigure_transmitter(transmitter, display_width / 4, display_height / 2);
    create_dialog(transmitter);
  } else {
    GtkWidget *content = gtk_dialog_get_content_area(GTK_DIALOG(transmitter->dialog));
    gtk_container_remove(GTK_CONTAINER(content), transmitter->panel);
    gtk_widget_destroy(transmitter->dialog);
    transmitter->dialog = NULL;
    int width = full_screen ? screen_width : display_width;
    reconfigure_transmitter(transmitter, width, rx_height);
  }

  g_idle_add(ext_vfo_update, NULL);
}

static void PA_enable_cb(GtkWidget *widget, gpointer data) {
  pa_enabled = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));
  schedule_general();
  schedule_high_priority();
}

static void duplex_cb(GtkWidget *widget, gpointer data) {
  if (isTransmitting()) {
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (duplex_b), duplex);
    return;
  }

  duplex = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));
  setDuplex();
}

static void mute_rx_cb(GtkWidget *widget, gpointer data) {
  mute_rx_while_transmitting = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));
}

static void sat_cb(GtkWidget *widget, gpointer data) {
  sat_mode = gtk_combo_box_get_active(GTK_COMBO_BOX(widget));
  g_idle_add(ext_vfo_update, NULL);
}

void n2adr_oc_settings() {
  //
  // set OC outputs for each band according to the N2ADR board requirements
  // unlike load_filters(), this can be executed outside the GTK queue
  //
  BAND *band;
  band = band_get_band(band160);
  band->OCrx = band->OCtx = 1;
  band = band_get_band(band80);
  band->OCrx = band->OCtx = 66;
  band = band_get_band(band60);
  band->OCrx = band->OCtx = 68;
  band = band_get_band(band40);
  band->OCrx = band->OCtx = 68;
  band = band_get_band(band30);
  band->OCrx = band->OCtx = 72;
  band = band_get_band(band20);
  band->OCrx = band->OCtx = 72;
  band = band_get_band(band17);
  band->OCrx = band->OCtx = 80;
  band = band_get_band(band15);
  band->OCrx = band->OCtx = 80;
  band = band_get_band(band12);
  band->OCrx = band->OCtx = 96;
  band = band_get_band(band10);
  band->OCrx = band->OCtx = 96;
  schedule_high_priority();
}

void load_filters() {
  switch (filter_board) {
  case N2ADR:
    n2adr_oc_settings();
    break;

  case ALEX:
  case APOLLO:
  case CHARLY25:
    // This is most likely not necessary here, but can do no harm
    set_alex_antennas();
    break;

  case NONE:
    break;

  default:
    break;
  }

  //
  // This switches between StepAttenuator slider and CHARLY25 ATT/Preamp checkboxes
  // when the filter board is switched to/from CHARLY25
  //
  att_type_changed();
}

static void filter_cb(GtkWidget *widget, gpointer data) {
  int val = gtk_combo_box_get_active (GTK_COMBO_BOX(widget));

  switch (val) {
  case 0:
  default:
    filter_board = NONE;
    break;

  case 1:
    filter_board = ALEX;
    break;

  case 2:
    filter_board = APOLLO;
    break;

  case 3:
    filter_board = CHARLY25;
    break;

  case 4:
    filter_board = N2ADR;
    break;
  }

  load_filters();
}

static void mic_input_cb(GtkWidget *widget, gpointer data) {
  int val = gtk_combo_box_get_active (GTK_COMBO_BOX(widget));

  switch (val) {
  case 0:
  default:
    mic_input_xlr = MIC3P55MM;
    break;

  case 1:
    mic_input_xlr = MICXLR;
    break;
  }
  schedule_transmit_specific();
}
static void sample_rate_cb(GtkToggleButton *widget, gpointer data) {
  const char *p = gtk_combo_box_text_get_active_text(GTK_COMBO_BOX_TEXT(widget));
  int samplerate;

  //
  // There are so many different possibilities for sample rates, so
  // we just "scanf" from the combobox text entry
  //
  if (sscanf(p, "%d", &samplerate) != 1) { return; }

#ifdef CLIENT_SERVER

  if (radio_is_remote) {
    send_sample_rate(client_socket, -1, samplerate);
  } else
#endif
  {
    radio_change_sample_rate(samplerate);
  }
}

static void receivers_cb(GtkToggleButton *widget, gpointer data) {
  int val = gtk_combo_box_get_active (GTK_COMBO_BOX(widget)) + 1;

  //
  // reconfigure_radio requires that the RX panels are active
  // (segfault otherwise), therefore ignore this while TXing
  //
  if (isTransmitting()) {
    gtk_combo_box_set_active(GTK_COMBO_BOX(widget), receivers - 1);
    return;
  }

#ifdef CLIENT_SERVER

  if (radio_is_remote) {
    send_receivers(client_socket, val);
  } else
#endif
  {
    radio_change_receivers(val);
  }
}

static void region_cb(GtkWidget *widget, gpointer data) {
  radio_change_region(gtk_combo_box_get_active (GTK_COMBO_BOX(widget)));
}

static void rit_cb(GtkWidget *widget, gpointer data) {
  int val = gtk_combo_box_get_active (GTK_COMBO_BOX(widget));

  switch (val) {
  case 0:
  default:
    rit_increment = 1;
    break;

  case 1:
    rit_increment = 10;
    break;

  case 2:
    rit_increment = 100;
    break;
  }
}

static void ck10mhz_cb(GtkWidget *widget, gpointer data) {
  atlas_clock_source_10mhz = gtk_combo_box_get_active (GTK_COMBO_BOX(widget));
}

static void ck128mhz_cb(GtkWidget *widget, gpointer data) {
  atlas_clock_source_128mhz = SET(gtk_combo_box_get_active (GTK_COMBO_BOX(widget)));
}

static void micsource_cb(GtkWidget *widget, gpointer data) {
  atlas_mic_source = SET(gtk_combo_box_get_active (GTK_COMBO_BOX(widget)));
}

static void tx_cb(GtkWidget *widget, gpointer data) {
  atlas_penelope = SET(gtk_combo_box_get_active (GTK_COMBO_BOX(widget)));
}

static void janus_cb(GtkWidget *widget, gpointer data) {
  atlas_janus = SET(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget)));
}

void radio_menu(GtkWidget *parent) {
  int col;
  dialog = gtk_dialog_new();
  gtk_window_set_transient_for(GTK_WINDOW(dialog), GTK_WINDOW(parent));
  GtkWidget *headerbar = gtk_header_bar_new();
  gtk_window_set_titlebar(GTK_WINDOW(dialog), headerbar);
  gtk_header_bar_set_show_close_button(GTK_HEADER_BAR(headerbar), TRUE);
  gtk_header_bar_set_title(GTK_HEADER_BAR(headerbar), "piHPSDR - Radio");
  g_signal_connect (dialog, "delete_event", G_CALLBACK (close_cb), NULL);
  g_signal_connect (dialog, "destroy", G_CALLBACK (close_cb), NULL);
  GtkWidget *content = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
  GtkWidget *grid = gtk_grid_new();
  gtk_grid_set_column_spacing (GTK_GRID(grid), 5);
  gtk_grid_set_row_spacing (GTK_GRID(grid), 5);
  gtk_grid_set_column_homogeneous (GTK_GRID(grid), FALSE);
  gtk_grid_set_row_homogeneous (GTK_GRID(grid), FALSE);
  int row;
  int max_row;
  GtkWidget *close_b = gtk_button_new_with_label("Close");
  gtk_widget_set_name(close_b, "close_button");
  g_signal_connect (close_b, "button_press_event", G_CALLBACK(close_cb), NULL);
  gtk_grid_attach(GTK_GRID(grid), close_b, 0, 0, 1, 1);
  row = 1;
  GtkWidget *receivers_label = gtk_label_new("Receivers:");
  gtk_widget_set_name(receivers_label, "boldlabel");
  gtk_widget_set_halign(receivers_label, GTK_ALIGN_START);
  gtk_grid_attach(GTK_GRID(grid), receivers_label, 0, row, 1, 1);
  row++;
  GtkWidget *receivers_combo = gtk_combo_box_text_new();
  gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(receivers_combo), NULL, "1");

  if (radio->supported_receivers > 1) {
    gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(receivers_combo), NULL, "2");
  }

  gtk_combo_box_set_active(GTK_COMBO_BOX(receivers_combo), receivers - 1);
  my_combo_attach(GTK_GRID(grid), receivers_combo, 0, row, 1, 1);
  g_signal_connect(receivers_combo, "changed", G_CALLBACK(receivers_cb), NULL);
  row++;

  switch (protocol) {
  case NEW_PROTOCOL:
    // Sample rate changes handled in the RX menu
    break;

  case ORIGINAL_PROTOCOL: {
    GtkWidget *sample_rate_label = gtk_label_new("Sample Rate:");
    gtk_widget_set_name(sample_rate_label, "boldlabel");
    gtk_widget_set_halign(sample_rate_label, GTK_ALIGN_START);
    gtk_grid_attach(GTK_GRID(grid), sample_rate_label, 0, row, 1, 1);
    row++;
    GtkWidget *sample_rate_combo_box = gtk_combo_box_text_new();
    gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(sample_rate_combo_box), NULL, "48000");
    gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(sample_rate_combo_box), NULL, "96000");
    gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(sample_rate_combo_box), NULL, "192000");
    gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(sample_rate_combo_box), NULL, "384000");

    switch (active_receiver->sample_rate) {
    case 48000:
      gtk_combo_box_set_active(GTK_COMBO_BOX(sample_rate_combo_box), 0);
      break;

    case 96000:
      gtk_combo_box_set_active(GTK_COMBO_BOX(sample_rate_combo_box), 1);
      break;

    case 192000:
      gtk_combo_box_set_active(GTK_COMBO_BOX(sample_rate_combo_box), 2);
      break;

    case 384000:
      gtk_combo_box_set_active(GTK_COMBO_BOX(sample_rate_combo_box), 3);
      break;
    }

    my_combo_attach(GTK_GRID(grid), sample_rate_combo_box, 0, row, 1, 1);
    g_signal_connect(sample_rate_combo_box, "changed", G_CALLBACK(sample_rate_cb), NULL);
    row++;
  }
  break;
#ifdef SOAPYSDR

  case SOAPYSDR_PROTOCOL: {
    GtkWidget *sample_rate_label = gtk_label_new("Sample Rate:");
    gtk_widget_set_name(sample_rate_label, "boldlabel");
    gtk_widget_set_halign(sample_rate_label, GTK_ALIGN_START);
    gtk_grid_attach(GTK_GRID(grid), sample_rate_label, 0, row, 1, 1);
    row++;
    char rate_string[16];
    GtkWidget *sample_rate_combo_box = gtk_combo_box_text_new();
    int rate = radio->info.soapy.sample_rate;
    int pos = 0;

    while (rate >= 48000) {
      snprintf(rate_string, 16, "%d", rate);
      gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(sample_rate_combo_box), NULL, rate_string);

      if (rate == active_receiver->sample_rate) {
        gtk_combo_box_set_active(GTK_COMBO_BOX(sample_rate_combo_box), pos);
      }

      rate = rate / 2;
      pos++;
    }

    my_combo_attach(GTK_GRID(grid), sample_rate_combo_box, 0, row, 1, 1);
    g_signal_connect(sample_rate_combo_box, "changed", G_CALLBACK(sample_rate_cb), NULL);
  }

  row++;
  break;
#endif
  }

  max_row = row;
  row = 1;
  GtkWidget *rit_label = gtk_label_new("RIT/XIT step (Hz):");
  gtk_widget_set_name(rit_label, "boldlabel");
  gtk_widget_set_halign(rit_label, GTK_ALIGN_START);
  gtk_grid_attach(GTK_GRID(grid), rit_label, 1, row, 1, 1);
  row++;
  GtkWidget *rit_combo = gtk_combo_box_text_new();
  gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(rit_combo), NULL, "1");
  gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(rit_combo), NULL, "10");
  gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(rit_combo), NULL, "100");

  switch (rit_increment) {
  default:
    rit_increment = 1;

  // fallthrough
  case 1:
    gtk_combo_box_set_active(GTK_COMBO_BOX(rit_combo), 0);
    break;

  case 10:
    gtk_combo_box_set_active(GTK_COMBO_BOX(rit_combo), 1);
    break;

  case 100:
    gtk_combo_box_set_active(GTK_COMBO_BOX(rit_combo), 2);
    break;
  }

  my_combo_attach(GTK_GRID(grid), rit_combo, 1, row, 1, 1);
  g_signal_connect(rit_combo, "changed", G_CALLBACK(rit_cb), NULL);
  row++;
  GtkWidget *sat_label = gtk_label_new("SAT mode:");
  gtk_widget_set_halign(sat_label, GTK_ALIGN_START);
  gtk_widget_set_name(sat_label, "boldlabel");
  gtk_grid_attach(GTK_GRID(grid), sat_label, 1, row, 1, 1);
  row++;
  GtkWidget *sat_combo = gtk_combo_box_text_new();
  gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(sat_combo), NULL, "SAT Off");
  gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(sat_combo), NULL, "SAT");
  gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(sat_combo), NULL, "RSAT");
  gtk_combo_box_set_active(GTK_COMBO_BOX(sat_combo), sat_mode);
  my_combo_attach(GTK_GRID(grid), sat_combo, 1, row, 1, 1);
  g_signal_connect(sat_combo, "changed", G_CALLBACK(sat_cb), NULL);
  row++;

  if (row > max_row) { max_row = row; }

  row = 1;
  GtkWidget *region_label = gtk_label_new("Region:");
  gtk_widget_set_name(region_label, "boldlabel");
  gtk_widget_set_halign(region_label, GTK_ALIGN_START);
  gtk_grid_attach(GTK_GRID(grid), region_label, 2, row, 1, 1);
  row++;
  GtkWidget *region_combo = gtk_combo_box_text_new();
  gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(region_combo), NULL, "Other");
  gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(region_combo), NULL, "UK");
  gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(region_combo), NULL, "WRC15");
  gtk_combo_box_set_active(GTK_COMBO_BOX(region_combo), region);
  my_combo_attach(GTK_GRID(grid), region_combo, 2, row, 1, 1);
  g_signal_connect(region_combo, "changed", G_CALLBACK(region_cb), NULL);
  row++;

  if (protocol == ORIGINAL_PROTOCOL || protocol == NEW_PROTOCOL) {
    GtkWidget *filter_board_label = gtk_label_new("Filter Board:");
    gtk_widget_set_name(filter_board_label, "boldlabel");
    gtk_widget_set_halign(filter_board_label, GTK_ALIGN_START);
    gtk_grid_attach(GTK_GRID(grid), filter_board_label, 2, row, 1, 1);
    row++;
    GtkWidget *filter_combo = gtk_combo_box_text_new();
    gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(filter_combo), NULL, "NONE");
    gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(filter_combo), NULL, "ALEX");
    gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(filter_combo), NULL, "APOLLO");
    gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(filter_combo), NULL, "CHARLY25");
    gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(filter_combo), NULL, "N2ADR");

    switch (filter_board) {
    case NONE:
      gtk_combo_box_set_active(GTK_COMBO_BOX(filter_combo), 0);
      break;

    case ALEX:
      gtk_combo_box_set_active(GTK_COMBO_BOX(filter_combo), 1);
      break;

    case APOLLO:
      gtk_combo_box_set_active(GTK_COMBO_BOX(filter_combo), 2);
      break;

    case CHARLY25:
      gtk_combo_box_set_active(GTK_COMBO_BOX(filter_combo), 3);
      break;

    case N2ADR:
      gtk_combo_box_set_active(GTK_COMBO_BOX(filter_combo), 4);
      break;
    }

    my_combo_attach(GTK_GRID(grid), filter_combo, 2, row, 1, 1);
    g_signal_connect(filter_combo, "changed", G_CALLBACK(filter_cb), NULL);
    row++;
  }

  if (row > max_row) { max_row = row; }

  row = max_row;
  GtkWidget *vfo_divisor_label = gtk_label_new("VFO Encoder Divisor:");
  gtk_widget_set_name(vfo_divisor_label, "boldlabel");
  gtk_widget_set_halign(vfo_divisor_label, GTK_ALIGN_END);
  gtk_grid_attach(GTK_GRID(grid), vfo_divisor_label, 0, row, 2, 1);
  GtkWidget *vfo_divisor = gtk_spin_button_new_with_range(1.0, 60.0, 1.0);
  gtk_spin_button_set_value(GTK_SPIN_BUTTON(vfo_divisor), (double)vfo_encoder_divisor);
  gtk_grid_attach(GTK_GRID(grid), vfo_divisor, 2, row, 1, 1);
  g_signal_connect(vfo_divisor, "value_changed", G_CALLBACK(vfo_divisor_value_changed_cb), NULL);
  row++;

  // cppcheck-suppress knownConditionTrueFalse
  if (row > max_row) { max_row = row; }

  //
  // The HPSDR machine-specific stuff is now put in columns 3+4
  // either the ATLAS bits (METIS) or the ORION microphone settings
  //
  if (device == DEVICE_OZY || device == DEVICE_METIS) {
    row = 1;
    GtkWidget *atlas_label = gtk_label_new("ATLAS bus settings:");
    gtk_widget_set_name(atlas_label, "boldlabel");
    gtk_widget_set_halign(atlas_label, GTK_ALIGN_CENTER);
    gtk_grid_attach(GTK_GRID(grid), atlas_label, 3, row, 2, 1);
    row++;
    GtkWidget *clock10_label = gtk_label_new("10 MHz source:");
    gtk_widget_set_name(clock10_label, "boldlabel");
    gtk_widget_set_halign(clock10_label, GTK_ALIGN_END);
    gtk_grid_attach(GTK_GRID(grid), clock10_label, 3, row, 1, 1);
    GtkWidget *ck10mhz_combo = gtk_combo_box_text_new();
    gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(ck10mhz_combo), NULL, "Atlas");
    gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(ck10mhz_combo), NULL, "Penelope");
    gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(ck10mhz_combo), NULL, "Mercury");
    gtk_combo_box_set_active(GTK_COMBO_BOX(ck10mhz_combo), atlas_clock_source_10mhz);
    my_combo_attach(GTK_GRID(grid), ck10mhz_combo, 4, row, 1, 1);
    g_signal_connect(ck10mhz_combo, "changed", G_CALLBACK(ck10mhz_cb), NULL);
    row++;
    GtkWidget *clock128_label = gtk_label_new("122.88 MHz source:");
    gtk_widget_set_name(clock128_label, "boldlabel");
    gtk_widget_set_halign(clock128_label, GTK_ALIGN_END);
    gtk_grid_attach(GTK_GRID(grid), clock128_label, 3, row, 1, 1);
    GtkWidget *ck128mhz_combo = gtk_combo_box_text_new();
    gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(ck128mhz_combo), NULL, "Penelope");
    gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(ck128mhz_combo), NULL, "Mercury");
    gtk_combo_box_set_active(GTK_COMBO_BOX(ck128mhz_combo), SET(atlas_clock_source_128mhz));
    my_combo_attach(GTK_GRID(grid), ck128mhz_combo, 4, row, 1, 1);
    g_signal_connect(ck128mhz_combo, "changed", G_CALLBACK(ck128mhz_cb), NULL);
    row++;
    GtkWidget *micsource_label = gtk_label_new("Mic source:");
    gtk_widget_set_name(micsource_label, "boldlabel");
    gtk_widget_set_halign(micsource_label, GTK_ALIGN_END);
    gtk_grid_attach(GTK_GRID(grid), micsource_label, 3, row, 1, 1);
    GtkWidget *micsource_combo = gtk_combo_box_text_new();
    gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(micsource_combo), NULL, "Janus");
    gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(micsource_combo), NULL, "Penelope");
    gtk_combo_box_set_active(GTK_COMBO_BOX(micsource_combo), SET(atlas_mic_source));
    my_combo_attach(GTK_GRID(grid), micsource_combo, 4, row, 1, 1);
    g_signal_connect(micsource_combo, "changed", G_CALLBACK(micsource_cb), NULL);
    row++;
    GtkWidget *tx_label = gtk_label_new("TX config:");
    gtk_widget_set_name(tx_label, "boldlabel");
    gtk_widget_set_halign(tx_label, GTK_ALIGN_END);
    gtk_grid_attach(GTK_GRID(grid), tx_label, 3, row, 1, 1);
    GtkWidget *tx_combo = gtk_combo_box_text_new();
    gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(tx_combo), NULL, "No TX");
    gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(tx_combo), NULL, "Penelope");
    gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(tx_combo), NULL, "Pennylane");
    gtk_combo_box_set_active(GTK_COMBO_BOX(tx_combo), atlas_penelope);
    my_combo_attach(GTK_GRID(grid), tx_combo, 4, row, 1, 1);
    g_signal_connect(tx_combo, "changed", G_CALLBACK(tx_cb), NULL);
    row++;

    //
    // This option is for ATLAS systems which *only* have an OZY
    // and a JANUS board (the RF front end then is either SDR-1000 or SoftRock)
    //
    // It is assumed that the SDR-1000 is controlled outside piHPSDR
    //
    if (device == DEVICE_OZY) {
      GtkWidget *janus_b = gtk_check_button_new_with_label("Janus Only");
      gtk_widget_set_name(janus_b, "boldlabel");
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (janus_b), atlas_janus);
      gtk_grid_attach(GTK_GRID(grid), janus_b, 4, row, 1, 1);
      g_signal_connect(janus_b, "toggled", G_CALLBACK(janus_cb), NULL);
      row++;
    }

    if (row > max_row) { max_row = row; }
  }

  if (device == NEW_DEVICE_ORION || device == NEW_DEVICE_ORION2 || device == NEW_DEVICE_SATURN ||
      device == DEVICE_ORION || device == DEVICE_ORION2) {
    row = 1;
    GtkWidget *orion_label = gtk_label_new("ORION/SATURN Mic jack:");
    gtk_widget_set_name(orion_label, "boldlabel");
    gtk_widget_set_halign(orion_label, GTK_ALIGN_CENTER);
    gtk_grid_attach(GTK_GRID(grid), orion_label, 3, row, 2, 1);
    row++;
    GtkWidget *ptt_ring_b = gtk_radio_button_new_with_label(NULL, "PTT On Ring, Mic and Bias on Tip");
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (ptt_ring_b), mic_ptt_tip_bias_ring == 0);
    gtk_grid_attach(GTK_GRID(grid), ptt_ring_b, 3, row, 2, 1);
    g_signal_connect(ptt_ring_b, "toggled", G_CALLBACK(ptt_ring_cb), NULL);
    row++;
    GtkWidget *ptt_tip_b = gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(ptt_ring_b),
                           "PTT On Tip, Mic and Bias on Ring");
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (ptt_tip_b), mic_ptt_tip_bias_ring == 1);
    gtk_grid_attach(GTK_GRID(grid), ptt_tip_b, 3, row, 2, 1);
    g_signal_connect(ptt_tip_b, "toggled", G_CALLBACK(ptt_tip_cb), NULL);
    row++;

    if (device == NEW_DEVICE_SATURN) {
      GtkWidget *mic_input_label = gtk_label_new("Mic Input:");
      gtk_widget_set_name(mic_input_label, "boldlabel");
      gtk_grid_attach(GTK_GRID(grid), mic_input_label, 4, row, 1, 1);
      GtkWidget *mic_input_combo = gtk_combo_box_text_new();
      gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(mic_input_combo), NULL, "3.5mm");
      gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(mic_input_combo), NULL, "XLR");

      switch (mic_input_xlr) {
      case MIC3P55MM:
        gtk_combo_box_set_active(GTK_COMBO_BOX(mic_input_combo), 0);
        break;

      case MICXLR:
        gtk_combo_box_set_active(GTK_COMBO_BOX(mic_input_combo), 1);
        break;
      }

      my_combo_attach(GTK_GRID(grid), mic_input_combo, 4, row + 1, 1, 1);
      g_signal_connect(mic_input_combo, "changed", G_CALLBACK(mic_input_cb), NULL);
    }

    GtkWidget *ptt_b = gtk_check_button_new_with_label("PTT Enabled");
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (ptt_b), mic_ptt_enabled);
    gtk_grid_attach(GTK_GRID(grid), ptt_b, 3, row, 1, 1);
    g_signal_connect(ptt_b, "toggled", G_CALLBACK(ptt_cb), NULL);
    row++;
    GtkWidget *bias_b = gtk_check_button_new_with_label("BIAS Enabled");
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (bias_b), mic_bias_enabled);
    gtk_grid_attach(GTK_GRID(grid), bias_b, 3, row, 1, 1);
    g_signal_connect(bias_b, "toggled", G_CALLBACK(bias_cb), NULL);
    row++;

    if (row > max_row) { max_row = row; }
  }

  row = max_row;
  col = 0;
  //
  // Insert small separation between top columns and bottom rows
  //
  GtkWidget *Separator = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
  gtk_widget_set_size_request(Separator, -1, 3);
  gtk_grid_attach(GTK_GRID(grid), Separator, col, row, 5, 1);
  row++;

  if (can_transmit) {
    GtkWidget *split_b = gtk_check_button_new_with_label("Split");
    gtk_widget_set_name(split_b, "boldlabel");
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (split_b), split);
    gtk_grid_attach(GTK_GRID(grid), split_b, col, row, 1, 1);
    g_signal_connect(split_b, "toggled", G_CALLBACK(split_cb), NULL);
    col++;
    duplex_b = gtk_check_button_new_with_label("Duplex");
    gtk_widget_set_name(duplex_b, "boldlabel");
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (duplex_b), duplex);
    gtk_grid_attach(GTK_GRID(grid), duplex_b, col, row, 1, 1);
    g_signal_connect(duplex_b, "toggled", G_CALLBACK(duplex_cb), NULL);
    col++;
    mute_rx_b = gtk_check_button_new_with_label("Mute RX when TX");
    gtk_widget_set_name(mute_rx_b, "boldlabel");
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (mute_rx_b), mute_rx_while_transmitting);
    gtk_grid_attach(GTK_GRID(grid), mute_rx_b, col, row, 1, 1);
    g_signal_connect(mute_rx_b, "toggled", G_CALLBACK(mute_rx_cb), NULL);
    col++;
    GtkWidget *PA_enable_b = gtk_check_button_new_with_label("PA enable");
    gtk_widget_set_name(PA_enable_b, "boldlabel");
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (PA_enable_b), pa_enabled);
    gtk_grid_attach(GTK_GRID(grid), PA_enable_b, col, row, 1, 1);
    g_signal_connect(PA_enable_b, "toggled", G_CALLBACK(PA_enable_cb), NULL);
    row++;
  }

  col = 0;
  GtkWidget *touchscreen_b = gtk_check_button_new_with_label("Optimize for TouchScreen");
  gtk_widget_set_name(touchscreen_b, "boldlabel");
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (touchscreen_b), optimize_for_touchscreen);
  gtk_grid_attach(GTK_GRID(grid), touchscreen_b, col, row, 2, 1);
  g_signal_connect(touchscreen_b, "toggled", G_CALLBACK(touchscreen_cb), NULL);
  col += 2;

  switch(device) {
  case NEW_DEVICE_ORION2:
  case NEW_DEVICE_SATURN:
    {
    GtkWidget *mute_spkr_amp_b = gtk_check_button_new_with_label("Mute Spkr Amp");
    gtk_widget_set_name(mute_spkr_amp_b, "boldlabel");
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (mute_spkr_amp_b), mute_spkr_amp);
    gtk_grid_attach(GTK_GRID(grid), mute_spkr_amp_b, col, row, 1, 1);
    g_signal_connect(mute_spkr_amp_b, "toggled", G_CALLBACK(mute_spkr_amp_cb), NULL);
    col++;
    }
    break;
  case DEVICE_HERMES_LITE2:
    {
    GtkWidget *hl2audio_b = gtk_check_button_new_with_label("HL2 audio codec");
    gtk_widget_set_name(hl2audio_b, "boldlabel");
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(hl2audio_b), hl2_audio_codec);
    gtk_grid_attach(GTK_GRID(grid), hl2audio_b, col, row, 1, 1);
    g_signal_connect(hl2audio_b, "toggled", G_CALLBACK(hl2audio_cb), NULL);
    col++;
    }
    break;
  case DEVICE_HERMES:
  case NEW_DEVICE_HERMES:
    {
    GtkWidget *anan10e_b = gtk_check_button_new_with_label("Anan-10E/100B");
    gtk_widget_set_name(anan10e_b, "boldlabel");
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(anan10e_b), anan10E);
    gtk_grid_attach(GTK_GRID(grid), anan10e_b, col, row, 1, 1);
    g_signal_connect(anan10e_b, "toggled", G_CALLBACK(anan10e_cb), NULL);
    col++;
    }
    break;
#ifdef SOAPYSDR
  case SOAPYSDR_USB_DEVICE:
    {
    GtkWidget *iqswap_b = gtk_check_button_new_with_label("Swap IQ");
    gtk_widget_set_name(iqswap_b, "boldlabel");
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (iqswap_b), iqswap);
    gtk_grid_attach(GTK_GRID(grid), iqswap_b, col, row, 1, 1);
    g_signal_connect(iqswap_b, "toggled", G_CALLBACK(iqswap_cb), NULL);
    col++;

    if (radio->info.soapy.rx_has_automatic_gain) {
      GtkWidget *agc = gtk_check_button_new_with_label("Hardware AGC");
      gtk_widget_set_name(agc, "boldlabel");
      gtk_grid_attach(GTK_GRID(grid), agc, col, row, 1, 1);
      gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(agc), adc[0].agc);
      g_signal_connect(agc, "toggled", G_CALLBACK(agc_changed_cb), &adc[0]);
      col++;
    }
    }
    break;
#endif
  }

  row++;
  // cppcheck-suppress redundantAssignment
  col = 0;
  GtkWidget *calibration_label = gtk_label_new("Frequency\nCalibration (Hz):");
  gtk_widget_set_name(calibration_label, "boldlabel");
  gtk_grid_attach(GTK_GRID(grid), calibration_label, col, row, 1, 1);
  col++;
  GtkWidget *calibration_b = gtk_spin_button_new_with_range(-9999.0, 9999.0, 1.0);
  gtk_spin_button_set_value(GTK_SPIN_BUTTON(calibration_b), (double)frequency_calibration);
  gtk_grid_attach(GTK_GRID(grid), calibration_b, col, row, 1, 1);
  g_signal_connect(calibration_b, "value_changed", G_CALLBACK(calibration_value_changed_cb), NULL);
  //
  // Calibration of the RF front end
  //
  col++;
  GtkWidget *rx_gain_calib_label = gtk_label_new("RX Gain\nCalibration (dB):");
  gtk_widget_set_name(rx_gain_calib_label, "boldlabel");
  gtk_grid_attach(GTK_GRID(grid), rx_gain_calib_label, col, row, 1, 1);
  col++;
  GtkWidget *rx_gain_calibration_b = gtk_spin_button_new_with_range(-50.0, 50.0, 1.0);
  gtk_spin_button_set_value(GTK_SPIN_BUTTON(rx_gain_calibration_b), (double)rx_gain_calibration);
  gtk_grid_attach(GTK_GRID(grid), rx_gain_calibration_b, col, row, 1, 1);
  g_signal_connect(rx_gain_calibration_b, "value_changed", G_CALLBACK(rx_gain_calibration_value_changed_cb), NULL);
#ifdef SOAPYSDR
  row++;
  col = 0;

  if (device == SOAPYSDR_USB_DEVICE) {
    //
    // If there is only a single RX or TX gain element, then we need not display it
    // since it is better done via the main "Drive" or "RF gain" slider.
    // At the moment, we present spin-buttons for the individual gain ranges
    // for SDRplay, RTLsdr, and whenever there is more than on RX or TX gain element.
    //
    int display_gains = 0;

    //
    // For RTL sticks, there is a single gain element named "TUNER", and this is very well
    // controlled by the main RF gain slider. This perhaps also true for sdrplay, but
    // I could not test this because I do not have the hardware
    //
    //if (strcmp(radio->name, "sdrplay") == 0 || strcmp(radio->name, "rtlsdr") == 0) { display_gains = 1; }
    if (strcmp(radio->name, "sdrplay") == 0 )  { display_gains = 1; }

    if (radio->info.soapy.rx_gains > 1) { display_gains = 1; }

    if (radio->info.soapy.tx_gains > 1) { display_gains = 1; }

    if (display_gains) {
      //
      // Select RX gain from one out of a list of ranges (represented by spin buttons)
      //
      GtkWidget *rx_gain = gtk_label_new("RX Gains:");
      gtk_widget_set_name(rx_gain, "boldlabel");
      gtk_widget_set_halign(rx_gain, GTK_ALIGN_START);
      gtk_grid_attach(GTK_GRID(grid), rx_gain, 0, row, 1, 1);

      if (can_transmit) {
        GtkWidget *tx_gain = gtk_label_new("TX Gains:");
        gtk_widget_set_name(tx_gain, "boldlabel");
        gtk_widget_set_halign(tx_gain, GTK_ALIGN_START);
        gtk_grid_attach(GTK_GRID(grid), tx_gain, 2, row, 1, 1);
      }
    }

    row++;
    max_row = row;

    if (display_gains) {
      //
      // Draw a spin-button for each range
      //
      for (int i = 0; i < radio->info.soapy.rx_gains; i++) {
        GtkWidget *rx_gain_label = gtk_label_new(radio->info.soapy.rx_gain[i]);
        gtk_grid_attach(GTK_GRID(grid), rx_gain_label, 0, row, 1, 1);
        col++;
        SoapySDRRange range = radio->info.soapy.rx_range[i];

        if (range.step == 0.0) {
          range.step = 1.0;
        }

        GtkWidget *rx_gain = gtk_spin_button_new_with_range(range.minimum, range.maximum, range.step);
        gtk_widget_set_name (rx_gain, radio->info.soapy.rx_gain[i]);
        int value = soapy_protocol_get_gain_element(active_receiver, radio->info.soapy.rx_gain[i]);
        gtk_spin_button_set_value(GTK_SPIN_BUTTON(rx_gain), (double)value);
        gtk_grid_attach(GTK_GRID(grid), rx_gain, 1, row, 1, 1);
        g_signal_connect(rx_gain, "value_changed", G_CALLBACK(rx_gain_element_changed_cb), &adc[0]);
        row++;
      }
    }

    row = max_row;

    if (can_transmit && display_gains) {
      //
      // Select TX gain out of a list of discrete ranges
      //
      for (int i = 0; i < radio->info.soapy.tx_gains; i++) {
        GtkWidget *tx_gain_label = gtk_label_new(radio->info.soapy.tx_gain[i]);
        gtk_grid_attach(GTK_GRID(grid), tx_gain_label, 2, row, 1, 1);
        SoapySDRRange range = radio->info.soapy.tx_range[i];

        if (range.step == 0.0) {
          range.step = 1.0;
        }

        GtkWidget *tx_gain = gtk_spin_button_new_with_range(range.minimum, range.maximum, range.step);
        gtk_widget_set_name (tx_gain, radio->info.soapy.tx_gain[i]);
        int value = soapy_protocol_get_tx_gain_element(transmitter, radio->info.soapy.tx_gain[i]);
        gtk_spin_button_set_value(GTK_SPIN_BUTTON(tx_gain), (double)value);
        gtk_grid_attach(GTK_GRID(grid), tx_gain, 3, row, 1, 1);
        g_signal_connect(tx_gain, "value_changed", G_CALLBACK(tx_gain_element_changed_cb), &dac[0]);
        row++;
      }
    }
  }

#endif
  gtk_container_add(GTK_CONTAINER(content), grid);
  sub_menu = dialog;
  gtk_widget_show_all(dialog);
}
