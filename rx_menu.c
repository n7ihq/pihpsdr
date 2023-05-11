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

#include "audio.h"
#include "new_menu.h"
#include "rx_menu.h"
#include "band.h"
#include "discovered.h"
#include "filter.h"
#include "radio.h"
#include "receiver.h"
#include "sliders.h"
#include "new_protocol.h"

static GtkWidget *parent_window=NULL;
static GtkWidget *menu_b=NULL;
static GtkWidget *dialog=NULL;
static GtkWidget *local_audio_b=NULL;
static GtkWidget *output=NULL;

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

static void dither_cb(GtkWidget *widget, gpointer data) {
  active_receiver->dither=active_receiver->dither==1?0:1;
  if (protocol == NEW_PROTOCOL) {
    schedule_receive_specific();
  }
}

static void random_cb(GtkWidget *widget, gpointer data) {
  active_receiver->random=active_receiver->random==1?0:1;
  if (protocol == NEW_PROTOCOL) {
    schedule_receive_specific();
  }
}

static void preamp_cb(GtkWidget *widget, gpointer data) {
  active_receiver->preamp=active_receiver->preamp==1?0:1;
}

static void alex_att_cb(GtkWidget *widget, gpointer data) {
  int val=gtk_combo_box_get_active(GTK_COMBO_BOX(widget));
  set_alex_attenuation(val);
}

static void sample_rate_cb(GtkToggleButton *widget, gpointer data) {
  char *p = gtk_combo_box_text_get_active_text(GTK_COMBO_BOX_TEXT(widget));
  int samplerate;

  //  
  // There are so many different possibilities for sample rates, so
  // we just "scanf" from the combobox text entry
  //
  if (sscanf(p, "%d", &samplerate) != 1) return;
  receiver_change_sample_rate(active_receiver,samplerate);
}

static void adc_cb(GtkToggleButton *widget,gpointer data) {
  int val=gtk_combo_box_get_active(GTK_COMBO_BOX(widget));
  receiver_change_adc(active_receiver,val);
}

static void local_audio_cb(GtkWidget *widget, gpointer data) {
fprintf(stderr,"local_audio_cb: rx=%d\n",active_receiver->id);

  if(active_receiver->audio_name!=NULL) {
    g_free(active_receiver->audio_name);
    active_receiver->audio_name=NULL;
  }

  int i=gtk_combo_box_get_active(GTK_COMBO_BOX(output));
  active_receiver->audio_name=g_new(gchar,strlen(output_devices[i].name)+1);
  strcpy(active_receiver->audio_name,output_devices[i].name);

  if(gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget))) {
    if(audio_open_output(active_receiver)==0) {
      active_receiver->local_audio=1;
    } else {
fprintf(stderr,"local_audio_cb: audio_open_output failed\n");
      active_receiver->local_audio=0;
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget), FALSE);
    }
  } else {
    if(active_receiver->local_audio) {
      active_receiver->local_audio=0;
      audio_close_output(active_receiver);
    }
  }
fprintf(stderr,"local_audio_cb: local_audio=%d\n",active_receiver->local_audio);
}

static void mute_audio_cb(GtkWidget *widget, gpointer data) {
  active_receiver->mute_when_not_active=gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget));
}

static void mute_radio_cb(GtkWidget *widget, gpointer data) {
  active_receiver->mute_radio=gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget));
}

//
// possible the device has been changed:
// call audo_close_output with old device, audio_open_output with new one
//
static void local_output_changed_cb(GtkWidget *widget, gpointer data) {
  int i = gtk_combo_box_get_active(GTK_COMBO_BOX(widget));

  if(active_receiver->local_audio) {
    audio_close_output(active_receiver);                     // audio_close with OLD device
  }

  if(active_receiver->audio_name!=NULL) {
    g_free(active_receiver->audio_name);
    active_receiver->audio_name=NULL;
  }

  if(i>=0) {
    fprintf(stderr,"local_output_changed rx=%d %s\n",active_receiver->id,output_devices[i].name);
    active_receiver->audio_name=g_new(gchar,strlen(output_devices[i].name)+1);
    strcpy(active_receiver->audio_name,output_devices[i].name);
  }

  if(active_receiver->local_audio) {
    if(audio_open_output(active_receiver)<0) {              // audio_open with NEW device
      active_receiver->local_audio=0;
      gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON (local_audio_b),FALSE);
    }
  }
  fprintf(stderr,"local_output_changed rx=%d local_audio=%d\n",active_receiver->id,active_receiver->local_audio);
}

static void audio_channel_cb(GtkWidget *widget, gpointer data) {
  int val = gtk_combo_box_get_active(GTK_COMBO_BOX(widget));
  switch (val) {
    case 0:
      active_receiver->audio_channel=STEREO;
      break;
    case 1:
      active_receiver->audio_channel=LEFT;
      break;
    case 2:
      active_receiver->audio_channel=RIGHT;
      break;
    }
    fprintf(stderr,"CHANNEL=%d\n", active_receiver->audio_channel);
}

void rx_menu(GtkWidget *parent) {
  char label[32];
  GtkWidget *adc_b;
  int i;
  parent_window=parent;

  dialog=gtk_dialog_new();
  gtk_window_set_transient_for(GTK_WINDOW(dialog),GTK_WINDOW(parent_window));
  char title[64];
  sprintf(title,"piHPSDR - Receive (RX %d VFO %s)",active_receiver->id,active_receiver->id==0?"A":"B");
  gtk_window_set_title(GTK_WINDOW(dialog),title);
  g_signal_connect (dialog, "delete_event", G_CALLBACK (delete_event), NULL);
  set_backgnd(dialog);

  GtkWidget *content=gtk_dialog_get_content_area(GTK_DIALOG(dialog));

  GtkWidget *grid=gtk_grid_new();
  gtk_grid_set_column_spacing (GTK_GRID(grid),10);

  GtkWidget *close_b=gtk_button_new_with_label("Close");
  g_signal_connect (close_b, "button_press_event", G_CALLBACK(close_cb), NULL);
  gtk_grid_attach(GTK_GRID(grid),close_b,0,0,1,1);

  int row, col;

  row=1;
  switch(protocol) {
    case NEW_PROTOCOL:
      {
      GtkWidget *sample_rate_label=gtk_label_new(NULL);
      gtk_label_set_markup(GTK_LABEL(sample_rate_label), "<b>Sample Rate</b>");
      gtk_grid_attach(GTK_GRID(grid),sample_rate_label,0,row,1,1);

      GtkWidget *sample_rate_combo_box=gtk_combo_box_text_new();
      gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(sample_rate_combo_box),NULL,"48000");
      gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(sample_rate_combo_box),NULL,"96000");
      gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(sample_rate_combo_box),NULL,"192000");
      gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(sample_rate_combo_box),NULL,"384000");
#ifndef raspberrypi
      gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(sample_rate_combo_box),NULL,"768000");
      gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(sample_rate_combo_box),NULL,"1536000");
#endif
      switch (active_receiver->sample_rate) {
         case 48000:
            gtk_combo_box_set_active(GTK_COMBO_BOX(sample_rate_combo_box),0);
            break;
         case 96000:
            gtk_combo_box_set_active(GTK_COMBO_BOX(sample_rate_combo_box),1);
            break;
         case 192000:
            gtk_combo_box_set_active(GTK_COMBO_BOX(sample_rate_combo_box),2);
            break;
         case 384000:
            gtk_combo_box_set_active(GTK_COMBO_BOX(sample_rate_combo_box),3);
            break;
#ifndef raspberrypi
         case 768000:
            gtk_combo_box_set_active(GTK_COMBO_BOX(sample_rate_combo_box),4);
            break;
         case 1536000:
            gtk_combo_box_set_active(GTK_COMBO_BOX(sample_rate_combo_box),5);
            break;
#endif
      }
      my_combo_attach(GTK_GRID(grid),sample_rate_combo_box,1,row,1,1);
      g_signal_connect(sample_rate_combo_box,"changed",G_CALLBACK(sample_rate_cb),NULL);
      }
      row++;
      break;

#ifdef SOAPYSDR
    case SOAPYSDR_PROTOCOL:
      {
      GtkWidget *sample_rate_label=gtk_label_new(NULL);
      gtk_label_set_markup(GTK_LABEL(sample_rate_label), "<b>Sample Rate</b>");
      gtk_grid_attach(GTK_GRID(grid),sample_rate_label,0,row,1,1);

      char rate_string[16];
      sprintf(rate_string,"%d",radio->info.soapy.sample_rate);
      GtkWidget *sample_rate_combo_box=gtk_combo_box_text_new();
      gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(sample_rate_combo_box),NULL,rate_string);
      gtk_grid_attach(GTK_GRID(grid),sample_rate_combo_box,1,row,1,1);

      int rate=radio->info.soapy.sample_rate/2;
      while(rate>=48000) {
          sprintf(rate_string,"%d",rate);
          gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(sample_rate_combo_box),NULL,rate_string);
          rate=rate/2;
      }
      my_combo_attach(GTK_GRID(grid),sample_rate_combo_box,col,row,1,1);
      g_signal_connect(sample_rate_combo_box,"changed",G_CALLBACK(sample_rate_cb),NULL);
      }
      row++;
      break;
#endif
  }

  if (filter_board == ALEX && active_receiver->adc == 0
      && ((protocol==ORIGINAL_PROTOCOL && device != DEVICE_ORION2) || (protocol==NEW_PROTOCOL && device != NEW_DEVICE_ORION2))) {
      
    // 
    // The "Alex ATT" value is stored in receiver[0] no matter how the ADCs are selected
    //
    GtkWidget *alex_att_label=gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(alex_att_label), "<b>Alex Attenuator</b>");
    gtk_grid_attach(GTK_GRID(grid), alex_att_label, 0, row, 1, 1);

    GtkWidget *alex_att_combo_box=gtk_combo_box_text_new();
    gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(alex_att_combo_box),NULL," 0 dB");
    gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(alex_att_combo_box),NULL,"10 dB");
    gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(alex_att_combo_box),NULL,"20 dB");
    gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(alex_att_combo_box),NULL,"30 dB");
    gtk_combo_box_set_active(GTK_COMBO_BOX(alex_att_combo_box),receiver[0]->alex_attenuation);

    my_combo_attach(GTK_GRID(grid), alex_att_combo_box, 1, row, 1, 1);
    g_signal_connect(alex_att_combo_box,"changed",G_CALLBACK(alex_att_cb),NULL);

    row++;
  }

  //
  // If there is more than one ADC, let the user associate an ADC
  // with the current receiver.
  //
  if(n_adc>1) {
    GtkWidget *adc_label=gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(adc_label), "<b>Select ADC</b>");
    gtk_grid_attach(GTK_GRID(grid), adc_label, 0, row, 1, 1);

    GtkWidget *adc_combo_box=gtk_combo_box_text_new();
    for(i=0;i<n_adc;i++) {
      sprintf(label,"ADC-%d",i);
      gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(adc_combo_box),NULL,label);
    }
    gtk_combo_box_set_active(GTK_COMBO_BOX(adc_combo_box),active_receiver->adc);
    my_combo_attach(GTK_GRID(grid), adc_combo_box, 1, row, 1, 1);
    g_signal_connect(adc_combo_box,"changed",G_CALLBACK(adc_cb),NULL);
    row++;
  }

  //
  // The CHARLY25 board (with RedPitaya) has no support for dither or random,
  // so those are left out. For Charly25, PreAmps and Alex Attenuator are controlled via
  // the sliders menu.
  //
  // Preamps are switchable only for the very first HPSDR radios
  //
  // We assume Alex attenuators are present if we have an ALEX board and no Orion2
  // (ANAN-7000/8000 do not have these), and if the RX is fed by the first ADC.
  //
  if (filter_board != CHARLY25) {
      GtkWidget *dither_b=gtk_check_button_new_with_label("Dither");
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (dither_b), active_receiver->dither);
      gtk_grid_attach(GTK_GRID(grid),dither_b,0,row,1,1);
      g_signal_connect(dither_b,"toggled",G_CALLBACK(dither_cb),NULL);

      GtkWidget *random_b=gtk_check_button_new_with_label("Random");
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (random_b), active_receiver->random);
      gtk_grid_attach(GTK_GRID(grid),random_b,1,row,1,1);
      g_signal_connect(random_b,"toggled",G_CALLBACK(random_cb),NULL);
      row++;

      if((protocol==ORIGINAL_PROTOCOL && device == DEVICE_METIS) ||
#ifdef USBOZY
         (protocol==ORIGINAL_PROTOCOL && device == DEVICE_OZY) ||
#endif
	 (protocol==NEW_PROTOCOL && device == NEW_DEVICE_ATLAS)) {
        GtkWidget *preamp_b=gtk_check_button_new_with_label("Preamp");
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (preamp_b), active_receiver->preamp);
        gtk_grid_attach(GTK_GRID(grid),preamp_b,0,row,1,1);
        g_signal_connect(preamp_b,"toggled",G_CALLBACK(preamp_cb),NULL);
        row++;
      }
  }

  row=1;
  if(n_output_devices>0) {
    local_audio_b=gtk_check_button_new_with_label("Local Audio Output:");
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (local_audio_b), active_receiver->local_audio);
    gtk_widget_show(local_audio_b);
    gtk_grid_attach(GTK_GRID(grid),local_audio_b,2,1,1,1);
    g_signal_connect(local_audio_b,"toggled",G_CALLBACK(local_audio_cb),NULL);

    if(active_receiver->audio_device==-1) active_receiver->audio_device=0;

    output=gtk_combo_box_text_new();
    for(i=0;i<n_output_devices;i++) {
      gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(output),NULL,output_devices[i].description);
      if(active_receiver->audio_name!=NULL) {
        if(strcmp(active_receiver->audio_name,output_devices[i].name)==0) {
          gtk_combo_box_set_active(GTK_COMBO_BOX(output),i);
        }
      }
    }

    i=gtk_combo_box_get_active(GTK_COMBO_BOX(output));
    if (i < 0) {
      gtk_combo_box_set_active(GTK_COMBO_BOX(output),0);
      if (active_receiver->audio_name != NULL) {
        g_free(active_receiver->audio_name);
        active_receiver->audio_name=g_new(gchar,strlen(output_devices[0].name)+1);
        strcpy(active_receiver->audio_name,output_devices[0].name);
      }
    }

    my_combo_attach(GTK_GRID(grid),output,3,1,1,1);
    g_signal_connect(output,"changed",G_CALLBACK(local_output_changed_cb),NULL);

    GtkWidget *channel=gtk_combo_box_text_new();
    gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(channel),NULL,"STEREO");
    gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(channel),NULL,"LEFT");
    gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(channel),NULL,"RIGHT");

    switch (active_receiver->audio_channel) {
      case STEREO:
        gtk_combo_box_set_active(GTK_COMBO_BOX(channel),0);
        break;
      case LEFT:
        gtk_combo_box_set_active(GTK_COMBO_BOX(channel),1);
        break;
      case RIGHT:
        gtk_combo_box_set_active(GTK_COMBO_BOX(channel),2);
        break;
    }
    my_combo_attach(GTK_GRID(grid),channel,3,2,1,1);
    g_signal_connect(channel,"changed",G_CALLBACK(audio_channel_cb),NULL);

    row=2;
  }

  GtkWidget *mute_audio_b=gtk_check_button_new_with_label("Mute when not active");
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (mute_audio_b), active_receiver->mute_when_not_active);
  gtk_widget_show(mute_audio_b);
  gtk_grid_attach(GTK_GRID(grid),mute_audio_b,2,row,1,1);
  g_signal_connect(mute_audio_b,"toggled",G_CALLBACK(mute_audio_cb),NULL);
  row++;

  GtkWidget *mute_radio_b=gtk_check_button_new_with_label("Mute audio to radio");
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (mute_radio_b), active_receiver->mute_radio);
  gtk_widget_show(mute_radio_b);
  gtk_grid_attach(GTK_GRID(grid),mute_radio_b,2,row,1,1);
  g_signal_connect(mute_radio_b,"toggled",G_CALLBACK(mute_radio_cb),NULL);

  gtk_container_add(GTK_CONTAINER(content),grid);

  sub_menu=dialog;

  gtk_widget_show_all(dialog);

}

