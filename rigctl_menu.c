/* Copyright (C)
* 2015 - John Melton, G0ORX/N6LYT
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
#include <string.h>
#include <termios.h>
#include <unistd.h>

#include "new_menu.h"
#include "rigctl_menu.h"
#include "rigctl.h"
#include "band.h"
#include "radio.h"
#include "vfo.h"

static GtkWidget *serial_enable_b[MAX_SERIAL];

static GtkWidget *menu_b=NULL;
static GtkWidget *dialog=NULL;
static GtkWidget *serial_port_entry;

static void cleanup(void) {
  if(dialog!=NULL) {
    gtk_widget_destroy(dialog);
    dialog=NULL;
    sub_menu=NULL;
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

static void rigctl_value_changed_cb(GtkWidget *widget, gpointer data) {
   rigctl_port_base = gtk_spin_button_get_value(GTK_SPIN_BUTTON(widget));
}

static void rigctl_debug_cb(GtkWidget *widget, gpointer data) {
  rigctl_debug=gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));
  g_print("---------- RIGCTL DEBUG %s ----------\n",rigctl_debug?"ON":"OFF");
}

static void rigctl_enable_cb(GtkWidget *widget, gpointer data) {
  rigctl_enable=gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));
  if(rigctl_enable) {
     launch_rigctl();
  } else {
     //
     // If serial server is running, terminate it.
     // Disable serial and clear check-box
     //
     for (int id=0; id<MAX_SERIAL; id++) {
       if (SerialPorts[id].enable) {
         disable_serial(id);
       }
       SerialPorts[id].enable=0;
       gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON(serial_enable_b[id]),0);
     }
     disable_rigctl();
  }
}

static void serial_port_cb(GtkWidget *widget, gpointer data) {
  int id=GPOINTER_TO_INT(data);
  const char *cp=gtk_entry_get_text(GTK_ENTRY(widget));
  g_print("%s: ID=%d port=%s\n", __FUNCTION__, id, cp);
  strcpy(SerialPorts[id].port, cp);
}

#ifdef ANDROMEDA
static void andromeda_cb(GtkWidget *widget, gpointer data) {
  int id=GPOINTER_TO_INT(data);
  SerialPorts[id].andromeda=gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));
  if (SerialPorts[id].andromeda) {
   if(SerialPorts[id].enable) {
     launch_andromeda(id);
   }
  } else {
    disable_andromeda(id);
  }
}
#endif

static void serial_enable_cb(GtkWidget *widget, gpointer data) {
  //
  // If rigctl is not running, serial cannot be enabled
  //
  int id=GPOINTER_TO_INT(data);
  if (!rigctl_enable) {
    SerialPorts[id].enable=0;
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget),0);
    return;
  }
  if((SerialPorts[id].enable=gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget)))) {
     if(launch_serial(id) == 0) {
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget),FALSE);
     }
  } else {
     disable_serial(id);
  }
  g_print("RIGCTL_MENU: Serial enable : ID=%d Enabled=%d\n",id,SerialPorts[id].enable);
}

// Set Baud Rate
static void baud_cb(GtkWidget *widget, gpointer data) {
   int id=GPOINTER_TO_INT(data);
   int bd=gtk_combo_box_get_active(GTK_COMBO_BOX(widget));
   switch (bd) {
     case 0:
       SerialPorts[id].baud=B4800;
       break;
     case 1:
       SerialPorts[id].baud=B9600;
       break;
     case 2:
       SerialPorts[id].baud=B19200;
       break;
     case 3:
       SerialPorts[id].baud=B38400;
       break;
   }
   g_print("RIGCTL_MENU: Baud rate changed: ID=%d Baud=%d\n",id,SerialPorts[id].baud);
}

void rigctl_menu(GtkWidget *parent) {

  dialog=gtk_dialog_new();
  gtk_window_set_transient_for(GTK_WINDOW(dialog),GTK_WINDOW(parent));
  gtk_window_set_title(GTK_WINDOW(dialog),"piHPSDR - RIGCTL");
  g_signal_connect (dialog, "delete_event", G_CALLBACK (delete_event), NULL);
  set_backgnd(dialog);

  GtkWidget *content=gtk_dialog_get_content_area(GTK_DIALOG(dialog));

  GtkWidget *grid=gtk_grid_new();
  gtk_grid_set_column_spacing (GTK_GRID(grid),10);

  GtkWidget *close_b=gtk_button_new_with_label("Close");
  g_signal_connect (close_b, "pressed", G_CALLBACK(close_cb), NULL);
  gtk_grid_attach(GTK_GRID(grid),close_b,0,0,1,1);

  GtkWidget *rigctl_debug_b=gtk_check_button_new_with_label("Debug");
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (rigctl_debug_b), rigctl_debug);
  gtk_widget_show(rigctl_debug_b);
  gtk_grid_attach(GTK_GRID(grid),rigctl_debug_b,3,0,1,1);
  g_signal_connect(rigctl_debug_b,"toggled",G_CALLBACK(rigctl_debug_cb),NULL);

  GtkWidget *rigctl_enable_b=gtk_check_button_new_with_label("Rigctl Enable");
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (rigctl_enable_b), rigctl_enable);
  gtk_widget_show(rigctl_enable_b);
  gtk_grid_attach(GTK_GRID(grid),rigctl_enable_b,0,1,1,1);
  g_signal_connect(rigctl_enable_b,"toggled",G_CALLBACK(rigctl_enable_cb),NULL);

  GtkWidget *rigctl_port_label =gtk_label_new(NULL);
  gtk_label_set_markup(GTK_LABEL(rigctl_port_label), "<b>RigCtl TCP Port</b>");
  gtk_widget_show(rigctl_port_label);
  gtk_grid_attach(GTK_GRID(grid),rigctl_port_label,1,1,1,1);

  GtkWidget *rigctl_port_spinner =gtk_spin_button_new_with_range(18000,21000,1);
  gtk_spin_button_set_value(GTK_SPIN_BUTTON(rigctl_port_spinner),(double)19090);
  gtk_widget_show(rigctl_port_spinner);
  gtk_grid_attach(GTK_GRID(grid),rigctl_port_spinner,2,1,2,1);
  g_signal_connect(rigctl_port_spinner,"value_changed",G_CALLBACK(rigctl_value_changed_cb),NULL);

  /* Put the Serial Port stuff here, one port per line */


  for (int i=0; i<MAX_SERIAL; i++) {
    char str[64];
    int row=i+2;
    sprintf (str,"<b>Serial Port%d: </b>", i);
    GtkWidget *serial_text_label=gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(serial_text_label), str);
    gtk_grid_attach(GTK_GRID(grid),serial_text_label,0,row,1,1);

    serial_port_entry=gtk_entry_new();
    gtk_entry_set_text(GTK_ENTRY(serial_port_entry),SerialPorts[i].port);
    gtk_widget_show(serial_port_entry);
    gtk_grid_attach(GTK_GRID(grid),serial_port_entry,1,row,2,1);
    g_signal_connect(serial_port_entry, "changed", G_CALLBACK(serial_port_cb), GINT_TO_POINTER(i));

    GtkWidget *baud_combo=gtk_combo_box_text_new();
    gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(baud_combo), NULL,"4800 Bd");
    gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(baud_combo), NULL,"9600 Bd");
    gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(baud_combo), NULL,"19200 Bd");
    gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(baud_combo), NULL,"38400 Bd");
    switch (SerialPorts[i].baud) {
      case B9600:
        gtk_combo_box_set_active(GTK_COMBO_BOX(baud_combo), 1);
        break;
      case B19200:
        gtk_combo_box_set_active(GTK_COMBO_BOX(baud_combo), 2);
        break;
      case B38400:
        gtk_combo_box_set_active(GTK_COMBO_BOX(baud_combo), 3);
        break;
      default:
        SerialPorts[i].baud=B4800;
        gtk_combo_box_set_active(GTK_COMBO_BOX(baud_combo), 0);
    }
    my_combo_attach(GTK_GRID(grid), baud_combo, 3, row, 1, 1);
    g_signal_connect(baud_combo,"changed",G_CALLBACK(baud_cb), GINT_TO_POINTER(i));

    serial_enable_b[i]=gtk_check_button_new_with_label("Enable");
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (serial_enable_b[i]), SerialPorts[i].enable);
    gtk_grid_attach(GTK_GRID(grid),serial_enable_b[i],4,row,1,1);
    g_signal_connect(serial_enable_b[i],"toggled",G_CALLBACK(serial_enable_cb),GINT_TO_POINTER(i));

#ifdef ANDROMEDA
    GtkWidget *andromeda_b=gtk_check_button_new_with_label("Andromeda");
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (andromeda_b), SerialPorts[i].andromeda);
    gtk_grid_attach(GTK_GRID(grid),andromeda_b,5,row,1,1);
    g_signal_connect(andromeda_b,"toggled",G_CALLBACK(andromeda_cb),GINT_TO_POINTER(i));
#endif
  }

  gtk_container_add(GTK_CONTAINER(content),grid);

  sub_menu=dialog;

  gtk_widget_show_all(dialog);

}

