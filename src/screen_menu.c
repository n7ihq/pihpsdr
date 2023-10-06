/* Copyright (C)
* 2023 - Christoph van Wüllen, DL1YCF
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

#include "radio.h"
#include "new_menu.h"
#include "main.h"
#include "appearance.h"
#include "message.h"


static GtkWidget *dialog = NULL;
static GtkWidget *wide_b = NULL;
static GtkWidget *height_b = NULL;
static GtkWidget *full_b = NULL;
static GtkWidget *vfo_b = NULL;
static gulong vfo_signal_id;

static void apply() {
  reconfigure_screen();
  //
  // VFO layout may have been re-adjusted so update combo-box
  // (without letting it emit a signal)
  //
  g_signal_handler_block(G_OBJECT(vfo_b), vfo_signal_id);
  gtk_combo_box_set_active(GTK_COMBO_BOX(vfo_b), vfo_layout);
  g_signal_handler_unblock(G_OBJECT(vfo_b), vfo_signal_id);
}

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

static void vfo_cb(GtkWidget *widget, gpointer data) {
  vfo_layout = gtk_combo_box_get_active (GTK_COMBO_BOX(widget));
  VFO_HEIGHT = vfo_layout_list[vfo_layout].height;
  int needed = vfo_layout_list[vfo_layout].width + METER_WIDTH + MENU_WIDTH;

  if (needed % 32 != 0) { needed = 32 * (needed / 32 + 1); }

  if (needed > display_width && wide_b && needed < screen_width) {
    display_width = needed;
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(wide_b), (double) needed);
  }

  apply();
}

static void width_cb(GtkWidget *widget, gpointer data) {
  //
  // It has been reported (thanks Bill) that changing the width
  // with several (fast) clicks leads to a crash of the program.
  // This could be verified but without getting a clue why it
  // happens. Therefore, we make the spin button "deaf" while
  // changing the width.
  //
  gtk_widget_set_sensitive(widget, FALSE);
  display_width = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(widget));

  if (!full_screen) { apply(); }
  gtk_widget_set_sensitive(widget, TRUE);
}

static void height_cb(GtkWidget *widget, gpointer data) {
  //
  // see width_cb. I have never found that I can crash piHPSDR by
  // fast repititive changes of the height, but I shall do the same
  // here as for the width callback
  //
  gtk_widget_set_sensitive(widget, FALSE);
  display_height = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(widget));

  if (!full_screen) { apply(); }
  gtk_widget_set_sensitive(widget, TRUE);
}

static void horizontal_cb(GtkWidget *widget, gpointer data) {
  rx_stack_horizontal = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));
  apply();
}

static void full_cb(GtkWidget *widget, gpointer data) {
  full_screen = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));
  apply();
}

void screen_menu(GtkWidget *parent) {
  GtkWidget *label;
  GtkWidget *button;
  dialog = gtk_dialog_new();
  gtk_window_set_transient_for(GTK_WINDOW(dialog), GTK_WINDOW(parent));
  gtk_window_set_title(GTK_WINDOW(dialog), "piHPSDR - Screen Layout");
  g_signal_connect (dialog, "delete_event", G_CALLBACK (close_cb), NULL);
  g_signal_connect (dialog, "destroy", G_CALLBACK (close_cb), NULL);
  GtkWidget *content = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
  GtkWidget *grid = gtk_grid_new();
  gtk_grid_set_column_homogeneous(GTK_GRID(grid), TRUE);
  gtk_grid_set_row_homogeneous(GTK_GRID(grid), TRUE);
  gtk_grid_set_column_spacing (GTK_GRID(grid), 5);
  gtk_grid_set_row_spacing (GTK_GRID(grid), 5);
  int row = 0;
  int col = 0;
  GtkWidget *close_b = gtk_button_new_with_label("Close");
  gtk_widget_set_name(close_b, "close_button");
  g_signal_connect (close_b, "button-press-event", G_CALLBACK(close_cb), NULL);
  gtk_grid_attach(GTK_GRID(grid), close_b, col, row, 1, 1);
  row++;
  label = gtk_label_new("Window Width:");
  gtk_widget_set_name(label, "boldlabel");
  gtk_widget_set_halign(label, GTK_ALIGN_END);
  gtk_grid_attach(GTK_GRID(grid), label, col, row, 1, 1);
  col++;
  wide_b = gtk_spin_button_new_with_range(640.0, (double) screen_width, 32.0);
  gtk_spin_button_set_value(GTK_SPIN_BUTTON(wide_b), (double) display_width);
  gtk_grid_attach(GTK_GRID(grid), wide_b, col, row, 1, 1);
  g_signal_connect(wide_b, "value-changed", G_CALLBACK(width_cb), NULL);
  col++;
  label = gtk_label_new("Window Height:");
  gtk_widget_set_halign(label, GTK_ALIGN_END);
  gtk_widget_set_name(label, "boldlabel");
  gtk_grid_attach(GTK_GRID(grid), label, col, row, 1, 1);
  col++;
  height_b = gtk_spin_button_new_with_range(400.0, (double) screen_height, 16.0);
  gtk_spin_button_set_value(GTK_SPIN_BUTTON(height_b), (double) display_height);
  gtk_grid_attach(GTK_GRID(grid), height_b, col, row, 1, 1);
  g_signal_connect(height_b, "value-changed", G_CALLBACK(height_cb), NULL);
  row++;
  col = 0;
  label = gtk_label_new("Select VFO bar layout:");
  gtk_widget_set_name(label, "boldlabel");
  gtk_widget_set_halign(label, GTK_ALIGN_END);
  gtk_grid_attach(GTK_GRID(grid), label, col, row, 1, 1);
  col++;
  vfo_b = gtk_combo_box_text_new();
  const VFO_BAR_LAYOUT *vfl = vfo_layout_list;

  for (;;) {
    if (vfl->width < 0) { break; }

    gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(vfo_b), NULL, vfl->description);

    if (vfl - vfo_layout_list == vfo_layout) { gtk_combo_box_set_active(GTK_COMBO_BOX(vfo_b), vfo_layout); }

    vfl++;
  }

  // This combo-box spans three columns so the text may be really long
  gtk_grid_attach(GTK_GRID(grid), vfo_b, col, row, 3, 1);
  vfo_signal_id=g_signal_connect(vfo_b, "changed", G_CALLBACK(vfo_cb), NULL);
  row++;
  button = gtk_check_button_new_with_label("Stack receivers horizontally");
  gtk_widget_set_name(button, "boldlabel");
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button), rx_stack_horizontal);
  gtk_grid_attach(GTK_GRID(grid), button, 0, row, 2, 1);
  g_signal_connect(button, "toggled", G_CALLBACK(horizontal_cb), NULL);
  full_b = gtk_check_button_new_with_label("Full Screen Mode");
  gtk_widget_set_name(full_b, "boldlabel");
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(full_b), full_screen);
  gtk_grid_attach(GTK_GRID(grid), full_b, 2, row, 2, 1);
  g_signal_connect(full_b, "toggled", G_CALLBACK(full_cb), NULL);
  gtk_container_add(GTK_CONTAINER(content), grid);
  sub_menu = dialog;
  gtk_widget_show_all(dialog);
}
