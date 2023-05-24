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
#include <stdlib.h>
#include <string.h>

#include "main.h"
#include "discovered.h"
#include "new_menu.h"
#include "radio_menu.h"
#include "adc.h"
#include "band.h"
#include "filter.h"
#include "radio.h"
#include "receiver.h"
#include "sliders.h"
#include "new_protocol.h"
#include "old_protocol.h"
#ifdef SOAPYSDR
#include "soapy_protocol.h"
#endif
#include "actions.h"
#ifdef GPIO
#include "gpio.h"
#endif
#include "vfo.h"
#include "ext.h"
#ifdef CLIENT_SERVER
#include "client_server.h"
#endif

static GtkWidget *menu_b=NULL;
static GtkWidget *dialog=NULL;
static GtkWidget *rx_gains[3];
static GtkWidget *tx_gain;
static GtkWidget *tx_gains[2];
static GtkWidget *sat_b;
static GtkWidget *rsat_b;

static GtkWidget *duplex_b;
static GtkWidget *mute_rx_b;

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

static void rf_gain_value_changed_cb(GtkWidget *widget, gpointer data) {
  ADC *myadc=(ADC *)data;
  myadc->gain=gtk_spin_button_get_value(GTK_SPIN_BUTTON(widget));

#ifdef SOAPYSDR
  if(device==SOAPYSDR_USB_DEVICE) {
    soapy_protocol_set_gain(receiver[0]);
  }
#endif
}

static void rx_gain_value_changed_cb(GtkWidget *widget, gpointer data) {
  ADC *myadc=(ADC *)data;
  if(device==SOAPYSDR_USB_DEVICE) {
    myadc->gain=gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(widget));
#ifdef SOAPYSDR
    soapy_protocol_set_gain_element(receiver[0],(char *)gtk_widget_get_name(widget),myadc->gain);
#endif
  }
}

static void drive_gain_value_changed_cb(GtkWidget *widget, gpointer data) {
  if(device==SOAPYSDR_USB_DEVICE) {
    // should use setDrive here to move the main drive slider
    transmitter->drive=gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(widget));
#ifdef SOAPYSDR
    soapy_protocol_set_tx_gain(transmitter,(double)transmitter->drive);
/*
    for(int i=0;i<radio->info.soapy.tx_gains;i++) {
      int value=soapy_protocol_get_tx_gain_element(transmitter,radio->info.soapy.tx_gain[i]);
      gtk_spin_button_set_value(GTK_SPIN_BUTTON(tx_gains[i]),(double)value);
    }
*/
#endif
  }
}

#if 0
static void tx_gain_value_changed_cb(GtkWidget *widget, gpointer data) {
  int gain;
  if(device==SOAPYSDR_USB_DEVICE) {
    gain=gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(widget));
#ifdef SOAPYSDR
    soapy_protocol_set_tx_gain_element(transmitter,(char *)gtk_widget_get_name(widget),gain);
#endif
  }
}
#endif

static void agc_changed_cb(GtkWidget *widget, gpointer data) {
  gboolean agc=gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));
#ifdef SOAPYSDR
  soapy_protocol_set_automatic_gain(active_receiver,agc);
  if(!agc) {
    soapy_protocol_set_gain(active_receiver);
  }
#endif
}

/*
static void dac0_gain_value_changed_cb(GtkWidget *widget, gpointer data) {
  DAC *dac=(DAC *)data;
  int gain;
#ifdef SOAPYSDR
  if(device==SOAPYSDR_USB_DEVICE) {
    gain=gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(widget));
    soapy_protocol_set_tx_gain_element(radio->transmitter,(char *)gtk_widget_get_name(widget),gain);
    for(int i=0;i<radio->discovered->info.soapy.tx_gains;i++) {
      if(strcmp(radio->discovered->info.soapy.tx_gain[i],(char *)gtk_widget_get_name(widget))==0) {
        radio->dac[0].tx_gain[i]=gain;
        break;
      }
    }
  }
#endif
}
*/

static void calibration_value_changed_cb(GtkWidget *widget, gpointer data) {
  frequency_calibration=(long long)gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(widget));
}

static void rx_gain_calibration_value_changed_cb(GtkWidget *widget, gpointer data) {
  rx_gain_calibration=gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(widget));
}

static void vfo_divisor_value_changed_cb(GtkWidget *widget, gpointer data) {
  vfo_encoder_divisor=gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(widget));
}

static void ptt_cb(GtkWidget *widget, gpointer data) {
  mic_ptt_enabled=gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));
}

static void ptt_ring_cb(GtkWidget *widget, gpointer data) {
  if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget))) {
    mic_ptt_tip_bias_ring=0;
  }
}

static void ptt_tip_cb(GtkWidget *widget, gpointer data) {
  if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget))) {
    mic_ptt_tip_bias_ring=1;
  }
}

static void bias_cb(GtkWidget *widget, gpointer data) {
  mic_bias_enabled=gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));
}

static void iqswap_cb(GtkWidget *widget, gpointer data) {
  iqswap=gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));
}

static void touchscreen_cb(GtkWidget *widget, gpointer data) {
  optimize_for_touchscreen=gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));
}

static void hl2audio_cb(GtkWidget *widget, gpointer data) {
  hl2_audio_codec=gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));
}

static void split_cb(GtkWidget *widget, gpointer data) {
  int new=gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));
  radio_set_split(new);
}

//
// call-able from outside, e.g. toolbar or MIDI, through g_idle_add
//
void setDuplex() {
  if(duplex) {
    gtk_container_remove(GTK_CONTAINER(fixed),transmitter->panel);
    reconfigure_transmitter(transmitter,display_width/4,display_height/2);
    create_dialog(transmitter);
  } else {
    GtkWidget *content=gtk_dialog_get_content_area(GTK_DIALOG(transmitter->dialog));
    gtk_container_remove(GTK_CONTAINER(content),transmitter->panel);
    gtk_widget_destroy(transmitter->dialog);
    transmitter->dialog=NULL;
    reconfigure_transmitter(transmitter,display_width,rx_height);
  }
  g_idle_add(ext_vfo_update, NULL);
}

static void PA_enable_cb(GtkWidget *widget, gpointer data) {
  pa_enabled=gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));
  if (protocol == NEW_PROTOCOL) {
    schedule_general();
  }
}

static void duplex_cb(GtkWidget *widget, gpointer data) {
  if (isTransmitting()) {
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (duplex_b), duplex);
    return;
  }
  duplex=gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));
  setDuplex();
}

static void mute_rx_cb(GtkWidget *widget, gpointer data) {
  mute_rx_while_transmitting=gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));
}

static void sat_cb(GtkWidget *widget, gpointer data) {
  sat_mode=gtk_combo_box_get_active(GTK_COMBO_BOX(widget));
  g_idle_add(ext_vfo_update, NULL);
}

void n2adr_oc_settings() {
    //
    // set OC outputs for each band according to the N2ADR board requirements
    // unlike load_filters(), this can be executed outside the GTK queue
    //
    BAND *band;
    band=band_get_band(band160);
    band->OCrx=band->OCtx=1;
    band=band_get_band(band80);
    band->OCrx=band->OCtx=66;
    band=band_get_band(band60);
    band->OCrx=band->OCtx=68;
    band=band_get_band(band40);
    band->OCrx=band->OCtx=68;
    band=band_get_band(band30);
    band->OCrx=band->OCtx=72;
    band=band_get_band(band20);
    band->OCrx=band->OCtx=72;
    band=band_get_band(band17);
    band->OCrx=band->OCtx=80;
    band=band_get_band(band15);
    band->OCrx=band->OCtx=80;
    band=band_get_band(band12);
    band->OCrx=band->OCtx=96;
    band=band_get_band(band10);
    band->OCrx=band->OCtx=96;
    if(protocol==NEW_PROTOCOL) {
      schedule_high_priority();
    }
}

void load_filters() {
  switch (filter_board) {
      case N2ADR:
        n2adr_oc_settings();
        break;
    case ALEX:
    case APOLLO:
    case CHARLY25:
        // This is most likely not necessary here, but can do no harm
        set_alex_antennas();
        break;
    case NONE:
        break;
    default:
        break;
  }
  //
  // This switches between StepAttenuator slider and CHARLY25 ATT/Preamp checkboxes
  // when the filter board is switched to/from CHARLY25
  //
  att_type_changed();
}

static void filter_cb(GtkWidget *widget, gpointer data) {
  int val = gtk_combo_box_get_active (GTK_COMBO_BOX(widget));
  switch (val) {
  case 0:
  default:
    filter_board=NONE;
    break;
  case 1:
    filter_board=ALEX;
    break;
  case 2:
    filter_board=APOLLO;
    break;
  case 3:
    filter_board=CHARLY25;
    break;
  case 4:
    filter_board=N2ADR;
    break;
  }
  load_filters();
}

static void sample_rate_cb(GtkToggleButton *widget, gpointer data) {
  char *p = gtk_combo_box_text_get_active_text(GTK_COMBO_BOX_TEXT(widget));
  int samplerate;

  //
  // There are so many different possibilities for sample rates, so
  // we just "scanf" from the combobox text entry
  //
  if (sscanf(p, "%d", &samplerate) != 1) return;
#ifdef CLIENT_SERVER
  if(radio_is_remote)
  {
    send_sample_rate(client_socket,-1,samplerate);
  }
  else
#endif
  {
    radio_change_sample_rate(samplerate);
  }
}

static void receivers_cb(GtkToggleButton *widget, gpointer data) {
  int val=gtk_combo_box_get_active (GTK_COMBO_BOX(widget))+1;
  //
  // reconfigure_radio requires that the RX panels are active
  // (segfault otherwise), therefore ignore this while TXing
  //
  if (isTransmitting()) {
    gtk_combo_box_set_active(GTK_COMBO_BOX(widget),receivers - 1);
    return;
  }
#ifdef CLIENT_SERVER
    if(radio_is_remote)
    {
      send_receivers(client_socket,val);
    }
    else
#endif
    {
      radio_change_receivers(val);
    }
}

static void region_cb(GtkWidget *widget, gpointer data) {
  radio_change_region(gtk_combo_box_get_active (GTK_COMBO_BOX(widget)));
}

static void rit_cb(GtkWidget *widget,gpointer data) {
  int val = gtk_combo_box_get_active (GTK_COMBO_BOX(widget));
  switch (val) {
    case 0:
    default:
      rit_increment=1;
      break;
    case 1:
      rit_increment=10;
      break;
    case 2:
      rit_increment=100;
      break;
  }
}

static void ck10mhz_cb(GtkWidget *widget, gpointer data) {
  atlas_clock_source_10mhz = gtk_combo_box_get_active (GTK_COMBO_BOX(widget));
}

static void ck128mhz_cb(GtkWidget *widget, gpointer data) {
  atlas_clock_source_128mhz=gtk_combo_box_get_active (GTK_COMBO_BOX(widget)) ? 1 : 0;
}

static void micsource_cb(GtkWidget *widget, gpointer data) {
  atlas_mic_source=gtk_combo_box_get_active (GTK_COMBO_BOX(widget)) ? 1 : 0;
}

static void tx_cb(GtkWidget *widget, gpointer data) {
  atlas_penelope=gtk_combo_box_get_active (GTK_COMBO_BOX(widget));
}

static void janus_cb(GtkWidget *widget, gpointer data) {
  atlas_janus=atlas_janus==1?0:1;
}

void radio_menu(GtkWidget *parent) {

  dialog=gtk_dialog_new();
  gtk_window_set_transient_for(GTK_WINDOW(dialog),GTK_WINDOW(parent));
  gtk_window_set_title(GTK_WINDOW(dialog),"piHPSDR - Radio");
  g_signal_connect (dialog, "delete_event", G_CALLBACK (delete_event), NULL);
  set_backgnd(dialog);

  GtkWidget *content=gtk_dialog_get_content_area(GTK_DIALOG(dialog));

  GtkWidget *grid=gtk_grid_new();
  gtk_grid_set_column_spacing (GTK_GRID(grid),5);
  gtk_grid_set_row_spacing (GTK_GRID(grid),5);
  gtk_grid_set_column_homogeneous (GTK_GRID(grid), FALSE);
  gtk_grid_set_row_homogeneous (GTK_GRID(grid), FALSE);

  int col=0;
  int row=0;
  int temp_row;

  GtkWidget *close_b=gtk_button_new_with_label("Close");
  g_signal_connect (close_b, "button_press_event", G_CALLBACK(close_cb), NULL);
  gtk_grid_attach(GTK_GRID(grid),close_b,col,row,1,1);


  col=0;
  row=1;

  GtkWidget *receivers_label=gtk_label_new(NULL);
  gtk_label_set_markup(GTK_LABEL(receivers_label), "<b>Receivers:</b>");
  gtk_label_set_justify(GTK_LABEL(receivers_label),GTK_JUSTIFY_LEFT);
  gtk_grid_attach(GTK_GRID(grid),receivers_label,col,row,1,1);

  row++;
  GtkWidget *receivers_combo=gtk_combo_box_text_new();
  gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(receivers_combo),NULL,"1");
  if(radio->supported_receivers>1) {
    gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(receivers_combo),NULL,"2");
  }
  gtk_combo_box_set_active(GTK_COMBO_BOX(receivers_combo),receivers - 1);
  my_combo_attach(GTK_GRID(grid),receivers_combo,col,row,1,1);
  g_signal_connect(receivers_combo,"changed",G_CALLBACK(receivers_cb),NULL);

  row++;


  switch(protocol) {
    case NEW_PROTOCOL:
      // Sample rate changes handled in the RX menu
      break;
    case ORIGINAL_PROTOCOL:
      {
      GtkWidget *sample_rate_label=gtk_label_new(NULL);
      gtk_label_set_markup(GTK_LABEL(sample_rate_label), "<b>Sample Rate:</b>");
      gtk_grid_attach(GTK_GRID(grid),sample_rate_label,col,row,1,1);
      row++;

      GtkWidget *sample_rate_combo_box=gtk_combo_box_text_new();
      gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(sample_rate_combo_box),NULL,"48000");
      gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(sample_rate_combo_box),NULL,"96000");
      gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(sample_rate_combo_box),NULL,"192000");
      gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(sample_rate_combo_box),NULL,"384000");
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
      }
      my_combo_attach(GTK_GRID(grid),sample_rate_combo_box,col,row,1,1);
      g_signal_connect(sample_rate_combo_box,"changed",G_CALLBACK(sample_rate_cb),NULL);
      row++;
      }
      break;
#ifdef SOAPYSDR
    case SOAPYSDR_PROTOCOL:
      {
      GtkWidget *sample_rate_label=gtk_label_new(NULL);
      gtk_label_set_markup(GTK_LABEL(sample_rate_label), "<b>Sample Rate:</b>");
      gtk_grid_attach(GTK_GRID(grid),sample_rate_label,col,row,1,1);
      row++;

      char rate_string[16];
      GtkWidget *sample_rate_combo_box=gtk_combo_box_text_new();

      int rate=radio->info.soapy.sample_rate;
      int pos=0;
      while(rate>=48000) {
          sprintf(rate_string,"%d",rate);
          gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(sample_rate_combo_box),NULL,rate_string);
          if (rate == active_receiver->sample_rate) {
            gtk_combo_box_set_active(GTK_COMBO_BOX(sample_rate_combo_box),pos);
          }
          rate=rate/2;
          pos++;
      }
      my_combo_attach(GTK_GRID(grid),sample_rate_combo_box,col,row,1,1);
      g_signal_connect(sample_rate_combo_box,"changed",G_CALLBACK(sample_rate_cb),NULL);
      }
      row++;
      break;
#endif
  }


  temp_row=row;
  col++,
  row=1;

  GtkWidget *rit_label=gtk_label_new(NULL);
  gtk_label_set_markup(GTK_LABEL(rit_label), "<b>RIT/XIT step (Hz):</b>");
  gtk_grid_attach(GTK_GRID(grid),rit_label,col,row,1,1);
  row++;


  GtkWidget *rit_combo=gtk_combo_box_text_new();
  gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(rit_combo),NULL,"1");
  gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(rit_combo),NULL,"10");
  gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(rit_combo),NULL,"100");

  switch (rit_increment) {
    default:
      rit_increment=1;
      // fallthrough
    case 1:
      gtk_combo_box_set_active(GTK_COMBO_BOX(rit_combo),0);
      break;
    case 10:
      gtk_combo_box_set_active(GTK_COMBO_BOX(rit_combo),1);
      break;
    case 100:
      gtk_combo_box_set_active(GTK_COMBO_BOX(rit_combo),2);
      break;
  }
  my_combo_attach(GTK_GRID(grid), rit_combo, col, row, 1, 1);
  g_signal_connect(rit_combo,"changed",G_CALLBACK(rit_cb),NULL);
  row++;

  GtkWidget *sat_label=gtk_label_new(NULL);
  gtk_label_set_markup(GTK_LABEL(sat_label), "<b>SAT mode:</b>");
  gtk_grid_attach(GTK_GRID(grid),sat_label,col,row,1,1);
  row++;

  GtkWidget *sat_combo=gtk_combo_box_text_new();
  gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(sat_combo),NULL,"SAT Off");
  gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(sat_combo),NULL,"SAT");
  gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(sat_combo),NULL,"RSAT");
  gtk_combo_box_set_active(GTK_COMBO_BOX(sat_combo),sat_mode);
  my_combo_attach(GTK_GRID(grid),sat_combo,col,row,1,1);
  g_signal_connect(sat_combo,"changed",G_CALLBACK(sat_cb),NULL);
  row++;

  if(row>temp_row) temp_row=row;
  col++;
  row=1;

  GtkWidget *region_label=gtk_label_new(NULL);
  gtk_label_set_markup(GTK_LABEL(region_label), "<b>Region:</b>");
  gtk_grid_attach(GTK_GRID(grid),region_label,col,row,1,1);
  row++;

  GtkWidget *region_combo=gtk_combo_box_text_new();
  gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(region_combo),NULL,"Other");
  gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(region_combo),NULL,"UK");
  gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(region_combo),NULL,"WRC15");
  gtk_combo_box_set_active(GTK_COMBO_BOX(region_combo),region);
  my_combo_attach(GTK_GRID(grid),region_combo,col,row,1,1);
  g_signal_connect(region_combo,"changed",G_CALLBACK(region_cb),NULL);
  row++;


  if(protocol==ORIGINAL_PROTOCOL || protocol==NEW_PROTOCOL) {

    GtkWidget *filter_board_label=gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(filter_board_label), "<b>Filter Board:</b>");
    gtk_grid_attach(GTK_GRID(grid),filter_board_label,col,row,1,1);
    row++;

    GtkWidget *filter_combo=gtk_combo_box_text_new();
    gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(filter_combo),NULL,"NONE");
    gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(filter_combo),NULL,"ALEX");
    gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(filter_combo),NULL,"APOLLO");
    gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(filter_combo),NULL,"CHARLY25");
    gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(filter_combo),NULL,"N2ADR");

    switch (filter_board) {
      case NONE:
        gtk_combo_box_set_active(GTK_COMBO_BOX(filter_combo),0);
        break;
      case ALEX:
        gtk_combo_box_set_active(GTK_COMBO_BOX(filter_combo),1);
        break;
      case APOLLO:
        gtk_combo_box_set_active(GTK_COMBO_BOX(filter_combo),2);
        break;
      case CHARLY25:
        gtk_combo_box_set_active(GTK_COMBO_BOX(filter_combo),3);
        break;
      case N2ADR:
        gtk_combo_box_set_active(GTK_COMBO_BOX(filter_combo),4);
        break;
    }
    my_combo_attach(GTK_GRID(grid), filter_combo, col, row, 1, 1);
    g_signal_connect(filter_combo,"changed",G_CALLBACK(filter_cb),NULL);

    row++;

  }

#ifdef GPIO
  GtkWidget *vfo_divisor_label=gtk_label_new(NULL);
  gtk_label_set_markup(GTK_LABEL(vfo_divisor_label), "<b>VFO Encoder Divisor:</b>");
  gtk_grid_attach(GTK_GRID(grid),vfo_divisor_label,col,row,1,1);
  row++;

  GtkWidget *vfo_divisor=gtk_spin_button_new_with_range(1.0,60.0,1.0);
  gtk_spin_button_set_value(GTK_SPIN_BUTTON(vfo_divisor),(double)vfo_encoder_divisor);
  gtk_grid_attach(GTK_GRID(grid),vfo_divisor,col,row,1,1);
  g_signal_connect(vfo_divisor,"value_changed",G_CALLBACK(vfo_divisor_value_changed_cb),NULL);
  row++;
#endif

  if(row>temp_row) temp_row=row;

  //
  // The HPSDR machine-specific stuff is now put in the last column(s),
  // either the ATLAS bits (METIS) or the ORION microphone settings
  //
  if (device == DEVICE_OZY || device == DEVICE_METIS)
  {
    col++;
    row=1;
    GtkWidget *atlas_label=gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(atlas_label), "<b>ATLAS bus settings:</b>");
    gtk_grid_attach(GTK_GRID(grid),atlas_label,col,row,2,1);
    row++;

    GtkWidget *clock10_label=gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(clock10_label), "<b>10 MHz source:</b>");
    gtk_grid_attach(GTK_GRID(grid),clock10_label,col,row,1,1);


    GtkWidget *ck10mhz_combo=gtk_combo_box_text_new();
    gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(ck10mhz_combo),NULL,"Atlas");
    gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(ck10mhz_combo),NULL,"Penelope");
    gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(ck10mhz_combo),NULL,"Mercury");
    gtk_combo_box_set_active(GTK_COMBO_BOX(ck10mhz_combo),atlas_clock_source_10mhz);
    my_combo_attach(GTK_GRID(grid),ck10mhz_combo,col+1,row,1,1);
    g_signal_connect(ck10mhz_combo,"changed",G_CALLBACK(ck10mhz_cb),NULL);

    row++;

    GtkWidget *clock128_label=gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(clock128_label), "<b>122.88 MHz source:</b>");
    gtk_grid_attach(GTK_GRID(grid),clock128_label,col,row,1,1);

    GtkWidget *ck128mhz_combo=gtk_combo_box_text_new();
    gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(ck128mhz_combo),NULL,"Penelope");
    gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(ck128mhz_combo),NULL,"Mercury");
    gtk_combo_box_set_active(GTK_COMBO_BOX(ck128mhz_combo),atlas_clock_source_128mhz ? 1 : 0);
    my_combo_attach(GTK_GRID(grid),ck128mhz_combo,col+1,row,1,1);
    g_signal_connect(ck128mhz_combo,"changed",G_CALLBACK(ck128mhz_cb),NULL);

    row++;

    GtkWidget *micsource_label=gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(micsource_label), "<b>Mic source:</b>");
    gtk_grid_attach(GTK_GRID(grid),micsource_label,col,row,1,1);

    GtkWidget *micsource_combo=gtk_combo_box_text_new();
    gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(micsource_combo),NULL,"Janus");
    gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(micsource_combo),NULL,"Penelope");
    gtk_combo_box_set_active(GTK_COMBO_BOX(micsource_combo),atlas_mic_source ? 1 : 0);
    my_combo_attach(GTK_GRID(grid),micsource_combo,col+1,row,1,1);
    g_signal_connect(micsource_combo,"changed",G_CALLBACK(micsource_cb),NULL);

    row++;

    GtkWidget *tx_label=gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(tx_label), "<b>TX config:</b>");
    gtk_grid_attach(GTK_GRID(grid),tx_label,col,row,1,1);

    GtkWidget *tx_combo=gtk_combo_box_text_new();
    gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(tx_combo),NULL,"No TX");
    gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(tx_combo),NULL,"Penelope");
    gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(tx_combo),NULL,"Pennylane");
    gtk_combo_box_set_active(GTK_COMBO_BOX(tx_combo),atlas_penelope);
    my_combo_attach(GTK_GRID(grid),tx_combo,col+1,row,1,1);
    g_signal_connect(tx_combo,"changed",G_CALLBACK(tx_cb),NULL);

    row++;

    //
    // This option is for ATLAS systems which *only* have an OZY
    // and a JANUS board (the RF front end then is either SDR-1000 or SoftRock)
    //
    // It is assumed that the SDR-1000 is controlled outside piHPSDR
    //
    if (device == DEVICE_OZY) {
      GtkWidget *janus_b=gtk_check_button_new_with_label("Janus Only");
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (janus_b), atlas_janus);
      gtk_grid_attach(GTK_GRID(grid),janus_b,col+1,row,1,1);
      g_signal_connect(janus_b,"toggled",G_CALLBACK(janus_cb),NULL);
      row++;
    }

    if(row>temp_row) temp_row=row;
  }

  if(device==NEW_DEVICE_ORION || device==NEW_DEVICE_ORION2 ||
     device==DEVICE_ORION || device==DEVICE_ORION2) {

      col++;
      row=1;

      GtkWidget *orion_label=gtk_label_new(NULL);
      gtk_label_set_markup(GTK_LABEL(orion_label), "<b>ORION Mic jack:</b>");
      gtk_grid_attach(GTK_GRID(grid),orion_label,col,row,2,1);
      row++;

      GtkWidget *ptt_ring_b=gtk_radio_button_new_with_label(NULL,"PTT On Ring, Mic and Bias on Tip");
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (ptt_ring_b), mic_ptt_tip_bias_ring==0);
      gtk_grid_attach(GTK_GRID(grid),ptt_ring_b,col,row,2,1);
      g_signal_connect(ptt_ring_b,"toggled",G_CALLBACK(ptt_ring_cb),NULL);
      row++;

      GtkWidget *ptt_tip_b=gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(ptt_ring_b),"PTT On Tip, Mic and Bias on Ring");
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (ptt_tip_b), mic_ptt_tip_bias_ring==1);
      gtk_grid_attach(GTK_GRID(grid),ptt_tip_b,col,row,2,1);
      g_signal_connect(ptt_tip_b,"toggled",G_CALLBACK(ptt_tip_cb),NULL);
      row++;

      GtkWidget *ptt_b=gtk_check_button_new_with_label("PTT Enabled");
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (ptt_b), mic_ptt_enabled);
      gtk_grid_attach(GTK_GRID(grid),ptt_b,col,row,2,1);
      g_signal_connect(ptt_b,"toggled",G_CALLBACK(ptt_cb),NULL);
      row++;

      GtkWidget *bias_b=gtk_check_button_new_with_label("BIAS Enabled");
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (bias_b), mic_bias_enabled);
      gtk_grid_attach(GTK_GRID(grid),bias_b,col,row,2,1);
      g_signal_connect(bias_b,"toggled",G_CALLBACK(bias_cb),NULL);
      row++;

      if(row>temp_row) temp_row=row;
  }

  row=temp_row;
  col=0;
  //
  // Insert small separation between top columns and bottom rows
  //
  GtkWidget *Separator=gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
  gtk_grid_attach(GTK_GRID(grid), Separator, col, row, 5, 1);
  row++;

  GtkWidget *split_b=gtk_check_button_new_with_label("Split");
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (split_b), split);
  gtk_grid_attach(GTK_GRID(grid),split_b,col,row,1,1);
  g_signal_connect(split_b,"toggled",G_CALLBACK(split_cb),NULL);

  col++;

  duplex_b=gtk_check_button_new_with_label("Duplex");
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (duplex_b), duplex);
  gtk_grid_attach(GTK_GRID(grid),duplex_b,col,row,1,1);
  g_signal_connect(duplex_b,"toggled",G_CALLBACK(duplex_cb),NULL);

  col++;

  mute_rx_b=gtk_check_button_new_with_label("Mute RX when TX");
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (mute_rx_b), mute_rx_while_transmitting);
  gtk_grid_attach(GTK_GRID(grid),mute_rx_b,col,row,1,1);
  g_signal_connect(mute_rx_b,"toggled",G_CALLBACK(mute_rx_cb),NULL);

  row++;

  col=0;
  GtkWidget *touchscreen_b=gtk_check_button_new_with_label("TouchScreen");
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (touchscreen_b), optimize_for_touchscreen);
  gtk_grid_attach(GTK_GRID(grid),touchscreen_b,col,row,1,1);
  g_signal_connect(touchscreen_b,"toggled",G_CALLBACK(touchscreen_cb),NULL);

  col++;
  GtkWidget *PA_enable_b=gtk_check_button_new_with_label("PA enable");
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (PA_enable_b), pa_enabled);
  gtk_grid_attach(GTK_GRID(grid),PA_enable_b,col,row,1,1);
  g_signal_connect(PA_enable_b,"toggled",G_CALLBACK(PA_enable_cb),NULL);

  if (device==DEVICE_HERMES_LITE2) {
    col++;
    GtkWidget *hl2audio_b=gtk_check_button_new_with_label("HL2 audio codec");
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(hl2audio_b), hl2_audio_codec);
    gtk_grid_attach(GTK_GRID(grid),hl2audio_b,col,row,1,1);
    g_signal_connect(hl2audio_b,"toggled",G_CALLBACK(hl2audio_cb),NULL);
  }

  if (protocol == SOAPYSDR_PROTOCOL) {
    col++;
    GtkWidget *iqswap_b=gtk_check_button_new_with_label("Swap IQ");
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (iqswap_b), iqswap);
    gtk_grid_attach(GTK_GRID(grid),iqswap_b,col,row,1,1);
    g_signal_connect(iqswap_b,"toggled",G_CALLBACK(iqswap_cb),NULL);
  }

  row++;
  col=0;

  GtkWidget *calibration_label=gtk_label_new(NULL);
  gtk_label_set_markup(GTK_LABEL(calibration_label), "<b>Frequency\nCalibration(Hz):</b>");
  gtk_grid_attach(GTK_GRID(grid),calibration_label,col,row,1,1);
  col++;

  GtkWidget *calibration_b=gtk_spin_button_new_with_range(-10000.0,10000.0,1.0);
  gtk_spin_button_set_value(GTK_SPIN_BUTTON(calibration_b),(double)frequency_calibration);
  gtk_grid_attach(GTK_GRID(grid),calibration_b,col,row,1,1);
  g_signal_connect(calibration_b,"value_changed",G_CALLBACK(calibration_value_changed_cb),NULL);

  //
  // Calibration of the RF front end
  //
  col++;
  GtkWidget *rx_gain_calib_label=gtk_label_new(NULL);
  gtk_label_set_markup(GTK_LABEL(rx_gain_calib_label), "<b>RX Gain Calibration:</b>");
  gtk_grid_attach(GTK_GRID(grid),rx_gain_calib_label,col,row,1,1);

  col++;
  GtkWidget *rx_gain_calibration_b=gtk_spin_button_new_with_range(-50.0,50.0,1.0);
  gtk_spin_button_set_value(GTK_SPIN_BUTTON(rx_gain_calibration_b),(double)rx_gain_calibration);
  gtk_grid_attach(GTK_GRID(grid),rx_gain_calibration_b,col,row,1,1);
  g_signal_connect(rx_gain_calibration_b,"value_changed",G_CALLBACK(rx_gain_calibration_value_changed_cb),NULL);

  row++;

  col=0;
#ifdef SOAPYSDR
  if(device==SOAPYSDR_USB_DEVICE) {
    int i;
    if(strcmp(radio->name,"sdrplay")==0 || strcmp(radio->name,"rtlsdr")==0) {
      // We display fixed gains below
      if(radio->info.soapy.rx_gains>0) {
        GtkWidget *rx_gain=gtk_label_new(NULL);
        gtk_label_set_markup(GTK_LABEL(rx_gain), "<b>RX Gains:</b>");
        gtk_label_set_justify(GTK_LABEL(rx_gain),GTK_JUSTIFY_LEFT);
        gtk_grid_attach(GTK_GRID(grid),rx_gain,col,row,1,1);
      }
    }

    if(can_transmit) {
      /*
      if(radio->info.soapy.tx_gains>0) {
        col=2;
        GtkWidget *tx_gain=gtk_label_new(NULL);
        gtk_label_set_markup(GTK_LABEL(tx_gain), "<b>TX Gains:</b>");
        gtk_grid_attach(GTK_GRID(grid),tx_gain,col,row,1,1);
      }
      */
    }

    row++;
    temp_row=row;
    col=0;
    if(strcmp(radio->name,"sdrplay")==0 || strcmp(radio->name,"rtlsdr")==0) {
      for(i=0;i<radio->info.soapy.rx_gains;i++) {
        col=0;
        GtkWidget *rx_gain_label=gtk_label_new(radio->info.soapy.rx_gain[i]);
        gtk_grid_attach(GTK_GRID(grid),rx_gain_label,col,row,1,1);
        col++;
        SoapySDRRange range=radio->info.soapy.rx_range[i];
        if(range.step==0.0) {
          range.step=1.0;
        }
        rx_gains[i]=gtk_spin_button_new_with_range(range.minimum,range.maximum,range.step);
        gtk_widget_set_name (rx_gains[i], radio->info.soapy.rx_gain[i]);
        int value=soapy_protocol_get_gain_element(active_receiver,radio->info.soapy.rx_gain[i]);
        gtk_spin_button_set_value(GTK_SPIN_BUTTON(rx_gains[i]),(double)value);
        gtk_grid_attach(GTK_GRID(grid),rx_gains[i],col,row,1,1);
        g_signal_connect(rx_gains[i],"value_changed",G_CALLBACK(rx_gain_value_changed_cb),&adc[0]);

        row++;
      }
    } else {
      // used single gain control - LimeSDR works out best setting for the 3 rx gains
      col=0;
      GtkWidget *rf_gain_label=gtk_label_new(NULL);
      gtk_label_set_markup(GTK_LABEL(rf_gain_label), "<b>RF Gain</b>");
      gtk_grid_attach(GTK_GRID(grid),rf_gain_label,col,row,1,1);
      col++;
      double max=100;
      if(strcmp(radio->name,"lime")==0) {
        max=60.0;
      } else if(strcmp(radio->name,"plutosdr")==0) {
        max=73.0;
      }
      GtkWidget *rf_gain_b=gtk_spin_button_new_with_range(0.0,max,1.0);
      gtk_spin_button_set_value(GTK_SPIN_BUTTON(rf_gain_b),adc[active_receiver->id].gain);
      gtk_grid_attach(GTK_GRID(grid),rf_gain_b,col,row,1,1);
      g_signal_connect(rf_gain_b,"value_changed",G_CALLBACK(rf_gain_value_changed_cb),&adc[0]);

      row++;
    }

    if(radio->info.soapy.rx_has_automatic_gain) {
      GtkWidget *agc=gtk_check_button_new_with_label("Hardware AGC: ");
      gtk_grid_attach(GTK_GRID(grid),agc,col,row,1,1);
      gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(agc),adc[0].agc);
      g_signal_connect(agc,"toggled",G_CALLBACK(agc_changed_cb),&adc[0]);
    }

    row=temp_row;

    if(can_transmit) {
#if 0
      for(i=0;i<radio->info.soapy.tx_gains;i++) {
        col=2;
        GtkWidget *tx_gain_label=gtk_label_new(radio->info.soapy.tx_gain[i]);
        gtk_grid_attach(GTK_GRID(grid),tx_gain_label,col,row,1,1);
        col++;
        SoapySDRRange range=radio->info.soapy.tx_range[i];
        if(range.step==0.0) {
          range.step=1.0;
        }
        tx_gains[i]=gtk_spin_button_new_with_range(range.minimum,range.maximum,range.step);
        gtk_widget_set_name (tx_gains[i], radio->info.soapy.tx_gain[i]);
        int value=soapy_protocol_get_tx_gain_element(transmitter,radio->info.soapy.tx_gain[i]);
        gtk_spin_button_set_value(GTK_SPIN_BUTTON(tx_gains[i]),(double)value);
        gtk_grid_attach(GTK_GRID(grid),tx_gains[i],col,row,1,1);
        g_signal_connect(tx_gains[i],"value_changed",G_CALLBACK(tx_gain_value_changed_cb),&dac[0]);

        gtk_widget_set_sensitive(tx_gains[i], FALSE);

        row++;
      }
#endif
      // used single gain control - LimeSDR works out best setting for the 3 rx gains
      col=2;
      GtkWidget *tx_gain_label=gtk_label_new(NULL);
      gtk_label_set_markup(GTK_LABEL(tx_gain_label), "<b>TX Gain</b>");
      gtk_grid_attach(GTK_GRID(grid),tx_gain_label,col,row,1,1);
      col++;
      double max=100;
      if(strcmp(radio->name,"lime")==0) {
        max=64.0;
      } else if(strcmp(radio->name,"plutosdr")==0) {
        max=89.0;
      }
      tx_gain=gtk_spin_button_new_with_range(0.0,max,1.0);
      gtk_spin_button_set_value(GTK_SPIN_BUTTON(tx_gain),transmitter->drive);
      gtk_grid_attach(GTK_GRID(grid),tx_gain,col,row,1,1);
      g_signal_connect(tx_gain,"value_changed",G_CALLBACK(drive_gain_value_changed_cb),&adc[0]);
    }
  }
#endif

  gtk_container_add(GTK_CONTAINER(content),grid);

  sub_menu=dialog;

  gtk_widget_show_all(dialog);

}
