/* Copyright (C)
* 2017 - John Melton, G0ORX/N6LYT
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
#include <semaphore.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <wdsp.h>

#include "new_menu.h"
#include "fft_menu.h"
#include "radio.h"
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

static void filter_type_cb(GtkToggleButton *widget, gpointer data) {
  int type = gtk_combo_box_get_active (GTK_COMBO_BOX(widget));
  int channel  = GPOINTER_TO_INT(data);

  switch (channel) {
  case 0:
  case 1:
    receiver[channel]->low_latency = type;
    RXASetMP(channel, type);
    break;

  case 8:
    transmitter->low_latency = type;
    TXASetMP(transmitter->id, type);
    break;
  }

  //t_print("WDSP filter type channel=%d changed to %d\n", channel, type);
}

static void filter_size_cb(GtkWidget *widget, gpointer data) {
  int channel = GPOINTER_TO_INT(data);
  char *p = gtk_combo_box_text_get_active_text(GTK_COMBO_BOX_TEXT(widget));
  int size;

  // Get size from string in the combobox
  if (sscanf(p, "%d", &size) != 1) { return; }

  switch (channel) {
  case 0:
  case 1:
    receiver[channel]->fft_size = size;
    RXASetNC(channel, size);
    break;

  case 8:
    transmitter->fft_size = size;;
    TXASetNC(transmitter->id, size);
    break;
  }

  //t_print("WDSP filter size channel=%d changed to %d\n", channel, size);
}

void fft_menu(GtkWidget *parent) {
  GtkWidget *wid;
  dialog = gtk_dialog_new();
  gtk_window_set_transient_for(GTK_WINDOW(dialog), GTK_WINDOW(parent));
  gtk_window_set_title(GTK_WINDOW(dialog), "piHPSDR - FFT");
  g_signal_connect (dialog, "delete_event", G_CALLBACK (delete_event), NULL);
  set_backgnd(dialog);
  GtkWidget *content = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
  GtkWidget *grid = gtk_grid_new();
  gtk_grid_set_column_spacing (GTK_GRID(grid), 10);
  wid = gtk_button_new_with_label("Close");
  g_signal_connect (wid, "button_press_event", G_CALLBACK(close_cb), NULL);
  gtk_grid_attach(GTK_GRID(grid), wid, 0, 0, 1, 1);
  wid = gtk_label_new(NULL);
  gtk_label_set_markup(GTK_LABEL(wid), "<b>Filter Type</b>");
  gtk_grid_attach(GTK_GRID(grid), wid, 0, 2, 1, 1);
  wid = gtk_label_new(NULL);
  gtk_label_set_markup(GTK_LABEL(wid), "<b>Filter Size</b>");
  gtk_grid_attach(GTK_GRID(grid), wid, 0, 3, 1, 1);
  int col = 1;

  for (int i = 0; i <= receivers; i++) {
    // i == receivers means "TX"
    int chan;
    int j, s, dsize, fsize, ftype;
    char text[32];

    if ((i == receivers) && !can_transmit) { break; }

    wid = gtk_label_new(NULL);

    if (i == 0) {
      gtk_label_set_markup(GTK_LABEL(wid), "<b>RX1</b>");
      chan = 0;
      fsize = receiver[0]->fft_size;
      dsize = receiver[0]->dsp_size;
      ftype = receiver[0]->low_latency;
    } else if (i == receivers - 1) {
      gtk_label_set_markup(GTK_LABEL(wid), "<b>RX2</b>");
      chan = 1;
      fsize = receiver[1]->fft_size;
      dsize = receiver[1]->fft_size;
      ftype = receiver[1]->low_latency;
    } else {
      gtk_label_set_markup(GTK_LABEL(wid), "<b>TX</b>");
      chan = 8;
      fsize = transmitter->fft_size;
      dsize = transmitter->dsp_size;
      ftype = transmitter->low_latency;
    }

    // chan: actual channel, fsize: actual size value, dsize: minimum size value
    gtk_grid_attach(GTK_GRID(grid), wid, col, 1, 1, 1);
    wid = gtk_combo_box_text_new();
    gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(wid), NULL, "Linear Phase");
    gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(wid), NULL, "Low Latency");
    gtk_combo_box_set_active(GTK_COMBO_BOX(wid), ftype);
    my_combo_attach(GTK_GRID(grid), wid, col, 2, 1, 1);
    g_signal_connect(wid, "changed", G_CALLBACK(filter_type_cb), GINT_TO_POINTER(chan));
    //
    // The filter size must be a power of two and at least equal to the dsp size
    // Apart from that, we allow values from 1024 ... 16384.
    //
    wid = gtk_combo_box_text_new();
    s = 512;
    j = 0;

    for (;;) {
      s = 2 * s;

      if (s >= dsize) {
        sprintf(text, "%d", s);
        gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(wid), NULL, text);

        if (s == fsize) { gtk_combo_box_set_active(GTK_COMBO_BOX(wid), j); }

        j++;
      }

      if (s >= 16384) { break; }
    }

    my_combo_attach(GTK_GRID(grid), wid, col, 3, 1, 1);
    g_signal_connect(wid, "changed", G_CALLBACK(filter_size_cb), GINT_TO_POINTER(chan));
    col++;
  }

  gtk_container_add(GTK_CONTAINER(content), grid);
  sub_menu = dialog;
  gtk_widget_show_all(dialog);
}

