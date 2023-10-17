/* Copyright (C)
* 2020 - John Melton, G0ORX/N6LYT
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
#include <stdlib.h>
#include <math.h>

#include "appearance.h"
#include "main.h"
#include "receiver.h"
#include "radio.h"
#include "vfo.h"
#include "sliders.h"
#include "zoompan.h"
#ifdef CLIENT_SERVER
  #include "client_server.h"
#endif
#include "actions.h"
#include "ext.h"
#include "message.h"

static int width;
static int height;

static GtkWidget *zoompan;
static GtkWidget *zoom_label;
static GtkWidget *zoom_scale;
static gulong zoom_signal_id;
static GtkWidget *pan_label;
static GtkWidget *pan_scale;
static gulong pan_signal_id;
static GMutex pan_zoom_mutex;

int zoompan_active_receiver_changed(void *data) {
  if (display_zoompan) {
    g_mutex_lock(&pan_zoom_mutex);
    g_signal_handler_block(G_OBJECT(zoom_scale), zoom_signal_id);
    g_signal_handler_block(G_OBJECT(pan_scale), pan_signal_id);
    gtk_range_set_value(GTK_RANGE(zoom_scale), active_receiver->zoom);
    gtk_range_set_range(GTK_RANGE(pan_scale), 0.0,
                        (double)(active_receiver->zoom == 1 ? active_receiver->pixels : active_receiver->pixels - active_receiver->width));
    gtk_range_set_value (GTK_RANGE(pan_scale), active_receiver->pan);

    if (active_receiver->zoom == 1) {
      gtk_widget_set_sensitive(pan_scale, FALSE);
    }

    g_signal_handler_unblock(G_OBJECT(pan_scale), pan_signal_id);
    g_signal_handler_unblock(G_OBJECT(zoom_scale), zoom_signal_id);
    g_mutex_unlock(&pan_zoom_mutex);
  }

  return FALSE;
}

static void zoom_value_changed_cb(GtkWidget *widget, gpointer data) {
  //t_print("zoom_value_changed_cb\n");
  g_mutex_lock(&pan_zoom_mutex);
  g_mutex_lock(&active_receiver->display_mutex);
#ifdef CLIENT_SERVER

  if (radio_is_remote) {
    int zoom = (int)gtk_range_get_value(GTK_RANGE(zoom_scale));
    active_receiver->zoom = zoom;
    send_zoom(client_socket, active_receiver->id, zoom);
  } else {
#endif
    active_receiver->zoom = (int)(gtk_range_get_value(GTK_RANGE(zoom_scale)) + 0.5);;
    receiver_update_zoom(active_receiver);
#ifdef CLIENT_SERVER
  }

#endif
  g_signal_handler_block(G_OBJECT(pan_scale), pan_signal_id);
  gtk_range_set_range(GTK_RANGE(pan_scale), 0.0,
                      (double)(active_receiver->zoom == 1 ? active_receiver->pixels : active_receiver->pixels - active_receiver->width));
  gtk_range_set_value (GTK_RANGE(pan_scale), active_receiver->pan);
  g_signal_handler_unblock(G_OBJECT(pan_scale), pan_signal_id);

  if (active_receiver->zoom == 1) {
    gtk_widget_set_sensitive(pan_scale, FALSE);
  } else {
    gtk_widget_set_sensitive(pan_scale, TRUE);
  }

  g_mutex_unlock(&active_receiver->display_mutex);
  g_mutex_unlock(&pan_zoom_mutex);
  g_idle_add(ext_vfo_update, NULL);
}

void set_zoom(int rx, double value) {
  //t_print("set_zoom: %f\n",value);
  if (rx >= receivers) { return; }

  int ival = (int) value;

  if (ival > MAX_ZOOM) { ival = MAX_ZOOM; }

  if (ival < 1       ) { ival = 1; }

  receiver[rx]->zoom = ival;
  receiver_update_zoom(receiver[rx]);

  if (display_zoompan && active_receiver->id == rx) {
    gtk_range_set_value (GTK_RANGE(zoom_scale), receiver[rx]->zoom);
  } else {
    char title[64];
    snprintf(title, 64, "Zoom RX %d", rx);
    show_popup_slider(ZOOM, rx, 1.0, MAX_ZOOM, 1.0, receiver[rx]->zoom, title);
  }

  g_idle_add(ext_vfo_update, NULL);
}

void remote_set_zoom(int rx, double value) {
  //t_print("remote_set_zoom: rx=%d zoom=%f\n",rx,value);
  g_mutex_lock(&pan_zoom_mutex);
  g_signal_handler_block(G_OBJECT(zoom_scale), zoom_signal_id);
  g_signal_handler_block(G_OBJECT(pan_scale), pan_signal_id);
  set_zoom(rx, value);
  g_signal_handler_unblock(G_OBJECT(pan_scale), pan_signal_id);
  g_signal_handler_unblock(G_OBJECT(zoom_scale), zoom_signal_id);
  g_mutex_unlock(&pan_zoom_mutex);
  //t_print("remote_set_zoom: EXIT\n");
}

static void pan_value_changed_cb(GtkWidget *widget, gpointer data) {
  //t_print("pan_value_changed_cb\n");
  g_mutex_lock(&pan_zoom_mutex);
#ifdef CLIENT_SERVER

  if (radio_is_remote) {
    int pan = (int)gtk_range_get_value(GTK_RANGE(pan_scale));
    send_pan(client_socket, active_receiver->id, pan);
    g_mutex_unlock(&pan_zoom_mutex);
    return;
  }

#endif

  if (active_receiver->zoom > 1) {
    active_receiver->pan = (int)(gtk_range_get_value(GTK_RANGE(pan_scale)) + 0.5);
  }

  g_mutex_unlock(&pan_zoom_mutex);
}

void set_pan(int rx, double value) {
  //t_print("set_pan: value=%f\n",value);
  if (rx >= receivers) { return; }

  if (receiver[rx]->zoom == 1) {
    receiver[rx]->pan = 0;
    return;
  }

  int ival = (int) value;

  if (ival < 0) { ival = 0; }

  if (ival > (receiver[rx]->pixels - receiver[rx]->width)) { ival = receiver[rx]->pixels - receiver[rx]->width; }

  receiver[rx]->pan = ival;

  if (display_zoompan && rx == active_receiver->id) {
    gtk_range_set_value (GTK_RANGE(pan_scale), receiver[rx]->pan);
  } else {
    char title[64];
    snprintf(title, 64, "Pan RX %d", rx);
    show_popup_slider(PAN, rx, 0.0, receiver[rx]->pixels - receiver[rx]->width, 1.00, receiver[rx]->pan, title);
  }
}

void remote_set_pan(int rx, double value) {
  //t_print("remote_set_pan: rx=%d pan=%f\n",rx,value);
  if (rx >= receivers) { return; }

  g_mutex_lock(&pan_zoom_mutex);
  g_signal_handler_block(G_OBJECT(pan_scale), pan_signal_id);
  gtk_range_set_range(GTK_RANGE(pan_scale), 0.0,
                      (double)(receiver[rx]->zoom == 1 ? receiver[rx]->pixels : receiver[rx]->pixels - receiver[rx]->width));
  set_pan(rx, value);
  g_signal_handler_unblock(G_OBJECT(pan_scale), pan_signal_id);
  g_mutex_unlock(&pan_zoom_mutex);
  //t_print("remote_set_pan: EXIT\n");
}

GtkWidget *zoompan_init(int my_width, int my_height) {
  width = my_width;
  height = my_height;
  //t_print("%s: width=%d height=%d\n", __FUNCTION__,width,height);
  zoompan = gtk_grid_new();
  gtk_widget_set_size_request (zoompan, width, height);
  gtk_grid_set_row_homogeneous(GTK_GRID(zoompan), FALSE);
  gtk_grid_set_column_homogeneous(GTK_GRID(zoompan), TRUE);
  zoom_label = gtk_label_new("Zoom:");
  gtk_widget_set_name(zoom_label, "boldlabel");
  gtk_widget_set_halign(zoom_label, GTK_ALIGN_END);
  gtk_widget_show(zoom_label);
  gtk_grid_attach(GTK_GRID(zoompan), zoom_label, 0, 0, 2, 1);
  zoom_scale = gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL, 1.0, MAX_ZOOM, 1.00);
  gtk_widget_set_size_request(zoom_scale, 0, height);
  gtk_widget_set_valign(zoom_scale, GTK_ALIGN_CENTER);
  gtk_range_set_increments (GTK_RANGE(zoom_scale), 1.0, 1.0);
  gtk_range_set_value (GTK_RANGE(zoom_scale), active_receiver->zoom);
  gtk_widget_show(zoom_scale);
  gtk_grid_attach(GTK_GRID(zoompan), zoom_scale, 2, 0, 4, 1);
  zoom_signal_id = g_signal_connect(G_OBJECT(zoom_scale), "value_changed", G_CALLBACK(zoom_value_changed_cb), NULL);
  pan_label = gtk_label_new("Pan:");
  gtk_widget_set_name(pan_label, "boldlabel");
  gtk_widget_set_halign(pan_label, GTK_ALIGN_END);
  gtk_widget_show(pan_label);
  gtk_grid_attach(GTK_GRID(zoompan), pan_label, 6, 0, 2, 1);
  pan_scale = gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL, 0.0,
                                       active_receiver->zoom == 1 ? active_receiver->width : active_receiver->width * (active_receiver->zoom - 1), 1.0);
  gtk_widget_set_size_request(pan_scale, 0, height);
  gtk_widget_set_valign(pan_scale, GTK_ALIGN_CENTER);
  gtk_scale_set_draw_value (GTK_SCALE(pan_scale), FALSE);
  gtk_range_set_increments (GTK_RANGE(pan_scale), 10.0, 10.0);
  gtk_range_set_value (GTK_RANGE(pan_scale), active_receiver->pan);
  gtk_widget_show(pan_scale);
  gtk_grid_attach(GTK_GRID(zoompan), pan_scale, 8, 0, 10, 1);
  pan_signal_id = g_signal_connect(G_OBJECT(pan_scale), "value_changed", G_CALLBACK(pan_value_changed_cb), NULL);

  if (active_receiver->zoom == 1) {
    gtk_widget_set_sensitive(pan_scale, FALSE);
  }

  g_mutex_init(&pan_zoom_mutex);
  return zoompan;
}
