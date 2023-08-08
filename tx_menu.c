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

#include <gtk/gtk.h>
#include <stdio.h>
#include <string.h>

#include "audio.h"
#include "new_menu.h"
#include "radio.h"
#include "sliders.h"
#include "transmitter.h"
#include "ext.h"
#include "filter.h"
#include "mode.h"
#include "vfo.h"
#include "message.h"

static GtkWidget *dialog = NULL;
static GtkWidget *input;
static GtkWidget *mic_in_b = NULL;
static GtkWidget *tx_spin_low;
static GtkWidget *tx_spin_high;

static void cleanup() {
  if (dialog != NULL) {
    gtk_widget_destroy(dialog);
    dialog = NULL;
    sub_menu = NULL;
  }
}

static gboolean close_cb (GtkWidget *widget, GdkEventButton *event, gpointer data) {
  cleanup();
  return TRUE;
}

static gboolean delete_event(GtkWidget *widget, GdkEvent *event, gpointer user_data) {
  cleanup();
  return FALSE;
}

static void comp_enable_cb(GtkWidget *widget, gpointer data) {
  transmitter_set_compressor(transmitter, gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget)));
  mode_settings[transmitter->mode].compressor = transmitter->compressor;
  g_idle_add(ext_vfo_update, NULL);
}

static void comp_cb(GtkWidget *widget, gpointer data) {
  transmitter_set_compressor_level(transmitter, gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(widget)));
  mode_settings[transmitter->mode].compressor_level = transmitter->compressor_level;
  g_idle_add(ext_vfo_update, NULL);
}

static void tx_spin_low_cb (GtkWidget *widget, gpointer data) {
  tx_filter_low = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(widget));
  tx_set_filter(transmitter);
}

static void tx_spin_high_cb (GtkWidget *widget, gpointer data) {
  tx_filter_high = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(widget));
  tx_set_filter(transmitter);
}

static void mic_in_cb(GtkWidget *widget, gpointer data) {
  int i = gtk_combo_box_get_active(GTK_COMBO_BOX(widget));

  switch (i) {
  case 0: // Mic In, but no boost
    mic_boost = 0;
    mic_linein = 0;
    break;

  case 1: // Mic In with boost
    mic_boost = 1;
    mic_linein = 0;
    break;

  case 2: // Line in
    mic_boost = 0;
    mic_linein = 1;
    break;
  }

  g_idle_add(ext_sliders_update, NULL);
}

static void panadapter_high_value_changed_cb(GtkWidget *widget, gpointer data) {
  transmitter->panadapter_high = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(widget));
}

static void panadapter_low_value_changed_cb(GtkWidget *widget, gpointer data) {
  transmitter->panadapter_low = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(widget));
}

static void panadapter_step_value_changed_cb(GtkWidget *widget, gpointer data) {
  transmitter->panadapter_step = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(widget));
}

static void am_carrier_level_value_changed_cb(GtkWidget *widget, gpointer data) {
  transmitter->am_carrier_level = gtk_spin_button_get_value(GTK_SPIN_BUTTON(widget));
  transmitter_set_am_carrier_level(transmitter);
}

static void ctcss_cb (GtkWidget *widget, gpointer data) {
  int state = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget));
  transmitter_set_ctcss(transmitter, state, transmitter->ctcss);
}

static void ctcss_frequency_cb(GtkWidget *widget, gpointer data) {
  int i = gtk_combo_box_get_active(GTK_COMBO_BOX(widget));
  transmitter_set_ctcss(transmitter, transmitter->ctcss_enabled, i);
}

static void tune_use_drive_cb (GtkWidget *widget, gpointer data) {
  transmitter->tune_use_drive = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget));
}

static void tune_drive_cb (GtkWidget *widget, gpointer data) {
  transmitter->tune_drive = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(widget));
}

static void digi_drive_cb (GtkWidget *widget, gpointer data) {
  drive_digi_max = gtk_spin_button_get_value(GTK_SPIN_BUTTON(widget));

  if (transmitter->drive > drive_digi_max + 0.5) {
    set_drive(drive_digi_max);
  }
}

static void swr_protection_cb (GtkWidget *widget, gpointer data) {
  transmitter->swr_protection = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));
}

static void swr_alarm_cb (GtkWidget *widget, gpointer data) {
  transmitter->swr_alarm = (double)gtk_spin_button_get_value(GTK_SPIN_BUTTON(widget));
}

static void use_rx_filter_cb(GtkWidget *widget, gpointer data) {
  transmitter->use_rx_filter = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget));
  tx_set_filter(transmitter);

  if (transmitter->use_rx_filter) {
    gtk_widget_set_sensitive (tx_spin_low, FALSE);
    gtk_widget_set_sensitive (tx_spin_high, FALSE);
  } else {
    gtk_widget_set_sensitive (tx_spin_low, TRUE);
    gtk_widget_set_sensitive (tx_spin_high, TRUE);
  }
}

static void local_microphone_cb(GtkWidget *widget, gpointer data) {
  if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget))) {
    if (transmitter->microphone_name == NULL) {
      int i = gtk_combo_box_get_active(GTK_COMBO_BOX(input));
      transmitter->microphone_name = g_strdup(input_devices[i].name);
    }

    if (audio_open_input() == 0) {
      transmitter->local_microphone = 1;

      if (mic_in_b) { gtk_widget_hide(mic_in_b); }
    } else {
      transmitter->local_microphone = 0;
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget), FALSE);

      if (mic_in_b) { gtk_widget_show(mic_in_b); }
    }
  } else {
    if (transmitter->local_microphone) {
      transmitter->local_microphone = 0;
      audio_close_input();

      if (mic_in_b) { gtk_widget_show(mic_in_b); }
    }
  }
}

static void local_input_changed_cb(GtkWidget *widget, gpointer data) {
  int i = gtk_combo_box_get_active(GTK_COMBO_BOX(widget));
  t_print("local_input_changed_cb: %d %s\n", i, input_devices[i].name);

  if (transmitter->local_microphone) {
    audio_close_input();
  }

  if (transmitter->microphone_name != NULL) {
    g_free(transmitter->microphone_name);
  }

  transmitter->microphone_name = g_strdup(input_devices[i].name);

  if (transmitter->local_microphone) {
    if (audio_open_input() < 0) {
      transmitter->local_microphone = 0;
    }
  }
}

static gboolean emp_cb (GtkWidget *widget, gpointer data) {
  pre_emphasize = !gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget));
  tx_set_pre_emphasize(transmitter, pre_emphasize);
  return FALSE;
}

void tx_menu(GtkWidget *parent) {
  int i;
  char temp[32];
  GtkWidget *label;
  dialog = gtk_dialog_new();
  gtk_window_set_transient_for(GTK_WINDOW(dialog), GTK_WINDOW(parent));
  gtk_window_set_title(GTK_WINDOW(dialog), "piHPSDR - Transmit");
  g_signal_connect (dialog, "delete_event", G_CALLBACK (delete_event), NULL);
  set_backgnd(dialog);
  GtkWidget *content = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
  GtkWidget *grid = gtk_grid_new();
  gtk_grid_set_column_spacing (GTK_GRID(grid), 5);
  gtk_grid_set_row_spacing (GTK_GRID(grid), 5);
  int row = 0;
  int col = 0;
  GtkWidget *close_b = gtk_button_new_with_label("Close");
  g_signal_connect (close_b, "button-press-event", G_CALLBACK(close_cb), NULL);
  gtk_grid_attach(GTK_GRID(grid), close_b, col, row, 1, 1);
  row++;
  col = 0;

  if (n_input_devices > 0) {
    GtkWidget *local_microphone_b = gtk_check_button_new_with_label("Local Microphone");
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (local_microphone_b), transmitter->local_microphone);
    gtk_widget_show(local_microphone_b);
    gtk_grid_attach(GTK_GRID(grid), local_microphone_b, col, row, 1, 1);
    g_signal_connect(local_microphone_b, "toggled", G_CALLBACK(local_microphone_cb), NULL);
    col++;
    input = gtk_combo_box_text_new();

    for (i = 0; i < n_input_devices; i++) {
      gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(input), NULL, input_devices[i].description);

      if (transmitter->microphone_name != NULL) {
        if (strcmp(transmitter->microphone_name, input_devices[i].name) == 0) {
          gtk_combo_box_set_active(GTK_COMBO_BOX(input), i);
        }
      }
    }

    // If the combo box shows no device, take the first one
    // AND set the mic.name to that device name.
    // This situation occurs if the local microphone device in the props
    // file is no longer present
    i = gtk_combo_box_get_active(GTK_COMBO_BOX(input));

    if (i < 0) {
      gtk_combo_box_set_active(GTK_COMBO_BOX(input), 0);

      if (transmitter->microphone_name != NULL) {
        g_free(transmitter->microphone_name);
      }

      transmitter->microphone_name = g_strdup(input_devices[0].name);
    }

    my_combo_attach(GTK_GRID(grid), input, col, row, 3, 1);
    g_signal_connect(input, "changed", G_CALLBACK(local_input_changed_cb), NULL);
  }

  row++;
  col = 0;
  GtkWidget *comp_enable = gtk_check_button_new_with_label("Compression (dB):");
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (comp_enable), transmitter->compressor);
  gtk_grid_attach(GTK_GRID(grid), comp_enable, col, row, 1, 1);
  g_signal_connect(comp_enable, "toggled", G_CALLBACK(comp_enable_cb), NULL);
  col++;
  GtkWidget *comp = gtk_spin_button_new_with_range(0.0, 20.0, 1.0);
  gtk_spin_button_set_value(GTK_SPIN_BUTTON(comp), (double)transmitter->compressor_level);
  gtk_grid_attach(GTK_GRID(grid), comp, col, row, 1, 1);
  g_signal_connect(comp, "value-changed", G_CALLBACK(comp_cb), NULL);
  col++;
  gboolean device_has_microphone_input;

  switch (device) {
  case DEVICE_METIS:
  case DEVICE_OZY:
  case DEVICE_HERMES:
  case DEVICE_GRIFFIN:
  case DEVICE_ANGELIA:
  case DEVICE_ORION:
  case DEVICE_ORION2:
  case DEVICE_STEMLAB:
  case DEVICE_STEMLAB_Z20:
  case NEW_DEVICE_ATLAS:
  case NEW_DEVICE_HERMES:
  case NEW_DEVICE_HERMES2:
  case NEW_DEVICE_ANGELIA:
  case NEW_DEVICE_ORION:
  case NEW_DEVICE_ORION2:
  case NEW_DEVICE_SATURN:
    device_has_microphone_input = TRUE;
    break;

  default:
    device_has_microphone_input = FALSE;
    break;
  }

  if (device_has_microphone_input) {
    //
    // Mic Boost, Mic In, and Line In can the handled mutually exclusive
    //
    mic_in_b = gtk_combo_box_text_new();
    gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(mic_in_b), NULL, "Mic In");
    gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(mic_in_b), NULL, "Mic Boost");
    gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(mic_in_b), NULL, "Line In");
    int pos = 0;

    if (mic_linein) {
      pos = 2;
    } else if (mic_boost) {
      pos = 1;
    }

    gtk_combo_box_set_active(GTK_COMBO_BOX(mic_in_b), pos);
    my_combo_attach(GTK_GRID(grid), mic_in_b, col, row, 1, 1);
    g_signal_connect(mic_in_b, "changed", G_CALLBACK(mic_in_cb), NULL);
    col++;
  }

  GtkWidget *emp_b = gtk_check_button_new_with_label("FM TX Pre-emphasize before limiting");
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (emp_b), !pre_emphasize);
  gtk_grid_attach(GTK_GRID(grid), emp_b, col, row, 2, 1);
  g_signal_connect(emp_b, "toggled", G_CALLBACK(emp_cb), NULL);
  row++;
  col = 0;
  label = gtk_label_new(NULL);
  gtk_label_set_markup(GTK_LABEL(label), "<b>TX Filter Low: </b>");
  gtk_widget_set_halign(label, GTK_ALIGN_START);
  gtk_grid_attach(GTK_GRID(grid), label, col, row, 1, 1);
  col++;
  tx_spin_low = gtk_spin_button_new_with_range(0.0, 8000.0, 1.0);
  gtk_spin_button_set_value(GTK_SPIN_BUTTON(tx_spin_low), (double)tx_filter_low);
  gtk_grid_attach(GTK_GRID(grid), tx_spin_low, col, row, 1, 1);
  g_signal_connect(tx_spin_low, "value-changed", G_CALLBACK(tx_spin_low_cb), NULL);

  if (transmitter->use_rx_filter) {
    gtk_widget_set_sensitive (tx_spin_low, FALSE);
  }

  col++;
  label = gtk_label_new(NULL);
  gtk_label_set_markup(GTK_LABEL(label), "<b>TX Filter High: </b>");
  gtk_widget_set_halign(label, GTK_ALIGN_START);
  gtk_grid_attach(GTK_GRID(grid), label, col, row, 1, 1);
  col++;
  tx_spin_high = gtk_spin_button_new_with_range(0.0, 8000.0, 1.0);
  gtk_spin_button_set_value(GTK_SPIN_BUTTON(tx_spin_high), (double)tx_filter_high);
  gtk_grid_attach(GTK_GRID(grid), tx_spin_high, col, row, 1, 1);
  g_signal_connect(tx_spin_high, "value-changed", G_CALLBACK(tx_spin_high_cb), NULL);
  col++;
  GtkWidget *use_rx_filter_b = gtk_check_button_new_with_label("Use RX Filter");
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (use_rx_filter_b), transmitter->use_rx_filter);
  gtk_widget_show(use_rx_filter_b);
  gtk_grid_attach(GTK_GRID(grid), use_rx_filter_b, col, row, 1, 1);
  g_signal_connect(use_rx_filter_b, "toggled", G_CALLBACK(use_rx_filter_cb), NULL);

  if (transmitter->use_rx_filter) {
    gtk_widget_set_sensitive (tx_spin_high, FALSE);
  }

  row++;
  col = 0;
  label = gtk_label_new(NULL);
  gtk_label_set_markup(GTK_LABEL(label), "<b>Panadapter High: </b>");
  gtk_widget_set_halign(label, GTK_ALIGN_START);
  gtk_widget_show(label);
  gtk_grid_attach(GTK_GRID(grid), label, col, row, 1, 1);
  col++;
  GtkWidget *panadapter_high_r = gtk_spin_button_new_with_range(-220.0, 100.0, 1.0);
  gtk_spin_button_set_value(GTK_SPIN_BUTTON(panadapter_high_r), (double)transmitter->panadapter_high);
  gtk_widget_show(panadapter_high_r);
  gtk_grid_attach(GTK_GRID(grid), panadapter_high_r, col, row, 1, 1);
  g_signal_connect(panadapter_high_r, "value_changed", G_CALLBACK(panadapter_high_value_changed_cb), NULL);
  col++;
  GtkWidget *tune_use_drive_b = gtk_check_button_new_with_label("Tune use drive");
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (tune_use_drive_b), transmitter->tune_use_drive);
  gtk_widget_show(tune_use_drive_b);
  gtk_grid_attach(GTK_GRID(grid), tune_use_drive_b, col, row, 1, 1);
  g_signal_connect(tune_use_drive_b, "toggled", G_CALLBACK(tune_use_drive_cb), NULL);
  col++;
  label = gtk_label_new(NULL);
  gtk_label_set_markup(GTK_LABEL(label), "<b>Drive level for Tune:</b>");
  gtk_widget_set_halign(label, GTK_ALIGN_START);
  gtk_widget_show(label);
  gtk_grid_attach(GTK_GRID(grid), label, col, row, 1, 1);
  col++;
  GtkWidget *tune_drive = gtk_spin_button_new_with_range(1.0, 100.0, 1.0);
  gtk_spin_button_set_value(GTK_SPIN_BUTTON(tune_drive), (double)transmitter->tune_drive);
  gtk_grid_attach(GTK_GRID(grid), tune_drive, col, row, 1, 1);
  g_signal_connect(tune_drive, "value-changed", G_CALLBACK(tune_drive_cb), NULL);
  row++;
  col = 0;
  label = gtk_label_new(NULL);
  gtk_label_set_markup(GTK_LABEL(label), "<b>Panadapter Low: </b>");
  gtk_widget_set_halign(label, GTK_ALIGN_START);
  gtk_widget_show(label);
  gtk_grid_attach(GTK_GRID(grid), label, col, row, 1, 1);
  col++;
  GtkWidget *panadapter_low_r = gtk_spin_button_new_with_range(-400.0, 100.0, 1.0);
  gtk_spin_button_set_value(GTK_SPIN_BUTTON(panadapter_low_r), (double)transmitter->panadapter_low);
  gtk_widget_show(panadapter_low_r);
  gtk_grid_attach(GTK_GRID(grid), panadapter_low_r, col, row, 1, 1);
  g_signal_connect(panadapter_low_r, "value_changed", G_CALLBACK(panadapter_low_value_changed_cb), NULL);
  col++;

  if (protocol != SOAPYSDR_PROTOCOL) {
    GtkWidget *swr_protection_b = gtk_check_button_new_with_label("SWR Protection");
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (swr_protection_b), transmitter->swr_protection);
    gtk_widget_show(swr_protection_b);
    gtk_grid_attach(GTK_GRID(grid), swr_protection_b, col, row, 1, 1);
    g_signal_connect(swr_protection_b, "toggled", G_CALLBACK(swr_protection_cb), NULL);
    col++;
    GtkWidget *swr_alarm_label = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(swr_alarm_label), "<b>SWR alarm at:</b>");
    gtk_widget_set_halign(swr_alarm_label, GTK_ALIGN_START);
    gtk_widget_show(swr_alarm_label);
    gtk_grid_attach(GTK_GRID(grid), swr_alarm_label, col, row, 1, 1);
    col++;
    GtkWidget *swr_alarm = gtk_spin_button_new_with_range(1.0, 10.0, 0.1);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(swr_alarm), (double)transmitter->swr_alarm);
    gtk_grid_attach(GTK_GRID(grid), swr_alarm, col, row, 1, 1);
    g_signal_connect(swr_alarm, "value-changed", G_CALLBACK(swr_alarm_cb), NULL);
  }

  row++;
  col = 0;
  label = gtk_label_new(NULL);
  gtk_label_set_markup(GTK_LABEL(label), "<b>Panadapter Step: </b>");
  gtk_widget_set_halign(label, GTK_ALIGN_START);
  gtk_widget_show(label);
  gtk_grid_attach(GTK_GRID(grid), label, col, row, 1, 1);
  col++;
  GtkWidget *panadapter_step_r = gtk_spin_button_new_with_range(-400.0, 100.0, 1.0);
  gtk_spin_button_set_value(GTK_SPIN_BUTTON(panadapter_step_r), (double)transmitter->panadapter_step);
  gtk_widget_show(panadapter_step_r);
  gtk_grid_attach(GTK_GRID(grid), panadapter_step_r, col, row, 1, 1);
  g_signal_connect(panadapter_step_r, "value_changed", G_CALLBACK(panadapter_step_value_changed_cb), NULL);
  col++;
  GtkWidget *ctcss_b = gtk_check_button_new_with_label("CTCSS Enable");
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (ctcss_b), transmitter->ctcss_enabled);
  gtk_widget_show(ctcss_b);
  gtk_grid_attach(GTK_GRID(grid), ctcss_b, col, row, 1, 1);
  g_signal_connect(ctcss_b, "toggled", G_CALLBACK(ctcss_cb), NULL);
  col++;
  label = gtk_label_new(NULL);
  gtk_label_set_markup(GTK_LABEL(label), "<b>CTCSS Frequency: </b>");
  gtk_widget_set_halign(label, GTK_ALIGN_START);
  gtk_widget_show(label);
  gtk_grid_attach(GTK_GRID(grid), label, col, row, 1, 1);
  col++;
  GtkWidget *ctcss_frequency_b = gtk_combo_box_text_new();

  for (i = 0; i < CTCSS_FREQUENCIES; i++) {
    sprintf(temp, "%0.1f", ctcss_frequencies[i]);
    gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(ctcss_frequency_b), NULL, temp);
  }

  gtk_combo_box_set_active(GTK_COMBO_BOX(ctcss_frequency_b), transmitter->ctcss);
  my_combo_attach(GTK_GRID(grid), ctcss_frequency_b, col, row, 1, 1);
  g_signal_connect(ctcss_frequency_b, "changed", G_CALLBACK(ctcss_frequency_cb), NULL);
  row++;
  col = 0;
  label = gtk_label_new(NULL);
  gtk_label_set_markup(GTK_LABEL(label), "<b>AM Carrier Level:</b>");
  gtk_widget_set_halign(label, GTK_ALIGN_START);
  gtk_widget_show(label);
  gtk_grid_attach(GTK_GRID(grid), label, col, row, 1, 1);
  col++;
  GtkWidget *am_carrier_level = gtk_spin_button_new_with_range(0.0, 1.0, 0.1);
  gtk_spin_button_set_value(GTK_SPIN_BUTTON(am_carrier_level), (double)transmitter->am_carrier_level);
  gtk_widget_show(am_carrier_level);
  gtk_grid_attach(GTK_GRID(grid), am_carrier_level, col, row, 1, 1);
  g_signal_connect(am_carrier_level, "value_changed", G_CALLBACK(am_carrier_level_value_changed_cb), NULL);
  col++;
  label = gtk_label_new(NULL);
  gtk_label_set_markup(GTK_LABEL(label), "<b>Max Drive level for DIGU/DIGL modes:</b>");
  gtk_widget_set_halign(label, GTK_ALIGN_START);
  gtk_widget_show(label);
  gtk_grid_attach(GTK_GRID(grid), label, col, row, 2, 1);
  col++;
  col++;
  GtkWidget *digi_drive_b = gtk_spin_button_new_with_range(1.0, drive_max, 1.0);
  gtk_spin_button_set_value(GTK_SPIN_BUTTON(digi_drive_b), drive_digi_max);
  gtk_widget_show(digi_drive_b);
  gtk_grid_attach(GTK_GRID(grid), digi_drive_b, col, row, 1, 1);
  g_signal_connect(digi_drive_b, "value-changed", G_CALLBACK(digi_drive_cb), NULL);
  gtk_container_add(GTK_CONTAINER(content), grid);
  sub_menu = dialog;
  gtk_widget_show_all(dialog);
}
