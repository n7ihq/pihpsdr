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
#include <string.h>
#include <termios.h>
#include <unistd.h>

#include "new_menu.h"
#include "rigctl_menu.h"
#include "rigctl.h"
#include "band.h"
#include "radio.h"
#include "vfo.h"
#include "message.h"

static GtkWidget *serial_enable_b[MAX_SERIAL];
static GtkWidget *andromeda_b[MAX_SERIAL];
static GtkWidget *baud_combo[MAX_SERIAL];

static GtkWidget *dialog = NULL;
static GtkWidget *serial_port_entry;

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

static void rigctl_value_changed_cb(GtkWidget *widget, gpointer data) {
  rigctl_port_base = gtk_spin_button_get_value(GTK_SPIN_BUTTON(widget));
}

static void rigctl_debug_cb(GtkWidget *widget, gpointer data) {
  rigctl_debug = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));
  t_print("---------- RIGCTL DEBUG %s ----------\n", rigctl_debug ? "ON" : "OFF");
}

static void rigctl_enable_cb(GtkWidget *widget, gpointer data) {
  rigctl_enable = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));

  if (rigctl_enable) {
    launch_rigctl();
  } else {
    //
    // If serial server is running, terminate it.
    // Disable serial and clear check-box
    //
    for (int id = 0; id < MAX_SERIAL; id++) {
      if (SerialPorts[id].enable) {
        disable_serial(id);
      }

      SerialPorts[id].enable = 0;
      gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON(serial_enable_b[id]), 0);
    }

    disable_rigctl();
  }
}

static void serial_port_cb(GtkWidget *widget, gpointer data) {
  int id = GPOINTER_TO_INT(data);
  const char *cp = gtk_entry_get_text(GTK_ENTRY(widget));
  strlcpy(SerialPorts[id].port, cp, sizeof(SerialPorts[id].port));
}

static void andromeda_cb(GtkWidget *widget, gpointer data) {
  int id = GPOINTER_TO_INT(data);
  SerialPorts[id].andromeda = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));

  if (SerialPorts[id].andromeda) {
    if (SerialPorts[id].enable) {
      launch_andromeda(id);
    }

    gtk_combo_box_set_active(GTK_COMBO_BOX(baud_combo[id]), 1);
    SerialPorts[id].baud = B9600;
  } else {
    disable_andromeda(id);
  }
}

static void serial_enable_cb(GtkWidget *widget, gpointer data) {
  //
  // If rigctl is not running, serial cannot be enabled
  //
  int id = GPOINTER_TO_INT(data);

  if (!rigctl_enable) {
    SerialPorts[id].enable = 0;
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget), 0);
    return;
  }

  if ((SerialPorts[id].enable = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget)))) {
    if (launch_serial(id) == 0) {
      gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget), FALSE);
      SerialPorts[id].enable = 0;
    }
  } else {
    disable_serial(id);
  }

  t_print("%s: Serial enable : ID=%d Enabled=%d\n", __FUNCTION__, id, SerialPorts[id].enable);
}

// Set Baud Rate
static void baud_cb(GtkWidget *widget, gpointer data) {
  int id = GPOINTER_TO_INT(data);
  int bd = gtk_combo_box_get_active(GTK_COMBO_BOX(widget));
  int new;

  //
  // If ANDROMEDA is active, keep 9600
  //
  if (SerialPorts[id].andromeda && SerialPorts[id].baud == B9600) {
    gtk_combo_box_set_active(GTK_COMBO_BOX(widget), 1);
    return;
  }

  //
  // Do nothing if the baud rate is already effective.
  // If a serial client is already running and the baud rate is changed, we close and re-open it
  //
  switch (bd) {
  case 0:
  default:
    new = B4800;
    break;

  case 1:
    new = B9600;
    break;

  case 2:
    new = B19200;
    break;

  case 3:
    new = B38400;
    break;
  }

  if (new == SerialPorts[id].baud) {
    return;
  }

  SerialPorts[id].baud = new;

  if (SerialPorts[id].enable) {
    t_print("%s: closing/re-opening serial port %s\n", __FUNCTION__, SerialPorts[id].port);
    disable_serial(id);

    if (launch_serial(id) == 0) {
      gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(serial_enable_b[id]), FALSE);
      SerialPorts[id].enable = 0;
    }
  }

  t_print("%s: Baud rate changed: Port=%s Baud=%d\n", __FUNCTION__, SerialPorts[id].port, SerialPorts[id].baud);
}

void rigctl_menu(GtkWidget *parent) {
  dialog = gtk_dialog_new();
  gtk_window_set_transient_for(GTK_WINDOW(dialog), GTK_WINDOW(parent));
  GtkWidget *headerbar = gtk_header_bar_new();
  gtk_window_set_titlebar(GTK_WINDOW(dialog), headerbar);
  gtk_header_bar_set_show_close_button(GTK_HEADER_BAR(headerbar), TRUE);
  gtk_header_bar_set_title(GTK_HEADER_BAR(headerbar), "piHPSDR - RigCtl");
  g_signal_connect (dialog, "delete_event", G_CALLBACK (close_cb), NULL);
  g_signal_connect (dialog, "destroy", G_CALLBACK (close_cb), NULL);
  GtkWidget *content = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
  GtkWidget *grid = gtk_grid_new();
  gtk_grid_set_column_spacing (GTK_GRID(grid), 10);
  GtkWidget *close_b = gtk_button_new_with_label("Close");
  gtk_widget_set_name(close_b, "close_button");
  g_signal_connect (close_b, "button-press-event", G_CALLBACK(close_cb), NULL);
  gtk_grid_attach(GTK_GRID(grid), close_b, 0, 0, 1, 1);
  GtkWidget *rigctl_port_label = gtk_label_new("RigCtl TCP Port");
  gtk_widget_set_name(rigctl_port_label, "boldlabel");
  gtk_widget_set_halign(rigctl_port_label, GTK_ALIGN_END);
  gtk_widget_show(rigctl_port_label);
  gtk_grid_attach(GTK_GRID(grid), rigctl_port_label, 0, 1, 1, 1);
  GtkWidget *rigctl_port_spinner = gtk_spin_button_new_with_range(18000, 21000, 1);
  gtk_spin_button_set_value(GTK_SPIN_BUTTON(rigctl_port_spinner), (double)19090);
  gtk_widget_show(rigctl_port_spinner);
  gtk_grid_attach(GTK_GRID(grid), rigctl_port_spinner, 1, 1, 1, 1);
  g_signal_connect(rigctl_port_spinner, "value_changed", G_CALLBACK(rigctl_value_changed_cb), NULL);
  GtkWidget *rigctl_enable_b = gtk_check_button_new_with_label("Rigctl Enable");
  gtk_widget_set_name(rigctl_enable_b, "boldlabel");
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (rigctl_enable_b), rigctl_enable);
  gtk_widget_show(rigctl_enable_b);
  gtk_grid_attach(GTK_GRID(grid), rigctl_enable_b, 2, 1, 1, 1);
  g_signal_connect(rigctl_enable_b, "toggled", G_CALLBACK(rigctl_enable_cb), NULL);
  GtkWidget *rigctl_debug_b = gtk_check_button_new_with_label("Debug");
  gtk_widget_set_name(rigctl_debug_b, "boldlabel");
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (rigctl_debug_b), rigctl_debug);
  gtk_widget_show(rigctl_debug_b);
  gtk_grid_attach(GTK_GRID(grid), rigctl_debug_b, 3, 1, 1, 1);
  g_signal_connect(rigctl_debug_b, "toggled", G_CALLBACK(rigctl_debug_cb), NULL);

  /* Put the Serial Port stuff here, one port per line */

  for (int i = 0; i < MAX_SERIAL; i++) {
    char str[64];
    int row = i + 2;
    snprintf (str, 64, "Serial Port%d:", i);
    GtkWidget *serial_text_label = gtk_label_new(str);
    gtk_widget_set_name(serial_text_label, "boldlabel");
    gtk_widget_set_halign(serial_text_label, GTK_ALIGN_END);
    gtk_grid_attach(GTK_GRID(grid), serial_text_label, 0, row, 1, 1);
    serial_port_entry = gtk_entry_new();
    gtk_entry_set_text(GTK_ENTRY(serial_port_entry), SerialPorts[i].port);
    gtk_widget_show(serial_port_entry);
    gtk_grid_attach(GTK_GRID(grid), serial_port_entry, 1, row, 2, 1);
    g_signal_connect(serial_port_entry, "changed", G_CALLBACK(serial_port_cb), GINT_TO_POINTER(i));
    baud_combo[i] = gtk_combo_box_text_new();
    gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(baud_combo[i]), NULL, "4800 Bd");
    gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(baud_combo[i]), NULL, "9600 Bd");
    gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(baud_combo[i]), NULL, "19200 Bd");
    gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(baud_combo[i]), NULL, "38400 Bd");

    switch (SerialPorts[i].baud) {
    case B9600:
      gtk_combo_box_set_active(GTK_COMBO_BOX(baud_combo[i]), 1);
      break;

    case B19200:
      gtk_combo_box_set_active(GTK_COMBO_BOX(baud_combo[i]), 2);
      break;

    case B38400:
      gtk_combo_box_set_active(GTK_COMBO_BOX(baud_combo[i]), 3);
      break;

    default:
      SerialPorts[i].baud = B4800;
      gtk_combo_box_set_active(GTK_COMBO_BOX(baud_combo[i]), 0);
    }

    my_combo_attach(GTK_GRID(grid), baud_combo[i], 3, row, 1, 1);
    g_signal_connect(baud_combo[i], "changed", G_CALLBACK(baud_cb), GINT_TO_POINTER(i));
    serial_enable_b[i] = gtk_check_button_new_with_label("Enable");
    gtk_widget_set_name(serial_enable_b[i], "boldlabel");
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (serial_enable_b[i]), SerialPorts[i].enable);
    gtk_grid_attach(GTK_GRID(grid), serial_enable_b[i], 4, row, 1, 1);
    g_signal_connect(serial_enable_b[i], "toggled", G_CALLBACK(serial_enable_cb), GINT_TO_POINTER(i));
    andromeda_b[i] = gtk_check_button_new_with_label("Andromeda");
    gtk_widget_set_name(andromeda_b[i], "boldlabel");
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (andromeda_b[i]), SerialPorts[i].andromeda);
    gtk_grid_attach(GTK_GRID(grid), andromeda_b[i], 5, row, 1, 1);
    g_signal_connect(andromeda_b[i], "toggled", G_CALLBACK(andromeda_cb), GINT_TO_POINTER(i));
  }

  gtk_container_add(GTK_CONTAINER(content), grid);
  sub_menu = dialog;
  gtk_widget_show_all(dialog);
}

