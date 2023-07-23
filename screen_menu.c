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

static GtkWidget *dialog=NULL;

//
// Make local copies of METER_WIDTH and rx_stack_horizontal,
// and copy those back when the menu is closed or the apply()
// function is called. These values affect the update of the
// meter and the rx panadapter which are called asynchronously.
//
static int my_meter_width;
static int my_rx_hstack;

static void apply() {
  METER_WIDTH=my_meter_width;
  rx_stack_horizontal=my_rx_hstack;
  reconfigure_screen();
}

static void cleanup() {
  apply();
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

static gboolean apply_cb(GtkWidget *widget, gpointer data) {
  apply();
  return TRUE;
}

static void vfo_cb(GtkWidget *widget, gpointer data) {
  VFO_HEIGHT=gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(widget));
}

static void meter_cb(GtkWidget *widget, gpointer data) {
  my_meter_width=gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(widget));
}

static void width_cb(GtkWidget *widget, gpointer data) {
  display_width=gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(widget));
  gtk_widget_set_size_request(top_window, display_width, display_height);
}   

static void height_cb(GtkWidget *widget, gpointer data) {
  display_height=gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(widget));
  gtk_widget_set_size_request(top_window, display_width, display_height);
}   

static void horizontal_cb(GtkWidget *widget, gpointer data) {
  my_rx_hstack=gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));
}

void screen_menu(GtkWidget *parent) {

  GtkWidget *label;
  GtkWidget *button;

  my_rx_hstack=rx_stack_horizontal;
  my_meter_width=METER_WIDTH;

  dialog=gtk_dialog_new();
  gtk_window_set_transient_for(GTK_WINDOW(dialog),GTK_WINDOW(parent));
  gtk_window_set_title(GTK_WINDOW(dialog),"piHPSDR - Screen Layout");
  g_signal_connect (dialog, "delete_event", G_CALLBACK (delete_event), NULL);
  set_backgnd(dialog);

  GtkWidget *content=gtk_dialog_get_content_area(GTK_DIALOG(dialog));

  GtkWidget *grid=gtk_grid_new();

  gtk_grid_set_column_homogeneous(GTK_GRID(grid),TRUE);
  gtk_grid_set_row_homogeneous(GTK_GRID(grid),TRUE);
  gtk_grid_set_column_spacing (GTK_GRID(grid),5);
  gtk_grid_set_row_spacing (GTK_GRID(grid),5);

  int row=0;
  int col=0;

  GtkWidget *close_b=gtk_button_new_with_label("Close");
  g_signal_connect (close_b, "pressed", G_CALLBACK(close_cb), NULL);
  gtk_grid_attach(GTK_GRID(grid),close_b,col,row,1,1);
  row++;

  if (!full_screen) {
    label=gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(label), "<b>Window Width: </b>");
    gtk_widget_set_halign(label, GTK_ALIGN_START);
    gtk_grid_attach(GTK_GRID(grid),label,col,row,1,1);
    col++;

    button=gtk_spin_button_new_with_range(800.0, (double) screen_width, 32.0);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(button), (double) display_width);
    gtk_grid_attach(GTK_GRID(grid), button, col, row, 1, 1);
    g_signal_connect(button,"value-changed", G_CALLBACK(width_cb), NULL);
    col++;

    label=gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(label), "<b>Window Height: </b>");
    gtk_widget_set_halign(label, GTK_ALIGN_START);
    gtk_grid_attach(GTK_GRID(grid),label,col,row,1,1);
    col++;

    button=gtk_spin_button_new_with_range(480.0, (double) screen_height, 16.0);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(button), (double) display_height);
    gtk_grid_attach(GTK_GRID(grid), button, col, row, 1, 1);
    g_signal_connect(button,"value-changed", G_CALLBACK(height_cb), NULL);
    row++;
  }

  col=0;
  label=gtk_label_new(NULL);
  gtk_label_set_markup(GTK_LABEL(label), "<b>VFO Height: </b>");
  gtk_widget_set_halign(label, GTK_ALIGN_START);
  gtk_grid_attach(GTK_GRID(grid),label,col,row,1,1);
  col++;

  button=gtk_spin_button_new_with_range(60.0, 96.0, 12.0);
  gtk_spin_button_set_value(GTK_SPIN_BUTTON(button), (double) VFO_HEIGHT);
  gtk_grid_attach(GTK_GRID(grid), button, col, row, 1, 1);
  g_signal_connect(button,"value-changed", G_CALLBACK(vfo_cb), NULL);
  col++;

  label=gtk_label_new(NULL);
  gtk_label_set_markup(GTK_LABEL(label), "<b>Meter Width: </b>");
  gtk_widget_set_halign(label, GTK_ALIGN_START);
  gtk_grid_attach(GTK_GRID(grid),label,col,row,1,1);
  col++;

  button=gtk_spin_button_new_with_range(100.0, 250.0, 25.0);
  gtk_spin_button_set_value(GTK_SPIN_BUTTON(button), (double) my_meter_width);
  gtk_grid_attach(GTK_GRID(grid), button, col, row, 1, 1);
  g_signal_connect(button,"value-changed", G_CALLBACK(meter_cb), NULL);
  row++;
 
  col=0;
  button=gtk_check_button_new_with_label("Stack receivers horizontally");
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button), my_rx_hstack);
  gtk_grid_attach(GTK_GRID(grid),button,col,row,2,1);
  g_signal_connect(button,"toggled",G_CALLBACK(horizontal_cb),NULL);
  row++;


  button=gtk_button_new_with_label("Apply");
  g_signal_connect(button, "pressed", G_CALLBACK(apply_cb), NULL);
  gtk_grid_attach(GTK_GRID(grid), button, 5, row, 1, 1);


  gtk_container_add(GTK_CONTAINER(content),grid);

  sub_menu=dialog;

  gtk_widget_show_all(dialog);

}
