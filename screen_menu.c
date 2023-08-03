/*
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
#include <stdio.h>

#include "radio.h"
#include "new_menu.h"
#include "main.h"
#include "appearance.h"
#include "message.h"


static GtkWidget *dialog = NULL;
static GtkWidget *wide_b = NULL;
static GtkWidget *vfo_b = NULL;

static void apply() {
  reconfigure_screen();
  //
  // VFO layout may have been re-adjusted so update combo-box
  //
  gtk_combo_box_set_active(GTK_COMBO_BOX(vfo_b), vfo_layout);
}

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

static void meter_cb(GtkWidget *widget, gpointer data) {
  METER_WIDTH = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(widget));
  apply();
}

static void width_cb(GtkWidget *widget, gpointer data) {
  display_width = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(widget));
  gtk_widget_set_size_request(top_window, display_width, display_height);
  apply();
}

static void height_cb(GtkWidget *widget, gpointer data) {
  display_height = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(widget));
  gtk_widget_set_size_request(top_window, display_width, display_height);
  apply();
}

static void horizontal_cb(GtkWidget *widget, gpointer data) {
  rx_stack_horizontal = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));
  apply();
}

void screen_menu(GtkWidget *parent) {
  GtkWidget *label;
  GtkWidget *button;
  dialog = gtk_dialog_new();
  gtk_window_set_transient_for(GTK_WINDOW(dialog), GTK_WINDOW(parent));
  gtk_window_set_title(GTK_WINDOW(dialog), "piHPSDR - Screen Layout");
  g_signal_connect (dialog, "delete_event", G_CALLBACK (delete_event), NULL);
  set_backgnd(dialog);
  GtkWidget *content = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
  GtkWidget *grid = gtk_grid_new();
  gtk_grid_set_column_homogeneous(GTK_GRID(grid), TRUE);
  gtk_grid_set_row_homogeneous(GTK_GRID(grid), TRUE);
  gtk_grid_set_column_spacing (GTK_GRID(grid), 5);
  gtk_grid_set_row_spacing (GTK_GRID(grid), 5);
  int row = 0;
  int col = 0;
  GtkWidget *close_b = gtk_button_new_with_label("Close");
  g_signal_connect (close_b, "button-press-event", G_CALLBACK(close_cb), NULL);
  gtk_grid_attach(GTK_GRID(grid), close_b, col, row, 1, 1);
  row++;

  if (!full_screen) {
    label = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(label), "<b>Window Width: </b>");
    gtk_widget_set_halign(label, GTK_ALIGN_START);
    gtk_grid_attach(GTK_GRID(grid), label, col, row, 1, 1);
    col++;
    wide_b = gtk_spin_button_new_with_range(800.0, (double) screen_width, 32.0);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(wide_b), (double) display_width);
    gtk_grid_attach(GTK_GRID(grid), wide_b, col, row, 1, 1);
    g_signal_connect(wide_b, "value-changed", G_CALLBACK(width_cb), NULL);
    col++;
    label = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(label), "<b>Window Height: </b>");
    gtk_widget_set_halign(label, GTK_ALIGN_START);
    gtk_grid_attach(GTK_GRID(grid), label, col, row, 1, 1);
    col++;
    button = gtk_spin_button_new_with_range(480.0, (double) screen_height, 16.0);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(button), (double) display_height);
    gtk_grid_attach(GTK_GRID(grid), button, col, row, 1, 1);
    g_signal_connect(button, "value-changed", G_CALLBACK(height_cb), NULL);
    row++;
  }

  col = 0;
  label = gtk_label_new(NULL);
  gtk_label_set_markup(GTK_LABEL(label), "<b>Meter Width: </b>");
  gtk_widget_set_halign(label, GTK_ALIGN_START);
  gtk_grid_attach(GTK_GRID(grid), label, col, row, 1, 1);
  col++;
  button = gtk_spin_button_new_with_range(100.0, 250.0, 25.0);
  gtk_spin_button_set_value(GTK_SPIN_BUTTON(button), (double) METER_WIDTH);
  gtk_grid_attach(GTK_GRID(grid), button, col, row, 1, 1);
  g_signal_connect(button, "value-changed", G_CALLBACK(meter_cb), NULL);
  row++;
  button = gtk_check_button_new_with_label("Stack receivers horizontally");
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button), rx_stack_horizontal);
  gtk_grid_attach(GTK_GRID(grid), button, col, row, 2, 1);
  g_signal_connect(button, "toggled", G_CALLBACK(horizontal_cb), NULL);
  row++;
  col = 0;
  label = gtk_label_new(NULL);
  gtk_label_set_markup(GTK_LABEL(label), "<b>Select VFO bar layout: </b>");
  gtk_widget_set_halign(label, GTK_ALIGN_START);
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
  g_signal_connect(vfo_b, "changed", G_CALLBACK(vfo_cb), NULL);
  gtk_container_add(GTK_CONTAINER(content), grid);
  sub_menu = dialog;
  gtk_widget_show_all(dialog);
}
