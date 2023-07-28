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

static GtkWidget *dialog=NULL;

static void cleanup() {
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

static void filter_type_cb(GtkToggleButton *widget, gpointer data) {
  fft_type = gtk_combo_box_get_active (GTK_COMBO_BOX(widget));
  for(int i=0;i<RECEIVERS;i++) {
    RXASetMP(receiver[i]->id, fft_type);
  }
  TXASetMP(transmitter->id, fft_type);
  t_print("WDSP filter type changed to %d\n", fft_type);
}

static void filter_size_cb(GtkWidget *widget, gpointer data) {
  int val = gtk_combo_box_get_active (GTK_COMBO_BOX(widget));
  fft_size = 1 << (val+11);
  for(int i=0;i<RECEIVERS;i++) {
     RXASetNC(receiver[i]->id, fft_size);
   } 
   TXASetNC(transmitter->id, fft_size);
   t_print("WDSP filter size changed to %d\n", fft_size);
}

void fft_menu(GtkWidget *parent) {

  dialog=gtk_dialog_new();
  gtk_window_set_transient_for(GTK_WINDOW(dialog),GTK_WINDOW(parent));
  gtk_window_set_title(GTK_WINDOW(dialog),"piHPSDR - FFT");
  g_signal_connect (dialog, "delete_event", G_CALLBACK (delete_event), NULL);
  set_backgnd(dialog);

  GtkWidget *content=gtk_dialog_get_content_area(GTK_DIALOG(dialog));

  GtkWidget *grid=gtk_grid_new();
  gtk_grid_set_column_spacing (GTK_GRID(grid),10);

  GtkWidget *close_b=gtk_button_new_with_label("Close");
  g_signal_connect (close_b, "button_press_event", G_CALLBACK(close_cb), NULL);
  gtk_grid_attach(GTK_GRID(grid),close_b,0,0,1,1);

  int row=1;
  int col=0;

  GtkWidget *filter_type_label=gtk_label_new(NULL);
  gtk_label_set_markup(GTK_LABEL(filter_type_label), "<b>Filter Type</b>");
  gtk_grid_attach(GTK_GRID(grid),filter_type_label,col,row,1,1);

  col++;
  GtkWidget *filter_type_combo=gtk_combo_box_text_new();
  gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(filter_type_combo), NULL, "Linear Phase");
  gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(filter_type_combo), NULL, "Low Latency");
  gtk_combo_box_set_active(GTK_COMBO_BOX(filter_type_combo), fft_type);
  my_combo_attach(GTK_GRID(grid), filter_type_combo, col, row, 1, 1);
  g_signal_connect(filter_type_combo,"changed",G_CALLBACK(filter_type_cb), NULL);

  row++;
  col=0;

  GtkWidget *filter_size_label=gtk_label_new("Filter Size: ");
  gtk_grid_attach(GTK_GRID(grid),filter_size_label,col,row,1,1);

  col++;
  //
  // We use a fixed "dsp_size" of 2048, so the filter size must be 2048, 4096, 8192, 16384
  //
  GtkWidget *filter_size_combo=gtk_combo_box_text_new();
  gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(filter_size_combo), NULL, "2048");
  gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(filter_size_combo), NULL, "4096");
  gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(filter_size_combo), NULL, "8192");
  gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(filter_size_combo), NULL, "16384");
  switch (fft_size) {
    case 2048:
      gtk_combo_box_set_active(GTK_COMBO_BOX(filter_size_combo), 0);
      break;
    case 4096:
      gtk_combo_box_set_active(GTK_COMBO_BOX(filter_size_combo), 1);
      break;
    case 8192:
      gtk_combo_box_set_active(GTK_COMBO_BOX(filter_size_combo), 2);
      break;
    case 16384:
      gtk_combo_box_set_active(GTK_COMBO_BOX(filter_size_combo), 3);
      break;
  }
  my_combo_attach(GTK_GRID(grid), filter_size_combo, col, row, 1, 1);
  g_signal_connect(filter_size_combo,"changed",G_CALLBACK(filter_size_cb), NULL);

  gtk_container_add(GTK_CONTAINER(content),grid);

  sub_menu=dialog;

  gtk_widget_show_all(dialog);

}

