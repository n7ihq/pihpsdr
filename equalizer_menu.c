/* Copyright (C)
* 2016 - John Melton, G0ORX/N6LYT
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

#include <wdsp.h>

#include "new_menu.h"
#include "equalizer_menu.h"
#include "radio.h"
#include "ext.h"
#include "vfo.h"
#include "message.h"

static GtkWidget *dialog = NULL;

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

void set_eq() {
  SetTXAGrphEQ(transmitter->id, tx_equalizer);
  SetTXAEQRun(transmitter->id, enable_tx_equalizer);
  SetRXAGrphEQ(active_receiver->id, rx_equalizer);
  SetRXAEQRun(active_receiver->id, enable_rx_equalizer);
}

void update_eq() {
#ifdef CLIENT_SERVER

  if (radio_is_remote) {
    //
    // insert here any function to inform the client of equalizer setting
    // changes, if this becomes part of the protocol
    //
  } else {
#endif
    set_eq();
#ifdef CLIENT_SERVER
  }

#endif
}

static void enable_cb (GtkWidget *widget, gpointer data) {
  int i = GPOINTER_TO_INT(data);
  int m;

  if (i != 0) {
    enable_tx_equalizer = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));
    m = vfo[get_tx_vfo()].mode;
    mode_settings[m].en_txeq = enable_tx_equalizer;
  } else {
    enable_rx_equalizer = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));
    m = vfo[active_receiver->id].mode;
    mode_settings[m].en_rxeq = enable_rx_equalizer;
  }

  update_eq();
  g_idle_add(ext_vfo_update, NULL);
}

static void rx_changed_cb (GtkWidget *widget, gpointer data) {
  int i = GPOINTER_TO_INT(data);
  int m;
  rx_equalizer[i] = (int)gtk_range_get_value(GTK_RANGE(widget));
  m = vfo[active_receiver->id].mode;
  mode_settings[m].rxeq[i] = rx_equalizer[i];
  update_eq();
}

static void tx_changed_cb (GtkWidget *widget, gpointer data) {
  int i = GPOINTER_TO_INT(data);
  int m;
  tx_equalizer[i] = (int)gtk_range_get_value(GTK_RANGE(widget));
  m = vfo[get_tx_vfo()].mode;
  mode_settings[m].txeq[i] = tx_equalizer[i];
  update_eq();
}

void equalizer_menu(GtkWidget *parent) {
  GtkWidget *label;
  dialog = gtk_dialog_new();
  gtk_window_set_transient_for(GTK_WINDOW(dialog), GTK_WINDOW(parent));
  gtk_window_set_title(GTK_WINDOW(dialog), "piHPSDR - Equalizer");
  g_signal_connect (dialog, "delete_event", G_CALLBACK (delete_event), NULL);
  set_backgnd(dialog);
  GtkWidget *content = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
  GtkWidget *grid = gtk_grid_new();
  gtk_grid_set_column_spacing (GTK_GRID(grid), 10);
  gtk_grid_set_row_spacing (GTK_GRID(grid), 5);
  gtk_grid_set_row_homogeneous(GTK_GRID(grid), FALSE);
  gtk_grid_set_column_homogeneous(GTK_GRID(grid), TRUE);
  GtkWidget *close_b = gtk_button_new_with_label("Close");
  g_signal_connect (close_b, "button-press-event", G_CALLBACK(close_cb), NULL);
  gtk_grid_attach(GTK_GRID(grid), close_b, 0, 0, 1, 1);
  label = gtk_label_new(NULL);
  gtk_label_set_markup(GTK_LABEL(label), "<b>RX Equalizer:</b>");
  gtk_grid_attach(GTK_GRID(grid), label, 1, 1, 1, 1);
  GtkWidget *enable_rx_b = gtk_check_button_new_with_label("Enable");
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (enable_rx_b), enable_rx_equalizer);
  g_signal_connect(enable_rx_b, "toggled", G_CALLBACK(enable_cb), GINT_TO_POINTER(0));
  gtk_grid_attach(GTK_GRID(grid), enable_rx_b, 2, 1, 1, 1);
  // This makes column #3 "empty" so it acts as a separator
  label = gtk_label_new(NULL);
  gtk_grid_attach(GTK_GRID(grid), label, 3, 1, 1, 1);
  label = gtk_label_new(NULL);
  gtk_label_set_markup(GTK_LABEL(label), "<b>TX Equalizer:</b>");
  gtk_grid_attach(GTK_GRID(grid), label, 4, 1, 1, 1);
  GtkWidget *enable_tx_b = gtk_check_button_new_with_label("Enable");
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (enable_tx_b), enable_tx_equalizer);
  g_signal_connect(enable_tx_b, "toggled", G_CALLBACK(enable_cb), GINT_TO_POINTER(1));
  gtk_grid_attach(GTK_GRID(grid), enable_tx_b, 5, 1, 1, 1);
  label = gtk_label_new(NULL);
  gtk_label_set_markup(GTK_LABEL(label), "<b>Preamp</b>");
  gtk_grid_attach(GTK_GRID(grid), label, 0, 2, 1, 1);
  label = gtk_label_new(NULL);
  gtk_label_set_markup(GTK_LABEL(label), "<b>Low</b>");
  gtk_grid_attach(GTK_GRID(grid), label, 0, 3, 1, 1);
  label = gtk_label_new(NULL);
  gtk_label_set_markup(GTK_LABEL(label), "<b>Mid</b>");
  gtk_grid_attach(GTK_GRID(grid), label, 0, 4, 1, 1);
  label = gtk_label_new(NULL);
  gtk_label_set_markup(GTK_LABEL(label), "<b>High</b>");
  gtk_grid_attach(GTK_GRID(grid), label, 0, 5, 1, 1);

  for (int i = 0; i < 4; i++) {
    for (int j = 0; j < 2; j++) {
      GtkWidget *scale = gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL, -12.0, 15.0, 1.0);
      gtk_range_set_increments (GTK_RANGE(scale), 1.0, 1.0);

      switch (j) {
      case 0:
        gtk_range_set_value(GTK_RANGE(scale), (double)rx_equalizer[i]);
        g_signal_connect(scale, "value-changed", G_CALLBACK(rx_changed_cb), GINT_TO_POINTER(i));
        break;

      case 1:
        gtk_range_set_value(GTK_RANGE(scale), (double)tx_equalizer[i]);
        g_signal_connect(scale, "value-changed", G_CALLBACK(tx_changed_cb), GINT_TO_POINTER(i));
        break;
      }

      gtk_grid_attach(GTK_GRID(grid), scale, 1 + 3 * j, i + 2, 2, 1);
      gtk_scale_add_mark(GTK_SCALE(scale), -12.0, GTK_POS_LEFT, "-12dB");
      gtk_scale_add_mark(GTK_SCALE(scale), -9.0, GTK_POS_LEFT, NULL);
      gtk_scale_add_mark(GTK_SCALE(scale), -6.0, GTK_POS_LEFT, NULL);
      gtk_scale_add_mark(GTK_SCALE(scale), -3.0, GTK_POS_LEFT, NULL);
      gtk_scale_add_mark(GTK_SCALE(scale), 0.0, GTK_POS_LEFT, "0dB");
      gtk_scale_add_mark(GTK_SCALE(scale), 3.0, GTK_POS_LEFT, NULL);
      gtk_scale_add_mark(GTK_SCALE(scale), 6.0, GTK_POS_LEFT, NULL);
      gtk_scale_add_mark(GTK_SCALE(scale), 9.0, GTK_POS_LEFT, NULL);
      gtk_scale_add_mark(GTK_SCALE(scale), 12.0, GTK_POS_LEFT, NULL);
      gtk_scale_add_mark(GTK_SCALE(scale), 15.0, GTK_POS_LEFT, "15dB");
    }
  }

  gtk_container_add(GTK_CONTAINER(content), grid);
  sub_menu = dialog;
  gtk_widget_show_all(dialog);
}

