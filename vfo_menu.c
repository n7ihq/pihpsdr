/* Copyright (C)
* 2016 - John Melton, G0ORX/N6LYT
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
#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <wdsp.h>

#include "new_menu.h"
#include "band.h"
#include "filter.h"
#include "mode.h"
#include "radio.h"
#include "receiver.h"
#include "transmitter.h"
#include "vfo.h"
#include "button_text.h"
#include "ext.h"

static gint v;  //  VFO the menu is referring to
static GtkWidget *dialog=NULL;
static GtkWidget *label;

#define BUF_SIZE 88

//
// Note that the decimal point is hard-wired to a point,
// which may be incompatible with the LOCALE setting
// such that atof() and friends do not understand it.
// This is taken care of below
//
static char *btn_labels[] = {"1","2","3",
               "4","5","6",
               "7","8","9",
               ".","0","BS",
               "Hz","kHz","MHz"
               ,"CL"
              };

//
// The parameters for num_pad corresponding to the button labels
//
static int btn_actions[] = {1,2,3,4,5,6,7,8,9,-5,0,-6,-2,-3,-4,-1};

static GtkWidget *btn[16];

static void cleanup() {
  if(dialog!=NULL) {
    gtk_widget_destroy(dialog);
    dialog=NULL;
    sub_menu=NULL;
    active_menu=NO_MENU;
  }
  num_pad(-1, v);
}

static gboolean close_cb (GtkWidget *widget, GdkEventButton *event, gpointer data) {
  cleanup();
  return TRUE;
}

static gboolean delete_event(GtkWidget *widget, GdkEvent *event, gpointer user_data) {
  cleanup();
  return FALSE;
}

static void squelch_enable_cb(GtkWidget *widget, gpointer data) {
  active_receiver->squelch_enable=gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget));
  setSquelch(active_receiver);
}

static gboolean num_pad_cb(GtkWidget *widget, gpointer data) {
  int val = GPOINTER_TO_INT(data);
  char output[64];
  num_pad(btn_actions[val],v);
  if (vfo[v].entered_frequency[0]) {
    sprintf(output, "<big>%s</big>", vfo[v].entered_frequency);
  } else {
    sprintf(output, "<big>0</big>");
  }
  gtk_label_set_markup (GTK_LABEL (label), output);
  return FALSE;
}

static void rit_cb(GtkComboBox *widget,gpointer data) {
  switch(gtk_combo_box_get_active(widget)) {
    case 0:
      rit_increment=1;
      break;
    case 1:
      rit_increment=10;
      break;
    case 2:
      rit_increment=100;
      break;
  }
  g_idle_add(ext_vfo_update,NULL);
}

static void vfo_cb(GtkComboBox *widget,gpointer data) {
  vfo_set_step_from_index(gtk_combo_box_get_active(widget));
  g_idle_add(ext_vfo_update,NULL);
}

static void enable_ps_cb(GtkWidget *widget, gpointer data) {
  tx_set_ps(transmitter,gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget)));
}

static void set_btn_state() {
  int i;

  for(i=0;i<16;i++) {
    gtk_widget_set_sensitive(btn[i], locked==0);
  }
}

static void lock_cb(GtkWidget *widget, gpointer data) {
  locked=locked==1?0:1;
  set_btn_state();
  g_idle_add(ext_vfo_update,NULL);
}

void vfo_menu(GtkWidget *parent,int vfo) {
  int i;
  v=vfo;  // store this for cleanup()

  dialog=gtk_dialog_new();
  gtk_window_set_transient_for(GTK_WINDOW(dialog),GTK_WINDOW(parent));
  char title[64];
  sprintf(title,"piHPSDR - VFO %s",vfo==0?"A":"B");
  gtk_window_set_title(GTK_WINDOW(dialog),title);
  g_signal_connect (dialog, "delete_event", G_CALLBACK (delete_event), NULL);
  set_backgnd(dialog);

  GtkWidget *content=gtk_dialog_get_content_area(GTK_DIALOG(dialog));

  GtkWidget *grid=gtk_grid_new();

  gtk_grid_set_column_homogeneous(GTK_GRID(grid),FALSE);
  gtk_grid_set_row_homogeneous(GTK_GRID(grid),TRUE);
  gtk_grid_set_column_spacing (GTK_GRID(grid),4);
  gtk_grid_set_row_spacing (GTK_GRID(grid),4);

  GtkWidget *close_b=gtk_button_new_with_label("Close");
  g_signal_connect (close_b, "pressed", G_CALLBACK(close_cb), NULL);
  gtk_grid_attach(GTK_GRID(grid),close_b,0,0,2,1);

  GtkWidget *lock_b=gtk_check_button_new_with_label("Lock VFO");
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (lock_b), locked);
  gtk_grid_attach(GTK_GRID(grid),lock_b,2,0,2,1);
  g_signal_connect(lock_b,"toggled",G_CALLBACK(lock_cb),NULL);

  label = gtk_label_new (NULL);
  gtk_label_set_markup (GTK_LABEL (label), "<big>0</big>");
  gtk_misc_set_alignment (GTK_MISC (label), 1, .5);
  gtk_grid_attach(GTK_GRID(grid),label,0,1,3,1);

  for (i=0; i<16; i++) {
    btn[i]=gtk_button_new_with_label(btn_labels[i]);
    set_button_text_color(btn[i],"default");
    gtk_widget_show(btn[i]);
    gtk_grid_attach(GTK_GRID(grid),btn[i],i%3,2+(i/3),1,1);
    g_signal_connect(btn[i],"pressed",G_CALLBACK(num_pad_cb),GINT_TO_POINTER(i));
  }
  set_btn_state();

  GtkWidget *rit_label=gtk_label_new("RIT step: ");
  gtk_grid_attach(GTK_GRID(grid),rit_label,3,2,1,1);

  GtkWidget *rit_b=gtk_combo_box_text_new();
  gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(rit_b),NULL,"1 Hz");
  gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(rit_b),NULL,"10 Hz");
  gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(rit_b),NULL,"100 Hz");
  switch(rit_increment) {
    case 1:
      gtk_combo_box_set_active(GTK_COMBO_BOX(rit_b), 0);
      break;
    case 10:
      gtk_combo_box_set_active(GTK_COMBO_BOX(rit_b), 1);
      break;
    case 100:
      gtk_combo_box_set_active(GTK_COMBO_BOX(rit_b), 2);
      break;
  }
  g_signal_connect(rit_b,"changed",G_CALLBACK(rit_cb),NULL);
  my_combo_attach(GTK_GRID(grid),rit_b,4,2,1,1);

  GtkWidget *vfo_label=gtk_label_new("VFO step: ");
  gtk_grid_attach(GTK_GRID(grid),vfo_label,3,3,1,1);

  GtkWidget *vfo_b=gtk_combo_box_text_new();
  int ind=vfo_get_stepindex();
  for (i=0; i<STEPS; i++) {
    gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(vfo_b),NULL,step_labels[i]);
    if(i == ind) {
      gtk_combo_box_set_active (GTK_COMBO_BOX(vfo_b), i);
    }
  }
  g_signal_connect(vfo_b,"changed",G_CALLBACK(vfo_cb),NULL);
  my_combo_attach(GTK_GRID(grid),vfo_b,4,3,1,1);


  if (!display_sliders) {
    //
    // If the sliders are "on display", then we also have a squelch-enable checkbox
    // in the display area.
    //
    GtkWidget *enable_squelch=gtk_check_button_new_with_label("Enable Squelch");
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (enable_squelch), active_receiver->squelch_enable);
    gtk_grid_attach(GTK_GRID(grid),enable_squelch,3,5,1,1);
    g_signal_connect(enable_squelch,"toggled",G_CALLBACK(squelch_enable_cb),NULL);
  }

  if(can_transmit && (protocol==ORIGINAL_PROTOCOL || protocol==NEW_PROTOCOL)) {
    GtkWidget *enable_ps=gtk_check_button_new_with_label("Enable Pure Signal");
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (enable_ps), transmitter->puresignal);
    gtk_grid_attach(GTK_GRID(grid),enable_ps,3,7,1,1);
    g_signal_connect(enable_ps,"toggled",G_CALLBACK(enable_ps_cb),NULL);
  }

  gtk_container_add(GTK_CONTAINER(content),grid);

  sub_menu=dialog;

  gtk_widget_show_all(dialog);

}

//
// This is also called when hitting buttons on the
// numpad of the MIDI device or the controller
//
void num_pad(int action, int id) {
  char decimalpoint[2];
  char scr[16];
  double mult=1.0; 
  double fd;
  long long fl;
  int len;

  //
  // On a localized system, a decimal point may be something
  // else (in Germany, a comma, for example). Therefore do
  // a dummy "printf" to determine the character atof() 
  // expects. Construct a one-character string ready for strcat().
  //
  sprintf(scr,"%1.1f",mult);
  decimalpoint[0]=scr[1];
  decimalpoint[1]=0;

  char *buffer=vfo[id].entered_frequency;

  switch (action) {
    case 0:
      strcat(buffer,"0");
      break;
    case 1:
      strcat(buffer,"1");
      break;
    case 2:
      strcat(buffer,"2");
      break;
    case 3:
      strcat(buffer,"3");
      break;
    case 4:
      strcat(buffer,"4");
      break;
    case 5:
      strcat(buffer,"5");
      break;
    case 6:
      strcat(buffer,"6");
      break;
    case 7:
      strcat(buffer,"7");
      break;
    case 8:
      strcat(buffer,"8");
      break;
    case 9:
      strcat(buffer,"9");
      break;
    case -5:  // Decimal point
      strcat(buffer, decimalpoint);
      break;
    case -1:  // Clear
      *buffer = 0;
      break;
    case -6:  // Backspace
      len=strlen(buffer);
      if (len > 0) buffer[len-1] = 0;
      break;
    case -4:  // Enter as MHz
      mult *= 1000.0;
      //FALLTHROUGH
    case -3:  // Enter as kHz
      mult *= 1000.0;
      //FALLTHROUGH 
    case -2:  // Enter
      fd = atof(buffer) * mult;
      fl = (long long) (fd + 0.5);
      *buffer=0;
      //
      // If the frequency is less than 10 kHz, this
      // is most likely not intended by the user,
      // so we do not set the frequency. This guards
      // against setting the frequency to zero just
      // by hitting the "NUMPAD enter" button.
      //
      if (fl >= 10000ll) {
#ifdef CLIENT_SERVER
        if(radio_is_remote) {
          send_vfo_frequency(client_socket,id,fl);
        } else {
#endif
          vfo_set_frequency(id, fl);
#ifdef CLIENT_SERVER
        }
#endif
      }
      break;
  }
  g_idle_add(ext_vfo_update, NULL);
}
