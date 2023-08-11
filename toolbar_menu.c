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
#include <stdio.h>
#include <string.h>

#include "radio.h"
#include "new_menu.h"
#include "actions.h"
#include "action_dialog.h"
#include "gpio.h"
#include "toolbar.h"

static GtkWidget *dialog = NULL;

static void cleanup() {
  if (dialog != NULL) {
    gtk_widget_destroy(dialog);
    dialog = NULL;
    sub_menu = NULL;
    active_menu = NO_MENU;
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

static gboolean switch_cb(GtkWidget *widget, GdkEvent *event, gpointer data) {
  SWITCH *sw = (SWITCH *) data;
  int action = action_dialog(dialog, CONTROLLER_SWITCH, sw->switch_function);
  gtk_button_set_label(GTK_BUTTON(widget), ActionTable[action].button_str);
  sw->switch_function = action;
  update_toolbar_labels();
  return TRUE;
}

void toolbar_menu(GtkWidget *parent) {
  dialog = gtk_dialog_new();
  gtk_window_set_transient_for(GTK_WINDOW(dialog), GTK_WINDOW(parent));
  gtk_window_set_title(GTK_WINDOW(dialog), "piHPSDR - Toolbar configuration");
  g_signal_connect (dialog, "delete_event", G_CALLBACK (delete_event), NULL);
  set_backgnd(dialog);
  GtkWidget *content = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
  GtkWidget *grid = gtk_grid_new();
  gtk_grid_set_column_homogeneous(GTK_GRID(grid), TRUE);
  gtk_grid_set_row_homogeneous(GTK_GRID(grid), FALSE);
  gtk_grid_set_column_spacing (GTK_GRID(grid), 5);
  gtk_grid_set_row_spacing (GTK_GRID(grid), 5);
  GtkWidget *close_b = gtk_button_new_with_label("Close");
  g_signal_connect (close_b, "button-press-event", G_CALLBACK(close_cb), NULL);
  gtk_grid_attach(GTK_GRID(grid), close_b, 0, 0, 1, 1);

  gchar text[64];
  GtkWidget *widget;

  gint lfunction = 0;
  const int max_switches = 8;

  for (lfunction = 0; lfunction < MAX_FUNCTIONS; lfunction++) {
    widget=gtk_label_new(NULL);
    sprintf(text,"<b>F%d</b>", lfunction);
    gtk_label_set_markup(GTK_LABEL(widget),text);
    gtk_grid_attach(GTK_GRID(grid), widget, 0, lfunction+1, 1, 1);
    SWITCH *switches = switches_controller1[lfunction];

    for (int i = 0; i < max_switches; i++) {
      if (i == max_switches - 1) {
        // Rightmost switch is hardwired to FUNCTION
        switches[i].switch_function = FUNCTION;
        widget = gtk_button_new_with_label(ActionTable[switches[i].switch_function].button_str);
        gtk_grid_attach(GTK_GRID(grid), widget, i+1, lfunction+1, 1, 1);
      } else {
        widget = gtk_button_new_with_label(ActionTable[switches[i].switch_function].button_str);
        gtk_grid_attach(GTK_GRID(grid), widget, i+1, lfunction+1, 1, 1);
        g_signal_connect(widget, "button-press-event", G_CALLBACK(switch_cb), (gpointer) &switches[i]);
      }
    }
  }

  gtk_container_add(GTK_CONTAINER(content), grid);
  sub_menu = dialog;
  gtk_widget_show_all(dialog);
}
