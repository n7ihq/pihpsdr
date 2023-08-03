/*
* This program is free software; you can redistribute it and/or
* modify it under the terms of the GNU General Public License
* as published by the Free Software Foundation; either version 2
* of the License, or (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program; if not, write to the Free Software
* Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
*
*/

#include <gtk/gtk.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "new_menu.h"
#include "filter_menu.h"
#include "band.h"
#include "bandstack.h"
#include "filter.h"
#include "mode.h"
#include "radio.h"
#include "receiver.h"
#include "vfo.h"
#include "button_text.h"
#include "ext.h"
#include "message.h"

static GtkWidget *dialog = NULL;

static GtkWidget *last_filter;
static GtkWidget *var1_spin_low;
static GtkWidget *var1_spin_high;
static GtkWidget *var2_spin_low;
static GtkWidget *var2_spin_high;


static void cleanup() {
  if (dialog != NULL) {
    gtk_widget_destroy(dialog);
    dialog = NULL;
    sub_menu = NULL;
    active_menu = NO_MENU;
  }
}

static gboolean default_cb (GtkWidget *widget, GdkEventButton *event, gpointer data) {
  int mode = vfo[active_receiver->id].mode;
  int f = GPOINTER_TO_INT(data);
  int low, high;
  GtkWidget *spinlow, *spinhigh;

  switch (f) {
  case filterVar1:
    spinlow = var1_spin_low;
    spinhigh = var1_spin_high;
    low = var1_default_low[mode];
    high = var1_default_high[mode];
    break;

  case filterVar2:
    spinlow = var2_spin_low;
    spinhigh = var2_spin_high;
    low = var2_default_low[mode];
    high = var2_default_high[mode];
    break;

  default:
    t_print("%s: illegal data = %p (%d)\n", __FUNCTION__, data, f);
    return FALSE;
    break;
  }

  switch (mode) {
  case modeCWL:
  case modeCWU:
  case modeAM:
  case modeDSB:
  case modeSAM:
  case modeSPEC:
  case modeDRM:
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(spinlow), (double)(high - low));
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(spinhigh), 0.5 * (double)(high + low));
    break;

  case modeLSB:
  case modeDIGL:
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(spinlow), (double)(-high));
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(spinhigh), (double)(-low));
    break;

  case modeUSB:
  case modeDIGU:
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(spinlow), (double)(low));
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(spinhigh), (double)(high));
    break;
  }

  return FALSE;
}

static gboolean close_cb (GtkWidget *widget, GdkEventButton *event, gpointer data) {
  cleanup();
  return TRUE;
}

static gboolean delete_event(GtkWidget *widget, GdkEvent *event, gpointer user_data) {
  cleanup();
  return FALSE;
}

int filter_select(void *data) {
  int f = GPOINTER_TO_UINT(data);
  vfo_filter_changed(f);
  return 0;
}

static gboolean filter_select_cb (GtkWidget *widget, GdkEventButton *event, gpointer data) {
  filter_select(data);
  set_button_text_color(last_filter, "default");
  last_filter = widget;
  set_button_text_color(last_filter, "orange");
  return FALSE;
}

static gboolean deviation_select_cb (GtkWidget *widget, GdkEventButton *event, gpointer data) {
  active_receiver->deviation = GPOINTER_TO_UINT(data);
  transmitter->deviation = GPOINTER_TO_UINT(data);
  set_filter(active_receiver);
  tx_set_filter(transmitter);
  set_deviation(active_receiver);
  transmitter_set_deviation(transmitter);
  set_button_text_color(last_filter, "default");
  last_filter = widget;
  set_button_text_color(last_filter, "orange");
  g_idle_add(ext_vfo_update, NULL);
  return FALSE;
}

//
// var_spin_low controls the width for modes that ajust width/shift
//
static void var_spin_low_cb (GtkWidget *widget, gpointer data) {
  int f = GPOINTER_TO_UINT(data);
  int id = active_receiver->id;
  int m = vfo[id].mode;
  FILTER *mode_filters = filters[m];
  FILTER *filter = &mode_filters[f];
  int val = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(widget));
  int shift;

  switch (m) {
  case modeCWL:
  case modeCWU:
  case modeDSB:
  case modeAM:
  case modeSAM:
  case modeSPEC:
  case modeDRM:
    //
    // Set new width but keep the current shift
    //
    shift = (filter->low + filter->high) / 2;
    filter->low = shift - val / 2;
    filter->high = shift + val / 2;
    break;

  case modeLSB:
  case modeDIGL:
    //
    // The value corresponds to an audio frequency
    //
    filter->high = -val;
    break;

  case modeUSB:
  case modeDIGU:
    filter->low = val;
    break;
  }

  t_print("%s: new values=(%d:%d)\n", __FUNCTION__, filter->low, filter->high);

  //
  // Change all receivers that use *this* variable filter
  //
  for (int i = 0; i < receivers; i++) {
    if (vfo[i].filter == f) {
      receiver_filter_changed(receiver[i]);
    }
  }

  g_idle_add(ext_vfo_update, NULL);
}

//
// var_spin_low controls the shift for modes that ajust width/shift
//
static void var_spin_high_cb (GtkWidget *widget, gpointer data) {
  int f = GPOINTER_TO_UINT(data);
  int id = active_receiver->id;
  FILTER *mode_filters = filters[vfo[id].mode];
  FILTER *filter = &mode_filters[f];
  int m = vfo[id].mode;
  int val = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(widget));
  int width;

  switch (m) {
  case modeCWL:
    //
    // Set new shift but keep the current width.
    // (The shift is in the audio domain)
    //
    width = (filter->high - filter->low);
    filter->low = -val - width / 2;
    filter->high = -val + width / 2;
    break;

  case modeCWU:
  case modeDSB:
  case modeAM:
  case modeSAM:
  case modeSPEC:
  case modeDRM:
    //
    // Set new shift but keep the current width
    //
    width = (filter->high - filter->low);
    filter->low = val - width / 2;
    filter->high = val + width / 2;
    break;

  case modeLSB:
  case modeDIGL:
    //
    // The value corresponds to an audio frequency
    //
    filter->low = -val;
    break;

  case modeUSB:
  case modeDIGU:
    filter->high = val;
    break;
  }

  t_print("%s: new values=(%d:%d)\n", __FUNCTION__, filter->low, filter->high);

  //
  // Change all receivers that use *this* variable filter
  //
  for (int i = 0; i < receivers; i++) {
    if (vfo[i].filter == f) {
      receiver_filter_changed(receiver[i]);
    }
  }

  g_idle_add(ext_vfo_update, NULL);
}

void filter_menu(GtkWidget *parent) {
  int id = active_receiver->id;
  int f = vfo[id].filter;
  int m = vfo[id].mode;
  GtkWidget *w;
  dialog = gtk_dialog_new();
  gtk_window_set_transient_for(GTK_WINDOW(dialog), GTK_WINDOW(parent));
  char title[64];
  sprintf(title, "piHPSDR - Filter (RX %d VFO %s)", id, id == 0 ? "A" : "B");
  gtk_window_set_title(GTK_WINDOW(dialog), title);
  g_signal_connect (dialog, "delete_event", G_CALLBACK (delete_event), NULL);
  set_backgnd(dialog);
  GtkWidget *content = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
  GtkWidget *grid = gtk_grid_new();
  gtk_grid_set_column_homogeneous(GTK_GRID(grid), TRUE);
  gtk_grid_set_row_homogeneous(GTK_GRID(grid), TRUE);
  gtk_grid_set_column_spacing (GTK_GRID(grid), 5);
  gtk_grid_set_row_spacing (GTK_GRID(grid), 5);
  w = gtk_button_new_with_label("Close");
  g_signal_connect (w, "button-press-event", G_CALLBACK(close_cb), NULL);
  gtk_grid_attach(GTK_GRID(grid), w, 0, 0, 1, 1);
  FILTER* band_filters = filters[m];

  if (m == modeFMN) {
    GtkWidget *l = gtk_label_new("Deviation:");
    gtk_grid_attach(GTK_GRID(grid), l, 0, 1, 1, 1);
    w = gtk_button_new_with_label("2.5K");

    if (active_receiver->deviation == 2500) {
      set_button_text_color(w, "orange");
      last_filter = w;
    } else {
      set_button_text_color(w, "default");
    }

    g_signal_connect(w, "button-press-event", G_CALLBACK(deviation_select_cb), GINT_TO_POINTER(2500));
    gtk_grid_attach(GTK_GRID(grid), w, 1, 1, 1, 1);
    w = gtk_button_new_with_label("5.0K");

    if (active_receiver->deviation == 5000) {
      set_button_text_color(w, "orange");
      last_filter = w;
    } else {
      set_button_text_color(w, "default");
    }

    g_signal_connect(w, "button-press-event", G_CALLBACK(deviation_select_cb), GINT_TO_POINTER(5000));
    gtk_grid_attach(GTK_GRID(grid), w, 2, 1, 1, 1);
  } else {
    int row = 0;
    int col = 5;

    for (int i = 0; i < filterVar1; i++) {
      if (col >= 5) {
        col = 0;
        row++;
      }

      w = gtk_button_new_with_label(band_filters[i].title);

      if (i == f) {
        set_button_text_color(w, "orange");
        last_filter = w;
      } else {
        set_button_text_color(w, "default");
      }

      gtk_grid_attach(GTK_GRID(grid), w, col, row, 1, 1);
      g_signal_connect(w, "button-press-event", G_CALLBACK(filter_select_cb), GINT_TO_POINTER(i));
      col++;
    }

    //
    // Var1 and Var2
    //
    row++;
    const FILTER* filter1 = &band_filters[filterVar1];
    const FILTER* filter2 = &band_filters[filterVar2];
    w = gtk_button_new_with_label(band_filters[filterVar1].title);
    gtk_grid_attach(GTK_GRID(grid), w, 0, row, 1, 1);

    if (f == filterVar1) {
      set_button_text_color(w, "orange");
      last_filter = w;
    } else {
      set_button_text_color(w, "default");
    }

    g_signal_connect(w, "button-press-event", G_CALLBACK(filter_select_cb),
                     GINT_TO_POINTER(filterVar1));
    w = gtk_button_new_with_label(band_filters[filterVar2].title);
    gtk_grid_attach(GTK_GRID(grid), w, 0, row + 1, 1, 1);

    if (f == filterVar2) {
      set_button_text_color(w, "orange");
      last_filter = w;
    } else {
      set_button_text_color(w, "default");
    }

    g_signal_connect(w, "button-press-event", G_CALLBACK(filter_select_cb),
                     GINT_TO_POINTER(filterVar2));

    //
    // The spin buttons either control low/high or width/shift
    //
    switch (m) {
    case modeCWL:
    case modeCWU:
    case modeDSB:
    case modeAM:
    case modeSAM:
    case modeSPEC:
    case modeDRM:
      w = gtk_label_new("Filter Width:");
      gtk_grid_attach(GTK_GRID(grid), w, 1, row, 1, 1);
      w = gtk_label_new("Filter Width:");
      gtk_grid_attach(GTK_GRID(grid), w, 1, row + 1, 1, 1);
      var1_spin_low = gtk_spin_button_new_with_range(10.0, 16000.0, 5.0);
      gtk_spin_button_set_value(GTK_SPIN_BUTTON(var1_spin_low), (double)(filter1->high - filter1->low));
      var2_spin_low = gtk_spin_button_new_with_range(10.0, 8000.0, 5.0);
      gtk_spin_button_set_value(GTK_SPIN_BUTTON(var2_spin_low), (double)(filter2->high - filter2->low));
      w = gtk_label_new("Filter Shift:");
      gtk_grid_attach(GTK_GRID(grid), w, 3, row, 1, 1);
      w = gtk_label_new("Filter Shift:");
      gtk_grid_attach(GTK_GRID(grid), w, 3, row + 1, 1, 1);
      var1_spin_high = gtk_spin_button_new_with_range(-8000.0, 8000.0, 5.0);
      gtk_spin_button_set_value(GTK_SPIN_BUTTON(var1_spin_high), 0.5 * (double)(filter1->high + filter1->low));
      var2_spin_high = gtk_spin_button_new_with_range(-8000.0, 8000.0, 5.0);
      gtk_spin_button_set_value(GTK_SPIN_BUTTON(var2_spin_high),
                                0.5 * (double)(filter2->high + filter2->low));
      break;

    case modeLSB:
    case modeDIGL:
      w = gtk_label_new("Filter Low:");
      gtk_grid_attach(GTK_GRID(grid), w, 1, row, 1, 1);
      w = gtk_label_new("Filter Low:");
      gtk_grid_attach(GTK_GRID(grid), w, 1, row + 1, 1, 1);
      var1_spin_low = gtk_spin_button_new_with_range(0, 8000.0, 5.0);
      gtk_spin_button_set_value(GTK_SPIN_BUTTON(var1_spin_low), (double)(-filter1->high));
      var2_spin_low = gtk_spin_button_new_with_range(0, 8000.0, 5.0);
      gtk_spin_button_set_value(GTK_SPIN_BUTTON(var2_spin_low), (double)(-filter2->high));
      w = gtk_label_new("Filter High:");
      gtk_grid_attach(GTK_GRID(grid), w, 3, row, 1, 1);
      w = gtk_label_new("Filter High:");
      gtk_grid_attach(GTK_GRID(grid), w, 3, row + 1, 1, 1);
      var1_spin_high = gtk_spin_button_new_with_range(0, 8000.0, 5.0);
      gtk_spin_button_set_value(GTK_SPIN_BUTTON(var1_spin_high), (double)(-filter1->low));
      var2_spin_high = gtk_spin_button_new_with_range(0, 8000.0, 5.0);
      gtk_spin_button_set_value(GTK_SPIN_BUTTON(var2_spin_high), (double)(-filter2->low));
      break;

    case modeUSB:
    case modeDIGU:
      w = gtk_label_new("Filter Low:");
      gtk_grid_attach(GTK_GRID(grid), w, 1, row, 1, 1);
      w = gtk_label_new("Filter Low:");
      gtk_grid_attach(GTK_GRID(grid), w, 1, row + 1, 1, 1);
      var1_spin_low = gtk_spin_button_new_with_range(-8000, 8000.0, 5.0);
      gtk_spin_button_set_value(GTK_SPIN_BUTTON(var1_spin_low), (double)(filter1->low));
      var2_spin_low = gtk_spin_button_new_with_range(-8000, 8000.0, 5.0);
      gtk_spin_button_set_value(GTK_SPIN_BUTTON(var2_spin_low), (double)(filter2->low));
      w = gtk_label_new("Filter High:");
      gtk_grid_attach(GTK_GRID(grid), w, 3, row, 1, 1);
      w = gtk_label_new("Filter High:");
      gtk_grid_attach(GTK_GRID(grid), w, 3, row + 1, 1, 1);
      var1_spin_high = gtk_spin_button_new_with_range(-8000, 8000.0, 5.0);
      gtk_spin_button_set_value(GTK_SPIN_BUTTON(var1_spin_high), (double)(filter1->high));
      var2_spin_high = gtk_spin_button_new_with_range(-8000, 8000.0, 5.0);
      gtk_spin_button_set_value(GTK_SPIN_BUTTON(var2_spin_high), (double)(filter2->high));
      break;
    }

    gtk_grid_attach(GTK_GRID(grid), var1_spin_low, 2, row, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), var2_spin_low, 2, row + 1, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), var1_spin_high, 4, row, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), var2_spin_high, 4, row + 1, 1, 1);
    g_signal_connect(var1_spin_low, "value-changed", G_CALLBACK(var_spin_low_cb), GINT_TO_POINTER(filterVar1));
    g_signal_connect(var2_spin_low, "value-changed", G_CALLBACK(var_spin_low_cb), GINT_TO_POINTER(filterVar2));
    g_signal_connect(var1_spin_high, "value-changed", G_CALLBACK(var_spin_high_cb), GINT_TO_POINTER(filterVar1));
    g_signal_connect(var2_spin_high, "value-changed", G_CALLBACK(var_spin_high_cb), GINT_TO_POINTER(filterVar2));
    GtkWidget *var1_default_b = gtk_button_new_with_label("Default");
    g_signal_connect (var1_default_b, "button-press-event", G_CALLBACK(default_cb), GINT_TO_POINTER(filterVar1));
    gtk_grid_attach(GTK_GRID(grid), var1_default_b, 5, row, 1, 1);
    GtkWidget *var2_default_b = gtk_button_new_with_label("Default");
    g_signal_connect (var2_default_b, "button-press-event", G_CALLBACK(default_cb), GINT_TO_POINTER(filterVar2));
    gtk_grid_attach(GTK_GRID(grid), var2_default_b, 5, row + 1, 1, 1);
  }

  gtk_container_add(GTK_CONTAINER(content), grid);
  sub_menu = dialog;
  gtk_widget_show_all(dialog);
}
