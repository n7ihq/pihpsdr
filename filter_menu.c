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

static GtkWidget *dialog=NULL;

static GtkWidget *last_filter;
static GtkWidget *var1_spin_low;
static GtkWidget *var1_spin_high;
static GtkWidget *var2_spin_low;
static GtkWidget *var2_spin_high;


static void cleanup() {
  if(dialog!=NULL) {
    gtk_widget_destroy(dialog);
    dialog=NULL;
    sub_menu=NULL;
    active_menu=NO_MENU;
  }
}

static gboolean default_cb (GtkWidget *widget, gpointer data) {
  int mode=vfo[active_receiver->id].mode;
  int f=GPOINTER_TO_INT(data);
  int low, high;

  GtkWidget *spinlow, *spinhigh;

  switch(f) {
    case filterVar1:
      spinlow=var1_spin_low;
      spinhigh=var1_spin_high;
      low =var1_default_low[mode];
      high=var1_default_high[mode];
      break;
    case filterVar2:
      spinlow=var2_spin_low;
      spinhigh=var2_spin_high;
      low =var2_default_low[mode];
      high=var2_default_high[mode];
      break;
    default:
      g_print("%s: illegal data = %p (%d)\n",__FUNCTION__,data, f);
      return FALSE;
      break;
  }

  //
  // Determine value(s) for spin button(s)
  //
  if (mode == modeCWL || mode == modeCWU) {
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(spinlow),(double)(high-low));
  } else {
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(spinlow),(double)(low));
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(spinhigh),(double)(high));
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
  int f=GPOINTER_TO_UINT(data);
  vfo_filter_changed(f);
  return 0;
}

static gboolean filter_select_cb (GtkWidget *widget, gpointer        data) {
  filter_select(data);
  set_button_text_color(last_filter,"default");
  last_filter=widget;
  set_button_text_color(last_filter,"orange");
  return FALSE;
}

static gboolean deviation_select_cb (GtkWidget *widget, gpointer data) {
  active_receiver->deviation=GPOINTER_TO_UINT(data);
  transmitter->deviation=GPOINTER_TO_UINT(data);
  set_filter(active_receiver);
  tx_set_filter(transmitter);
  set_deviation(active_receiver);
  transmitter_set_deviation(transmitter);
  set_button_text_color(last_filter,"default");
  last_filter=widget;
  set_button_text_color(last_filter,"orange");
  g_idle_add(ext_vfo_update,NULL);
  return FALSE;
}

static void var_spin_low_cb (GtkWidget *widget, gpointer data) {
  int f=GPOINTER_TO_UINT(data);
  int id=active_receiver->id;

  FILTER *mode_filters=filters[vfo[id].mode];
  FILTER *filter=&mode_filters[f];

  int val=gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(widget));
  if(vfo[id].mode==modeCWL || vfo[id].mode==modeCWU) {
    filter->low = -val/2;
    filter->high=  val/2;
  } else {
    filter->low = val;
  }
  g_print("%s: new values=(%d:%d)\n", __FUNCTION__,filter->low,filter->high);
  if(f==vfo[id].filter) {
    vfo_filter_changed(f);
  }
}

static void var_spin_high_cb (GtkWidget *widget, gpointer data) {
  int f=GPOINTER_TO_UINT(data);
  int id=active_receiver->id;

  FILTER *mode_filters=filters[vfo[id].mode];
  FILTER *filter=&mode_filters[f];

  filter->high=gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(widget));
  g_print("%s: new values=(%d:%d)\n", __FUNCTION__,filter->low,filter->high);
  if(f==vfo[id].filter) {
    vfo_filter_changed(f);
  }
}

void filter_menu(GtkWidget *parent) {
  int i;

  dialog=gtk_dialog_new();
  gtk_window_set_transient_for(GTK_WINDOW(dialog),GTK_WINDOW(parent));
  char title[64];
  sprintf(title,"piHPSDR - Filter (RX %d VFO %s)",active_receiver->id,active_receiver->id==0?"A":"B");
  gtk_window_set_title(GTK_WINDOW(dialog),title);
  g_signal_connect (dialog, "delete_event", G_CALLBACK (delete_event), NULL);
  set_backgnd(dialog);

  GtkWidget *content=gtk_dialog_get_content_area(GTK_DIALOG(dialog));

  GtkWidget *grid=gtk_grid_new();

  gtk_grid_set_column_homogeneous(GTK_GRID(grid),TRUE);
  gtk_grid_set_row_homogeneous(GTK_GRID(grid),TRUE);
  gtk_grid_set_column_spacing (GTK_GRID(grid),5);
  gtk_grid_set_row_spacing (GTK_GRID(grid),5);

  GtkWidget *close_b=gtk_button_new_with_label("Close");
  g_signal_connect (close_b, "pressed", G_CALLBACK(close_cb), NULL);
  gtk_grid_attach(GTK_GRID(grid),close_b,0,0,1,1);

  FILTER* band_filters=filters[vfo[active_receiver->id].mode];

  switch(vfo[active_receiver->id].mode) {
    case modeFMN:
      {
      GtkWidget *l=gtk_label_new("Deviation:");
      gtk_grid_attach(GTK_GRID(grid),l,0,1,1,1);

      GtkWidget *b=gtk_button_new_with_label("2.5K");
      if(active_receiver->deviation==2500) {
        set_button_text_color(b,"orange");
        last_filter=b;
      } else {
        set_button_text_color(b,"default");
      }
      g_signal_connect(b,"pressed",G_CALLBACK(deviation_select_cb),(gpointer)(long)2500);
      gtk_grid_attach(GTK_GRID(grid),b,1,1,1,1);

      b=gtk_button_new_with_label("5.0K");
      if(active_receiver->deviation==5000) {
        set_button_text_color(b,"orange");
        last_filter=b;
      } else {
        set_button_text_color(b,"default");
      }
      g_signal_connect(b,"pressed",G_CALLBACK(deviation_select_cb),(gpointer)(long)5000);
      gtk_grid_attach(GTK_GRID(grid),b,2,1,1,1);
      }
      break;

    default:
      for(i=0;i<filterVar1;i++) {
        GtkWidget *b=gtk_button_new_with_label(band_filters[i].title);
        if(i==vfo[active_receiver->id].filter) {
          set_button_text_color(b,"orange");
          last_filter=b;
        } else {
          set_button_text_color(b,"default");
        }
        gtk_grid_attach(GTK_GRID(grid),b,i%5,1+(i/5),1,1);
        g_signal_connect(b,"pressed",G_CALLBACK(filter_select_cb),(gpointer)(long)i);
      }

      // last 2 are var1 and var2
      int row=1+((i+4)/5);
      int col=0;
      i=filterVar1;
      FILTER* band_filter=&band_filters[i];
      GtkWidget *b=gtk_button_new_with_label(band_filters[i].title);
      if(i==vfo[active_receiver->id].filter) {
        set_button_text_color(b,"orange");
        last_filter=b;
      } else {
        set_button_text_color(b,"default");
      }
      gtk_grid_attach(GTK_GRID(grid),b,col,row,1,1);
      g_signal_connect(b,"pressed",G_CALLBACK(filter_select_cb),(gpointer)(long)i);

      col++;
      //
      // Filter width in CW, filter low in other modes. In CW, use smaller granularity
      //
      if(vfo[active_receiver->id].mode==modeCWL || vfo[active_receiver->id].mode==modeCWU) {
        var1_spin_low=gtk_spin_button_new_with_range(0,+8000.0,2.0);
        gtk_spin_button_set_value(GTK_SPIN_BUTTON(var1_spin_low),(double)(band_filter->high - band_filter->low));
      } else {
        var1_spin_low=gtk_spin_button_new_with_range(-8000.0,+8000.0,5.0);
        gtk_spin_button_set_value(GTK_SPIN_BUTTON(var1_spin_low),(double)band_filter->low);
      }
      gtk_grid_attach(GTK_GRID(grid),var1_spin_low,col,row,1,1);
      g_signal_connect(var1_spin_low,"value-changed",G_CALLBACK(var_spin_low_cb),(gpointer)(long)i);
      col++;


      if(vfo[active_receiver->id].mode!=modeCWL && vfo[active_receiver->id].mode!=modeCWU) {
        var1_spin_high=gtk_spin_button_new_with_range(-8000.0,+8000.0,5.0);
        gtk_spin_button_set_value(GTK_SPIN_BUTTON(var1_spin_high),(double)band_filter->high);
        gtk_grid_attach(GTK_GRID(grid),var1_spin_high,col,row,1,1);
        g_signal_connect(var1_spin_high,"value-changed",G_CALLBACK(var_spin_high_cb),(gpointer)(long)i);
        col++;
      }

      GtkWidget *var1_default_b=gtk_button_new_with_label("Default");
      g_signal_connect (var1_default_b, "pressed", G_CALLBACK(default_cb), GINT_TO_POINTER(i));
      gtk_grid_attach(GTK_GRID(grid),var1_default_b,col,row,1,1);


      row++;
      col=0;
      i=filterVar2;
      band_filter=&band_filters[i];
      b=gtk_button_new_with_label(band_filters[i].title);
      if(i==vfo[active_receiver->id].filter) {
        set_button_text_color(b,"orange");
        last_filter=b;
      } else {
        set_button_text_color(b,"default");
      }
      gtk_grid_attach(GTK_GRID(grid),b,col,row,1,1);
      g_signal_connect(b,"pressed",G_CALLBACK(filter_select_cb),(gpointer)(long)i);
      col++;

      var2_spin_low=gtk_spin_button_new_with_range(-8000.0,+8000.0,1.0);
      //
      // in CW, filter_low is HALF of the filter width
      //
      if(vfo[active_receiver->id].mode==modeCWL || vfo[active_receiver->id].mode==modeCWU) {
        gtk_spin_button_set_value(GTK_SPIN_BUTTON(var2_spin_low),(double)(band_filter->high-band_filter->low));
      } else {
        gtk_spin_button_set_value(GTK_SPIN_BUTTON(var2_spin_low),(double)band_filter->low);
      }
      gtk_grid_attach(GTK_GRID(grid),var2_spin_low,col,row,1,1);
      g_signal_connect(var2_spin_low,"value-changed",G_CALLBACK(var_spin_low_cb),(gpointer)(long)i);
      col++;

      if(vfo[active_receiver->id].mode!=modeCWL && vfo[active_receiver->id].mode!=modeCWU) {
        var2_spin_high=gtk_spin_button_new_with_range(-8000.0,+8000.0,1.0);
        gtk_spin_button_set_value(GTK_SPIN_BUTTON(var2_spin_high),(double)band_filter->high);
        gtk_grid_attach(GTK_GRID(grid),var2_spin_high,2,row,1,1);
        g_signal_connect(var2_spin_high,"value-changed",G_CALLBACK(var_spin_high_cb),(gpointer)(long)i);
        col++;
      }

      GtkWidget *var2_default_b=gtk_button_new_with_label("Default");
      g_signal_connect (var2_default_b, "pressed", G_CALLBACK(default_cb), GINT_TO_POINTER(i));
      gtk_grid_attach(GTK_GRID(grid),var2_default_b,col,row,1,1);

      break;
  }

  gtk_container_add(GTK_CONTAINER(content),grid);

  sub_menu=dialog;

  gtk_widget_show_all(dialog);

}
