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
#include <stdint.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>
#include <stdbool.h>

#include "new_menu.h"
#include "saturn_menu.h"
#include "saturnserver.h"
#include "radio.h"

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

static void server_enable_cb(GtkWidget *widget, gpointer data) {
  saturn_server_en = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));

  if (saturn_server_en) {
    start_saturn_server();
  } else {
    shutdown_saturn_server();
  }
}

static void client_enable_tx_cb(GtkWidget *widget, gpointer data) {
  if (!saturn_server_en) { gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget), 0); }
  else { client_enable_tx = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget)); }
}

void saturn_menu(GtkWidget *parent) {
  GtkWidget *widget;
  dialog = gtk_dialog_new();
  gtk_window_set_transient_for(GTK_WINDOW(dialog), GTK_WINDOW(parent));
  gtk_window_set_title(GTK_WINDOW(dialog), "piHPSDR - Saturn");
  g_signal_connect (dialog, "delete_event", G_CALLBACK (delete_event), NULL);
  set_backgnd(dialog);
  GtkWidget *content = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
  GtkWidget *grid = gtk_grid_new();
  gtk_grid_set_column_spacing (GTK_GRID(grid), 10);
  GtkWidget *close_b = gtk_button_new_with_label("Close");
  g_signal_connect (close_b, "pressed", G_CALLBACK(close_cb), NULL);
  gtk_grid_attach(GTK_GRID(grid), close_b, 0, 0, 1, 1);
  widget = gtk_label_new(NULL);
  gtk_widget_set_name(widget, "small_button");
  gtk_grid_attach(GTK_GRID(grid), widget, 0, 1, 1, 1);
  GtkWidget *server_enable_b = gtk_check_button_new_with_label("Saturn Server Enable");
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (server_enable_b), saturn_server_en);
  gtk_widget_show(server_enable_b);
  gtk_grid_attach(GTK_GRID(grid), server_enable_b, 0, 2, 1, 1);
  g_signal_connect(server_enable_b, "toggled", G_CALLBACK(server_enable_cb), NULL);
  GtkWidget *client_enable_tx_b = gtk_check_button_new_with_label("Client Transmit Enable");
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (client_enable_tx_b), client_enable_tx);
  gtk_widget_show(client_enable_tx_b);
  gtk_grid_attach(GTK_GRID(grid), client_enable_tx_b, 0, 3, 1, 1);
  g_signal_connect(client_enable_tx_b, "toggled", G_CALLBACK(client_enable_tx_cb), NULL);
  gtk_container_add(GTK_CONTAINER(content), grid);
  sub_menu = dialog;
  gtk_widget_show_all(dialog);
}

