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
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include <wdsp.h>

#include "agc.h"
#include "audio.h"
#include "band.h"
#include "bandstack.h"
#include "channel.h"
#include "discovered.h"
#include "filter.h"
#include "main.h"
#include "meter.h"
#include "mode.h"
#include "property.h"
#include "radio.h"
#include "receiver.h"
#include "transmitter.h"
#include "vfo.h"
#include "meter.h"
#include "rx_panadapter.h"
#include "zoompan.h"
#include "sliders.h"
#include "waterfall.h"
#include "new_protocol.h"
#include "old_protocol.h"
#ifdef SOAPYSDR
#include "soapy_protocol.h"
#endif
#include "ext.h"
#include "new_menu.h"
#ifdef CLIENT_SERVER
#include "client_server.h"
#endif


#define min(x,y) (x<y?x:y)
#define max(x,y) (x<y?y:x)

static gint last_x;
static gboolean has_moved=FALSE;
static gboolean pressed=FALSE;
static gboolean making_active=FALSE;

#ifdef AUDIO_WATERFALL
static int waterfall_samples=0;
static int waterfall_resample=6;
#endif

void receiver_weak_notify(gpointer data,GObject  *obj) {
  RECEIVER *rx=(RECEIVER *)data;
  g_print("%s: id=%d obj=%p\n",__FUNCTION__,rx->id, obj);
}

gboolean receiver_button_press_event(GtkWidget *widget, GdkEventButton *event, gpointer data) {
  RECEIVER *rx=(RECEIVER *)data;
  //
  // TEMPORARY FIX:
  //
  // There seems to be a bug in GTK (June 2023).
  // If clicking into a sub-menu window that does not have the focus
  // (especially when clicking on the title bar), it does not get focus
  // immediatedly and we end up HERE with a button-press event, without
  // receiving the following button-release event.
  // As a consequence, the VFO starts to move with motions of the non-pressed mouse.
  // Therefore, as long as this behaviour is observed, we must ingnore
  // button-press-events if there is a submenu window open.
  //
  // This means that with an open menu, you cannot "drag" the receiver. So check
  // from time to time if this is still necessary.
  //
  if (sub_menu || main_menu) return TRUE;

  if(rx==active_receiver) {
    if (event->button == 1) {
      last_x=(int)event->x;
      has_moved=FALSE;
      pressed=TRUE;
    } else if(event->button==3) {
      g_idle_add(ext_start_rx,NULL);
    }
  } else {
    making_active=TRUE;
  }
  return TRUE;
}

void receiver_set_active(RECEIVER *rx) {
  //
  // Abort any frequency entering in the current receiver
  //
  num_pad(-1, active_receiver->id);
  //
  // Make rx the new active receiver
  //
  active_receiver=rx;
  g_idle_add(menu_active_receiver_changed,NULL);
  g_idle_add(ext_vfo_update,NULL);
  g_idle_add(zoompan_active_receiver_changed,NULL);
  g_idle_add(sliders_active_receiver_changed,NULL);
  //
  // Changing the active receiver flips the TX vfo
  //
  tx_vfo_changed();
  set_alex_antennas();
}

gboolean receiver_button_release_event(GtkWidget *widget, GdkEventButton *event, gpointer data) {
  RECEIVER *rx=(RECEIVER *)data;
  if(making_active) {
    making_active=FALSE;
#ifdef CLIENT_SERVER
    if(radio_is_remote) {
      send_rx_select(client_socket,rx->id);
    } else {
#endif
      receiver_set_active(rx);
      if(event->button==3) {
        g_idle_add(ext_start_rx,NULL);
      }
#ifdef CLIENT_SERVER
   }
#endif
  } else {
    if(pressed) {
      int x=(int)event->x;
      if (event->button == 1) {
        if(has_moved) {
          // drag
          vfo_move((long long)((float)(x-last_x)*rx->hz_per_pixel),TRUE);
        } else {
          // move to this frequency
          vfo_move_to((long long)((float)x*rx->hz_per_pixel));
        }
        last_x=x;
        pressed=FALSE;
      }
    }
  }
  return TRUE;
}

gboolean receiver_motion_notify_event(GtkWidget *widget, GdkEventMotion *event, gpointer data) {
  int x, y;
  GdkModifierType state;
  RECEIVER *rx=(RECEIVER *)data;
  //
  // if !pressed, we may come from the destruction
  // of a menu, and should not move the VFO.
  //
  if (!making_active && pressed) {
    gdk_window_get_device_position (event->window,
                                    event->device,
                                    &x,
                                    &y,
                                    &state);
    //
    // Sometimes it turned out to be difficult to "jump" to a
    // new frequency by just clicking in the panadaper. Futher analysis
    // showed that there were "moves" with zero offset arriving between
    // pressing and releasing the mouse button.
    // Accepting such a "move" between a  "press" and the next "release" event
    // sets "has_moved" and results in a "VFO drag" instead of a "VFO set".
    //
    // So we do the following:
    // - "moves" with zero offset are always ignored
    // - the first "move" to be accepted after a "press" must lead us
    //   at least 2 pixels away from the original position.
    //
    int moved=x-last_x;
    if (moved) {
      if (has_moved || moved < -1 || moved > 1) {
        vfo_move((long long)((float)moved*rx->hz_per_pixel),FALSE);
        last_x=x;
        has_moved=TRUE;
      }
    }
  }

  return TRUE;
}

gboolean receiver_scroll_event(GtkWidget *widget, GdkEventScroll *event, gpointer data) {
  if(event->direction==GDK_SCROLL_UP) {
    vfo_step(1);
  } else {
    vfo_step(-1);
  }
  return TRUE;
}

void receiver_save_state(RECEIVER *rx) {
  char name[128];
  char value[128];

  g_print("%s: %d\n",__FUNCTION__,rx->id);
  sprintf(name,"receiver.%d.audio_channel",rx->id);
  sprintf(value,"%d",rx->audio_channel);
  setProperty(name,value);
  sprintf(name,"receiver.%d.local_audio",rx->id);
  sprintf(value,"%d",rx->local_audio);
  setProperty(name,value);
  if(rx->audio_name!=NULL) {
    sprintf(name,"receiver.%d.audio_name",rx->id);
    sprintf(value,"%s",rx->audio_name);
    setProperty(name,value);
  }
  sprintf(name,"receiver.%d.mute_when_not_active",rx->id);
  sprintf(value,"%d",rx->mute_when_not_active);
  setProperty(name,value);
  sprintf(name,"receiver.%d.audio_device",rx->id);
  sprintf(value,"%d",rx->audio_device);
  setProperty(name,value);
  sprintf(name,"receiver.%d.mute_radio",rx->id);
  sprintf(value,"%d",rx->mute_radio);
  setProperty(name,value);

#ifdef CLIENT_SERVER
  if(!radio_is_remote) {
#endif
    sprintf(name,"receiver.%d.alex_antenna",rx->id);
    sprintf(value,"%d",rx->alex_antenna);
    setProperty(name,value);

    //
    // for PS_RX_RECEIVER, *only* save the ALEX antenna setting
    // and then return quickly.
    //
    if (rx->id == PS_RX_FEEDBACK) return;

    sprintf(name,"receiver.%d.sample_rate",rx->id);
    sprintf(value,"%d",rx->sample_rate);
    setProperty(name,value);
    sprintf(name,"receiver.%d.adc",rx->id);
    sprintf(value,"%d",rx->adc);
    setProperty(name,value);
    sprintf(name,"receiver.%d.filter_low",rx->id);
    sprintf(value,"%d",rx->filter_low);
    setProperty(name,value);
    sprintf(name,"receiver.%d.filter_high",rx->id);
    sprintf(value,"%d",rx->filter_high);
    setProperty(name,value);
    sprintf(name,"receiver.%d.fps",rx->id);
    sprintf(value,"%d",rx->fps);
    setProperty(name,value);
    sprintf(name,"receiver.%d.panadapter_low",rx->id);
    sprintf(value,"%d",rx->panadapter_low);
    setProperty(name,value);
    sprintf(name,"receiver.%d.panadapter_high",rx->id);
    sprintf(value,"%d",rx->panadapter_high);
    setProperty(name,value);
    sprintf(name,"receiver.%d.panadapter_step",rx->id);
    sprintf(value,"%d",rx->panadapter_step);
    setProperty(name,value);
    sprintf(name,"receiver.%d.display_waterfall",rx->id);
    sprintf(value,"%d",rx->display_waterfall);
    setProperty(name,value);
    sprintf(name,"receiver.%d.waterfall_low",rx->id);
    sprintf(value,"%d",rx->waterfall_low);
    setProperty(name,value);
    sprintf(name,"receiver.%d.waterfall_high",rx->id);
    sprintf(value,"%d",rx->waterfall_high);
    setProperty(name,value);
    sprintf(name,"receiver.%d.waterfall_automatic",rx->id);
    sprintf(value,"%d",rx->waterfall_automatic);
    setProperty(name,value);

    if (have_alex_att) {
      sprintf(name,"receiver.%d.alex_attenuation",rx->id);
      sprintf(value,"%d",rx->alex_attenuation);
      setProperty(name,value);
    }
    sprintf(name,"receiver.%d.volume",rx->id);
    sprintf(value,"%f",rx->volume);
    setProperty(name,value);
    sprintf(name,"receiver.%d.agc",rx->id);
    sprintf(value,"%d",rx->agc);
    setProperty(name,value);
    sprintf(name,"receiver.%d.agc_gain",rx->id);
    sprintf(value,"%f",rx->agc_gain);
    setProperty(name,value);
    sprintf(name,"receiver.%d.agc_slope",rx->id);
    sprintf(value,"%f",rx->agc_slope);
    setProperty(name,value);
    sprintf(name,"receiver.%d.agc_hang_threshold",rx->id);
    sprintf(value,"%f",rx->agc_hang_threshold);
    setProperty(name,value);

    sprintf(name,"receiver.%d.dither",rx->id);
    sprintf(value,"%d",rx->dither);
    setProperty(name,value);
    sprintf(name,"receiver.%d.random",rx->id);
    sprintf(value,"%d",rx->random);
    setProperty(name,value);
    sprintf(name,"receiver.%d.preamp",rx->id);
    sprintf(value,"%d",rx->preamp);
    setProperty(name,value);
    sprintf(name,"receiver.%d.nb",rx->id);
    sprintf(value,"%d",rx->nb);
    setProperty(name,value);
    sprintf(name,"receiver.%d.nr",rx->id);
    sprintf(value,"%d",rx->nr);
    setProperty(name,value);
    sprintf(name,"receiver.%d.anf",rx->id);
    sprintf(value,"%d",rx->anf);
    setProperty(name,value);
    sprintf(name,"receiver.%d.snb",rx->id);
    sprintf(value,"%d",rx->snb);
    setProperty(name,value);
    sprintf(name,"receiver.%d.nr_agc",rx->id);
    sprintf(value,"%d",rx->nr_agc);
    setProperty(name,value);
    sprintf(name,"receiver.%d.nr2_gain_method",rx->id);
    sprintf(value,"%d",rx->nr2_gain_method);
    setProperty(name,value);
    sprintf(name,"receiver.%d.nr2_npe_method",rx->id);
    sprintf(value,"%d",rx->nr2_npe_method);
    setProperty(name,value);
    sprintf(name,"receiver.%d.nr2_ae",rx->id);
    sprintf(value,"%d",rx->nr2_ae);
    setProperty(name,value);
    sprintf(name,"receiver.%d.nb2_mode",rx->id);
    sprintf(value,"%d",rx->nb2_mode);
    setProperty(name,value);
    sprintf(name,"receiver.%d.nb_tau",rx->id);
    sprintf(value,"%f",rx->nb_tau);
    setProperty(name,value);
    sprintf(name,"receiver.%d.nb_advtime",rx->id);
    sprintf(value,"%f",rx->nb_advtime);
    setProperty(name,value);
    sprintf(name,"receiver.%d.nb_hang",rx->id);
    sprintf(value,"%f",rx->nb_hang);
    setProperty(name,value);
    sprintf(name,"receiver.%d.nb_thresh",rx->id);
    sprintf(value,"%f",rx->nb_thresh);
    setProperty(name,value);
#ifdef EXTNR
    sprintf(name,"receiver.%d.nr4_reduction_amount",rx->id);
    sprintf(value,"%f",rx->nr4_reduction_amount);
    setProperty(name,value);
    sprintf(name,"receiver.%d.nr4_smoothing_factor",rx->id);
    sprintf(value,"%f",rx->nr4_smoothing_factor);
    setProperty(name,value);
    sprintf(name,"receiver.%d.nr4_whitening_factor",rx->id);
    sprintf(value,"%f",rx->nr4_whitening_factor);
    setProperty(name,value);
    sprintf(name,"receiver.%d.nr4_noise_rescale",rx->id);
    sprintf(value,"%f",rx->nr4_noise_rescale);
    setProperty(name,value);
    sprintf(name,"receiver.%d.nr4_post_filter_threshold",rx->id);
    sprintf(value,"%f",rx->nr4_post_filter_threshold);
    setProperty(name,value);
#endif

    sprintf(name,"receiver.%d.low_latency",rx->id);
    sprintf(value,"%d",rx->low_latency);
    setProperty(name,value);

    sprintf(name,"receiver.%d.deviation",rx->id);
    sprintf(value,"%d",rx->deviation);
    setProperty(name,value);

    sprintf(name,"receiver.%d.squelch_enable",rx->id);
    sprintf(value,"%d",rx->squelch_enable);
    setProperty(name,value);
    sprintf(name,"receiver.%d.squelch",rx->id);
    sprintf(value,"%f",rx->squelch);
    setProperty(name,value);

    sprintf(name,"receiver.%d.zoom",rx->id);
    sprintf(value,"%d",rx->zoom);
    setProperty(name,value);
    sprintf(name,"receiver.%d.pan",rx->id);
    sprintf(value,"%d",rx->pan);
    setProperty(name,value);
#ifdef CLIENT_SERVER
  }
#endif
}

void receiver_restore_state(RECEIVER *rx) {
  char name[128];
  char *value;

g_print("%s: id=%d\n",__FUNCTION__,rx->id);

  sprintf(name,"receiver.%d.audio_channel",rx->id);
  value=getProperty(name);
  if(value) rx->audio_channel=atoi(value);
  sprintf(name,"receiver.%d.local_audio",rx->id);
  value=getProperty(name);
  if(value) rx->local_audio=atoi(value);
  sprintf(name,"receiver.%d.audio_name",rx->id);
  value=getProperty(name);
  if(value) {
    rx->audio_name=g_new(gchar,strlen(value)+1);
    strcpy(rx->audio_name,value);
  }
  sprintf(name,"receiver.%d.mute_when_not_active",rx->id);
  value=getProperty(name);
  if(value) rx->mute_when_not_active=atoi(value);
  sprintf(name,"receiver.%d.audio_device",rx->id);
  value=getProperty(name);
  if(value) rx->audio_device=atoi(value);
  sprintf(name,"receiver.%d.mute_radio",rx->id);
  value=getProperty(name);
  if(value) rx->mute_radio=atoi(value);

#ifdef CLIENT_SERVER
  if(!radio_is_remote) {
#endif
    sprintf(name,"receiver.%d.alex_antenna",rx->id);
    value=getProperty(name);
    if(value) rx->alex_antenna=atoi(value);

    //
    // for PS_RX_RECEIVER, *only* restore the ALEX antenna and setting
    // and then return quickly
    //
    if (rx->id == PS_RX_FEEDBACK) return;

    sprintf(name,"receiver.%d.sample_rate",rx->id);
    value=getProperty(name);
    if(value) rx->sample_rate=atoi(value);
    sprintf(name,"receiver.%d.adc",rx->id);
    value=getProperty(name);
    if(value) rx->adc=atoi(value);
    //
    // Do not specify a second ADC if there is only one
    //
    if (n_adc == 1) rx->adc=0;
    sprintf(name,"receiver.%d.filter_low",rx->id);
    value=getProperty(name);
    if(value) rx->filter_low=atoi(value);
    sprintf(name,"receiver.%d.filter_high",rx->id);
    value=getProperty(name);
    if(value) rx->filter_high=atoi(value);
    sprintf(name,"receiver.%d.fps",rx->id);
    value=getProperty(name);
    if(value) rx->fps=atoi(value);
/*
  sprintf(name,"receiver.%d.frequency",rx->id);
  value=getProperty(name);
  if(value) rx->frequency=atoll(value);
  sprintf(name,"receiver.%d.display_frequency",rx->id);
  value=getProperty(name);
  if(value) rx->display_frequency=atoll(value);
  sprintf(name,"receiver.%d.dds_frequency",rx->id);
  value=getProperty(name);
  if(value) rx->dds_frequency=atoll(value);
  sprintf(name,"receiver.%d.dds_offset",rx->id);
  value=getProperty(name);
  if(value) rx->dds_offset=atoll(value);
  sprintf(name,"receiver.%d.rit",rx->id);
  value=getProperty(name);
  if(value) rx->rit=atoi(value);
*/
    sprintf(name,"receiver.%d.panadapter_low",rx->id);
    value=getProperty(name);
    if(value) rx->panadapter_low=atoi(value);
    sprintf(name,"receiver.%d.panadapter_high",rx->id);
    value=getProperty(name);
    if(value) rx->panadapter_high=atoi(value);
    sprintf(name,"receiver.%d.panadapter_step",rx->id);
    value=getProperty(name);
    if(value) rx->panadapter_step=atoi(value);
    sprintf(name,"receiver.%d.display_waterfall",rx->id);
    value=getProperty(name);
    if(value) rx->display_waterfall=atoi(value);
    sprintf(name,"receiver.%d.waterfall_low",rx->id);
    value=getProperty(name);
    if(value) rx->waterfall_low=atoi(value);
    sprintf(name,"receiver.%d.waterfall_high",rx->id);
    value=getProperty(name);
    if(value) rx->waterfall_high=atoi(value);
    sprintf(name,"receiver.%d.waterfall_automatic",rx->id);
    value=getProperty(name);
    if(value) rx->waterfall_automatic=atoi(value);

    if (have_alex_att) {
      sprintf(name,"receiver.%d.alex_attenuation",rx->id);
      value=getProperty(name);
      if(value) rx->alex_attenuation=atoi(value);
    }
    sprintf(name,"receiver.%d.volume",rx->id);
    value=getProperty(name);
    if(value) rx->volume=atof(value);
    sprintf(name,"receiver.%d.agc",rx->id);
    value=getProperty(name);
    if(value) rx->agc=atoi(value);
    sprintf(name,"receiver.%d.agc_gain",rx->id);
    value=getProperty(name);
    if(value) rx->agc_gain=atof(value);
    sprintf(name,"receiver.%d.agc_slope",rx->id);
    value=getProperty(name);
    if(value) rx->agc_slope=atof(value);
    sprintf(name,"receiver.%d.agc_hang_threshold",rx->id);
    value=getProperty(name);
    if(value) rx->agc_hang_threshold=atof(value);

    sprintf(name,"receiver.%d.dither",rx->id);
    value=getProperty(name);
    if(value) rx->dither=atoi(value);
    sprintf(name,"receiver.%d.random",rx->id);
    value=getProperty(name);
    if(value) rx->random=atoi(value);
    sprintf(name,"receiver.%d.preamp",rx->id);
    value=getProperty(name);
    if(value) rx->preamp=atoi(value);
    sprintf(name,"receiver.%d.nb",rx->id);
    value=getProperty(name);
    if(value) rx->nb=atoi(value);
    sprintf(name,"receiver.%d.nr",rx->id);
    value=getProperty(name);
    if(value) rx->nr=atoi(value);
    sprintf(name,"receiver.%d.anf",rx->id);
    value=getProperty(name);
    if(value) rx->anf=atoi(value);
    sprintf(name,"receiver.%d.snb",rx->id);
    value=getProperty(name);
    if(value) rx->snb=atoi(value);
    sprintf(name,"receiver.%d.nr_agc",rx->id);
    value=getProperty(name);
    if(value) rx->nr_agc=atoi(value);
    sprintf(name,"receiver.%d.nr2_gain_method",rx->id);
    value=getProperty(name);
    if(value) rx->nr2_gain_method=atoi(value);
    sprintf(name,"receiver.%d.nr2_npe_method",rx->id);
    value=getProperty(name);
    if(value) rx->nr2_npe_method=atoi(value);
    sprintf(name,"receiver.%d.ae",rx->id);
    value=getProperty(name);
    if(value) rx->nr2_ae=atoi(value);
    sprintf(name,"receiver.%d.nb2_mode",rx->id);
    value=getProperty(name);
    if(value) rx->nb2_mode=atoi(value);
    sprintf(name,"receiver.%d.nb_tau",rx->id);
    value=getProperty(name);
    if(value) rx->nb_tau=atof(value);
    sprintf(name,"receiver.%d.nb_advtime",rx->id);
    value=getProperty(name);
    if(value) rx->nb_advtime=atof(value);
    sprintf(name,"receiver.%d.nb_hang",rx->id);
    value=getProperty(name);
    if(value) rx->nb_hang=atof(value);
    sprintf(name,"receiver.%d.nb_thresh",rx->id);
    value=getProperty(name);
    if(value) rx->nb_thresh=atof(value);
#ifdef EXTNR
    sprintf(name,"receiver.%d.nr4_reduction_amount",rx->id);
    value=getProperty(name);
    if(value) rx->nr4_reduction_amount=atof(value);
    sprintf(name,"receiver.%d.nr4_smoothing_factor",rx->id);
    value=getProperty(name);
    if(value) rx->nr4_smoothing_factor=atof(value);
    sprintf(name,"receiver.%d.nr4_whitening_factor",rx->id);
    value=getProperty(name);
    if(value) rx->nr4_whitening_factor=atof(value);
    sprintf(name,"receiver.%d.nr4_noise_rescale",rx->id);
    value=getProperty(name);
    if(value) rx->nr4_noise_rescale=atof(value);
    sprintf(name,"receiver.%d.nr4_post_filter_threshold",rx->id);
    value=getProperty(name);
    if(value) rx->nr4_post_filter_threshold=atof(value);
#endif

    sprintf(name,"receiver.%d.low_latency",rx->id);
    value=getProperty(name);
    if(value) rx->low_latency=atoi(value);

    sprintf(name,"receiver.%d.deviation",rx->id);
    value=getProperty(name);
    if(value) rx->deviation=atoi(value);

    sprintf(name,"receiver.%d.squelch_enable",rx->id);
    value=getProperty(name);
    if(value) rx->squelch_enable=atoi(value);
    sprintf(name,"receiver.%d.squelch",rx->id);
    value=getProperty(name);
    if(value) rx->squelch=atof(value);

    sprintf(name,"receiver.%d.zoom",rx->id);
    value=getProperty(name);
    if(value) rx->zoom=atoi(value);
    sprintf(name,"receiver.%d.pan",rx->id);
    value=getProperty(name);
    if(value) rx->pan=atoi(value);
#ifdef CLIENT_SERVER
  }
#endif
}

void reconfigure_receiver(RECEIVER *rx,int height) {
  int y=0;
  //
  // myheight is the size of the waterfall or the panadapter
  // which is the full or half of the height depending on whether BOTH
  // are displayed
  //

  g_mutex_lock(&rx->display_mutex);
  int myheight=(rx->display_panadapter && rx->display_waterfall) ? height/2 : height;

  rx->height=height;  // total height

  if(rx->display_panadapter) {
    if(rx->panadapter==NULL) {
g_print("%s: panadapter_init: width:%d height:%d\n",__FUNCTION__,rx->width,myheight);
      rx_panadapter_init(rx, rx->width,myheight);
      gtk_fixed_put(GTK_FIXED(rx->panel),rx->panadapter,0,y);  // y=0 here always
    } else {
       // set the size
      gtk_widget_set_size_request(rx->panadapter, rx->width, myheight);
      // move the current one
      gtk_fixed_move(GTK_FIXED(rx->panel),rx->panadapter,0,y);
    }
    y+=myheight;
  } else {
    if(rx->panadapter!=NULL) {
      gtk_container_remove(GTK_CONTAINER(rx->panel),rx->panadapter);
      rx->panadapter=NULL;
    }
  }

  if(rx->display_waterfall) {
    if(rx->waterfall==NULL) {
g_print("%s: waterfall_init: width:%d height:%d\n",__FUNCTION__,rx->width,myheight);
      waterfall_init(rx,rx->width,myheight);
      gtk_fixed_put(GTK_FIXED(rx->panel),rx->waterfall,0,y);  // y=0 if ONLY waterfall is present
    } else {
      // set the size
g_print("%s: waterfall set_size_request: width:%d height:%d\n",__FUNCTION__,rx->width,myheight);
      gtk_widget_set_size_request(rx->waterfall, rx->width, myheight);
      // move the current one
      gtk_fixed_move(GTK_FIXED(rx->panel),rx->waterfall,0,y);
    }
  } else {
    if(rx->waterfall!=NULL) {
      gtk_container_remove(GTK_CONTAINER(rx->panel),rx->waterfall);
      rx->waterfall=NULL;
    }
  }

  gtk_widget_show_all(rx->panel);
  g_mutex_unlock(&rx->display_mutex);
}

static gint update_display(gpointer data) {
  RECEIVER *rx=(RECEIVER *)data;
  int rc;

  if(rx->displaying) {
    if(rx->pixels>0) {
      g_mutex_lock(&rx->display_mutex);
      GetPixels(rx->id,0,rx->pixel_samples,&rc);
      if(rc) {
        if(rx->display_panadapter) {
          rx_panadapter_update(rx);
        }
        if(rx->display_waterfall) {
          waterfall_update(rx);
        }
      }
      g_mutex_unlock(&rx->display_mutex);
      if(active_receiver==rx) {
        //
        // since rx->meter is used in other places as well (e.g. rigctl),
        // the value obtained from WDSP is best corrected HERE for
        // possible gain and attenuation
        //
        double level=GetRXAMeter(rx->id,smeter);
        level += (double)rx_gain_calibration + (double)adc[rx->adc].attenuation - adc[rx->adc].gain;

        if (filter_board == CHARLY25 && rx->adc == 0) {
          level += (double)(12*rx->alex_attenuation-18*rx->preamp-18*rx->dither);
        }
        if (filter_board == ALEX && rx->adc == 0) {
          level += (double)(10*rx->alex_attenuation);
        }
        rx->meter=level;

        meter_update(rx,SMETER,rx->meter,0.0,0.0,0.0,0.0);
      }
      return TRUE;
    }
  }
  return FALSE;
}

void receiver_remote_update_display(RECEIVER *rx) {
  if(rx->displaying) {
    if(rx->pixels>0) {
      g_mutex_lock(&rx->display_mutex);
      if(rx->display_panadapter) {
        rx_panadapter_update(rx);
      }
      if(rx->display_waterfall) {
        waterfall_update(rx);
      }
      if(active_receiver==rx) {
        meter_update(rx,SMETER,rx->meter,0.0,0.0,0.0,0.0);
      }
      g_mutex_unlock(&rx->display_mutex);
    }
  }
}

void set_displaying(RECEIVER *rx,int state) {
  rx->displaying=state;
#ifdef CLIENT_SERVER
  if(!radio_is_remote) {
#endif
    if(state) {
      if(rx->update_timer_id>=0) g_source_remove(rx->update_timer_id);
      rx->update_timer_id=gdk_threads_add_timeout_full(G_PRIORITY_HIGH_IDLE,1000/rx->fps, update_display, rx, NULL);
    } else {
      rx->update_timer_id=-1;
    }
#ifdef CLIENT_SERVER
  }
#endif
}

void set_mode(RECEIVER *rx,int m) {
  vfo[rx->id].mode=m;
  SetRXAMode(rx->id, vfo[rx->id].mode);
  //
  // The choice of the squelch method depends on the mode
  //
  setSquelch(rx);
}

void set_filter(RECEIVER *rx) {
  int m=vfo[rx->id].mode;
  FILTER *mode_filters=filters[m];
  FILTER *filter=&mode_filters[vfo[rx->id].filter]; // ignored in FMN

  if ((m == modeCWU || m == modeCWL) && cw_audio_peak_filter) {
    //
    // Possibly engage cw peak filter. Use a fixed gain
    //
    SetRXASPCWFreq(rx->id, (double) cw_keyer_sidetone_frequency);
    SetRXASPCWBandwidth(rx->id, (double) cw_audio_peak_width);
    SetRXASPCWGain(rx->id, 1.50);
    SetRXASPCWRun(rx->id, 1);
  } else {
    SetRXASPCWRun(rx->id, 0);
  }

  switch (m) {
    case modeCWL:
      //
      // translate CW filter edges to the CW pitch frequency
      //
      rx->filter_low=-cw_keyer_sidetone_frequency+filter->low;
      rx->filter_high=-cw_keyer_sidetone_frequency+filter->high;
      break;
    case modeCWU:
      rx->filter_low=cw_keyer_sidetone_frequency+filter->low;
      rx->filter_high=cw_keyer_sidetone_frequency+filter->high;
      break;
    case  modeFMN:
      //
      // FM filter settings are ignored, instead, the filter
      // size is calculated from the deviation
      //
      if(rx->deviation==2500) {
        rx->filter_low=-5500;
        rx->filter_high=5500;
      } else {
        rx->filter_low=-8000;
        rx->filter_high=8000;
      }
      set_deviation(rx);
      break;
    default:
      rx->filter_low=filter->low;
      rx->filter_high=filter->high;
      break;
  }

  RXASetPassband(rx->id,(double)rx->filter_low,(double)rx->filter_high);
  //
  // The AGC line position on the panadapter depends on the filter width,
  // therefore we need to re-calculate. In order to avoid code duplication,
  // we invoke set_agc
  //
  set_agc(rx, rx->agc);
}

void set_deviation(RECEIVER *rx) {
  SetRXAFMDeviation(rx->id, (double)rx->deviation);
}

void set_agc(RECEIVER *rx, int agc) {

  SetRXAAGCMode(rx->id, agc);
  //SetRXAAGCThresh(rx->id, agc_thresh_point, 4096.0, rx->sample_rate);
  SetRXAAGCSlope(rx->id,rx->agc_slope);
  SetRXAAGCTop(rx->id,rx->agc_gain);
  switch(agc) {
    case AGC_OFF:
      break;
    case AGC_LONG:
      SetRXAAGCAttack(rx->id,2);
      SetRXAAGCHang(rx->id,2000);
      SetRXAAGCDecay(rx->id,2000);
      SetRXAAGCHangThreshold(rx->id,(int)rx->agc_hang_threshold);
      break;
    case AGC_SLOW:
      SetRXAAGCAttack(rx->id,2);
      SetRXAAGCHang(rx->id,1000);
      SetRXAAGCDecay(rx->id,500);
      SetRXAAGCHangThreshold(rx->id,(int)rx->agc_hang_threshold);
      break;
    case AGC_MEDIUM:
      SetRXAAGCAttack(rx->id,2);
      SetRXAAGCHang(rx->id,0);
      SetRXAAGCDecay(rx->id,250);
      SetRXAAGCHangThreshold(rx->id,100);
      break;
    case AGC_FAST:
      SetRXAAGCAttack(rx->id,2);
      SetRXAAGCHang(rx->id,0);
      SetRXAAGCDecay(rx->id,50);
      SetRXAAGCHangThreshold(rx->id,100);
      break;
  }
  //
  // Recalculate the "panadapter" AGC line positions.
  //
  GetRXAAGCHangLevel(rx->id, &rx->agc_hang);
  GetRXAAGCThresh(rx->id, &rx->agc_thresh, 4096.0, (double)rx->sample_rate);

}

void set_offset(RECEIVER *rx,long long offset) {
  if(offset==0) {
    SetRXAShiftFreq(rx->id, (double)offset);
    RXANBPSetShiftFrequency(rx->id, (double)offset);
    SetRXAShiftRun(rx->id, 0);
  } else {
    SetRXAShiftFreq(rx->id, (double)offset);
    RXANBPSetShiftFrequency(rx->id, (double)offset);
    SetRXAShiftRun(rx->id, 1);
  }
}

static void init_analyzer(RECEIVER *rx) {
    int flp[] = {0};
    double keep_time = 0.1;
    int n_pixout=1;
    int spur_elimination_ffts = 1;
    int data_type = 1;
    int afft_size = 8192;
    int window_type = 4;
    double kaiser_pi = 14.0;
    int overlap = 2048;
    int clip = 0;
    double span_clip_l = 0;
    double span_clip_h = 0;
    int pixels=rx->pixels;
    int stitches = 1;
    int calibration_data_set = 0;
    double span_min_freq = 0.0;
    double span_max_freq = 0.0;

    int max_w = afft_size + (int) min(keep_time * (double) rx->fps, keep_time * (double) afft_size * (double) rx->fps);

    overlap = (int)fmax(0.0, ceil(afft_size - (double)rx->sample_rate / (double)rx->fps));

    //g_print("%s: id=%d buffer_size=%d overlap=%d\n",_FUNCTION__,rx->id,rx->buffer_size,overlap);


    SetAnalyzer(rx->id,
            n_pixout,
            spur_elimination_ffts, //number of LO frequencies = number of ffts used in elimination
            data_type, //0 for real input data (I only); 1 for complex input data (I & Q)
            flp, //vector with one elt for each LO frequency, 1 if high-side LO, 0 otherwise
            afft_size, //size of the fft, i.e., number of input samples
            rx->buffer_size, //number of samples transferred for each OpenBuffer()/CloseBuffer()
            window_type, //integer specifying which window function to use
            kaiser_pi, //PiAlpha parameter for Kaiser window
            overlap, //number of samples each fft (other than the first) is to re-use from the previous
            clip, //number of fft output bins to be clipped from EACH side of each sub-span
            span_clip_l, //number of bins to clip from low end of entire span
            span_clip_h, //number of bins to clip from high end of entire span
            pixels, //number of pixel values to return.  may be either <= or > number of bins
            stitches, //number of sub-spans to concatenate to form a complete span
            calibration_data_set, //identifier of which set of calibration data to use
            span_min_freq, //frequency at first pixel value8192
            span_max_freq, //frequency at last pixel value
            max_w //max samples to hold in input ring buffers
    );


}

static void create_visual(RECEIVER *rx) {
  int y=0;

  rx->panel=gtk_fixed_new();
g_print("%s: id=%d width=%d height=%d %p\n",__FUNCTION__,rx->id, rx->width, rx->height, rx->panel);
  g_object_weak_ref(G_OBJECT(rx->panel),receiver_weak_notify,(gpointer)rx);
  gtk_widget_set_size_request (rx->panel, rx->width, rx->height);

  rx->panadapter=NULL;
  rx->waterfall=NULL;

  int height=rx->height;
  if(rx->display_waterfall) {
    height=height/2;
  }

  rx_panadapter_init(rx, rx->width,height);
g_print("%s: panadapter height=%d y=%d %p\n",__FUNCTION__,height,y,rx->panadapter);
  g_object_weak_ref(G_OBJECT(rx->panadapter),receiver_weak_notify,(gpointer)rx);
  gtk_fixed_put(GTK_FIXED(rx->panel),rx->panadapter,0,y);
  y+=height;

  if(rx->display_waterfall) {
    waterfall_init(rx,rx->width,height);
g_print("%s: waterfall height=%d y=%d %p\n",__FUNCTION__,height,y,rx->waterfall);
    g_object_weak_ref(G_OBJECT(rx->waterfall),receiver_weak_notify,(gpointer)rx);
    gtk_fixed_put(GTK_FIXED(rx->panel),rx->waterfall,0,y);
  }

  gtk_widget_show_all(rx->panel);
}

RECEIVER *create_pure_signal_receiver(int id, int buffer_size,int sample_rate,int width) {
g_print("%s: id=%d buffer_size=%d\n",__FUNCTION__,id,buffer_size);
  //
  // For a PureSignal receiver, most parameters are just set to zero since
  // they are not used
  //
  RECEIVER *rx=malloc(sizeof(RECEIVER));
  rx->id=id;

  rx->adc=0;

  rx->sample_rate=sample_rate;
  rx->buffer_size=buffer_size;
  rx->fft_size=fft_size;
  rx->pixels=0;
  rx->fps=0;

  rx->width=width;  // save for later use, e.g. when changing the sample rate
  rx->height=0;
  rx->display_panadapter=0;
  rx->display_waterfall=0;

  if (id == PS_RX_FEEDBACK) {
    //
    // need a buffer for pixels for the spectrum of the feedback
    // signal (MON button).
    // NewProtocol: Since we want to display only 24 kHz instead of
    // 192 kHz, we make a spectrum with eight times the pixels and then
    // display only the central part.
    //
    // Also need a mutex, when changing sample rate
    //
    g_mutex_init(&rx->mutex);
    if (protocol == ORIGINAL_PROTOCOL) {
      rx->pixels=(sample_rate/24000) * width;
    } else {
      // sample rate of feedback is TX sample rate is 192000
      rx->pixels = 8*width;
    }
  }

  // need mutex for zoom/pan
  g_mutex_init(&rx->display_mutex);

  // allocate buffers
  rx->iq_input_buffer=g_new(double,2*rx->buffer_size);
  rx->audio_sequence=0L;
  rx->pixel_samples=g_new(float,rx->pixels);

  rx->samples=0;
  rx->displaying=0;
  rx->display_panadapter=0;
  rx->display_waterfall=0;

  rx->panadapter_high=0;
  rx->panadapter_low=0;
  rx->panadapter_step=0;

  rx->volume=0.0;

  rx->squelch_enable=0;
  rx->squelch=0;

  rx->dither=0;
  rx->random=0;
  rx->preamp=0;

  rx->nb=0;
  rx->nr=0;
  rx->anf=0;
  rx->snb=0;

  rx->nr_agc=0;
  rx->nr2_gain_method=0;
  rx->nr2_npe_method=0;
  rx->nr2_ae=0;

  rx->nb_tau = 0.0;
  rx->nb_advtime = 0.0;
  rx->nb_hang = 0.0;
  rx->nb_thresh = 0.0;
  rx->nb2_mode = 0.0;

#ifdef EXTNR
  rx->nr4_reduction_amount = 0.0;
  rx->nr4_smoothing_factor = 0.0;
  rx->nr4_whitening_factor = 0.0;
  rx->nr4_noise_rescale = 0.0;
  rx->nr4_post_filter_threshold = 0.0;
#endif

  rx->alex_antenna=0;
  rx->alex_attenuation=0;

  rx->agc=AGC_MEDIUM;
  rx->agc_gain=0.0;
  rx->agc_slope=0.0;
  rx->agc_hang_threshold=0.0;

  rx->local_audio_buffer=NULL;
  rx->local_audio=0;
  g_mutex_init(&rx->local_audio_mutex);
  rx->audio_name=NULL;
  rx->mute_when_not_active=0;
  rx->audio_channel=STEREO;
  rx->audio_device=-1;
  rx->mute_radio=0;

  rx->low_latency=0;

  rx->pan=0;
  rx->zoom=1;

  // not much to be restored, except alex_antenna and adc
  if (id == PS_RX_FEEDBACK) receiver_restore_state(rx);

  int result;
  XCreateAnalyzer(rx->id, &result, 262144, 1, 1, NULL);
  if(result != 0) {
    g_print( "%s: XCreateAnalyzer id=%d failed: %d\n",__FUNCTION__, rx->id, result);
  } else {
    init_analyzer(rx);
  }

  //
  // This cannot be changed for the PS feedback receiver,
  // use peak mode
  //
  SetDisplayDetectorMode(rx->id, 0, DETECTOR_MODE_PEAK);
  SetDisplayAverageMode(rx->id, 0,  AVERAGE_MODE_NONE);

  return rx;
}

RECEIVER *create_receiver(int id, int buffer_size, int fft_size, int pixels, int fps, int width, int height) {
g_print("%s: id=%d buffer_size=%d fft_size=%d pixels=%d fps=%d\n",__FUNCTION__,id,buffer_size, fft_size, pixels, fps);
  RECEIVER *rx=malloc(sizeof(RECEIVER));
  double amplitude;
  rx->id=id;
  g_mutex_init(&rx->mutex);
  g_mutex_init(&rx->display_mutex);
  switch(id) {
    case 0:
      rx->adc=0;
      break;
    default:
      switch(device) {
        case DEVICE_METIS:
        case DEVICE_OZY:
        case DEVICE_HERMES:
        case DEVICE_HERMES_LITE:
        case DEVICE_HERMES_LITE2:
        case NEW_DEVICE_ATLAS:
        case NEW_DEVICE_HERMES:
          rx->adc=0;
          break;
        default:
          rx->adc=1;
          break;
      }
  }
g_print("%s: id=%d default adc=%d\n",__FUNCTION__,rx->id, rx->adc);
  rx->sample_rate=48000;
  if(device==SOAPYSDR_USB_DEVICE) {
#ifdef SOAPYSDR
    rx->sample_rate=radio->info.soapy.sample_rate;
#endif
    rx->resampler=NULL;
    rx->resample_buffer=NULL;
  }
g_print("%s: id=%d sample_rate=%d\n",__FUNCTION__,rx->id, rx->sample_rate);
  rx->buffer_size=buffer_size;
  rx->fft_size=fft_size;
  rx->fps=fps;
  rx->update_timer_id=-1;

  rx->width=width;
  rx->height=height;

  rx->samples=0;
  rx->displaying=0;
  rx->display_panadapter=1;
  rx->display_waterfall=1;

  rx->panadapter_high=-40;
  rx->panadapter_low=-140;
  rx->panadapter_step=20;

  rx->waterfall_high=-40;
  rx->waterfall_low=-140;
  rx->waterfall_automatic=1;

  rx->volume=-20.0;

  rx->dither=0;
  rx->random=0;
  rx->preamp=0;

  rx->nb=0;
  rx->nr=0;
  rx->anf=0;
  rx->snb=0;

  rx->nr_agc=0;                // NR/NR2/ANF before AGC

  rx->nr2_gain_method=2;       // Gamma
  rx->nr2_npe_method=0;        // OSMS
  rx->nr2_ae=1;                // Artifact Elimination is "on"

  //
  // It has been reported that the piHPSDR noise blankers do not function
  // satisfactorily. I could reproduce this after building an "impulse noise source"
  // into the HPSDR simulator, and also confirmed that a popular Windows SDR program
  // has much better NB/NB2 performance.
  //
  // Digging into it, I found these SDR programs used NB default parameters *very*
  // different from those recommended in the WDSP manual: slewtime, hangtime and advtime
  // default to 0.01 msec, and the threshold to 30 (which is internally multiplied with 0.165
  // to obtain the WDSP threshold parameter).
  //

  rx->nb_tau=     0.00001;        // Slew=0.01    in the DSP menu
  rx->nb_advtime= 0.00001;        // Lead=0.01    in the DSP menu
  rx->nb_hang=    0.00001;        // Lag=0.01     in the DSP menu
  rx->nb_thresh=  4.95;           // Threshold=30 in the DSP menu
  rx->nb2_mode=0;                 // Zero mode

#ifdef EXTNR
  rx->nr4_reduction_amount = 10.0;
  rx->nr4_smoothing_factor = 0.0;
  rx->nr4_whitening_factor = 0.0;
  rx->nr4_noise_rescale = 2.0;
  rx->nr4_post_filter_threshold = -10.0;
#endif

  BAND *b=band_get_band(vfo[rx->id].band);
  rx->alex_antenna=b->alexRxAntenna;
  if (have_alex_att) {
    rx->alex_attenuation=b->alexAttenuation;
  } else {
    rx->alex_attenuation=0;
  }
  rx->agc=AGC_MEDIUM;
  rx->agc_gain=80.0;
  rx->agc_slope=35.0;
  rx->agc_hang_threshold=0.0;

  rx->local_audio=0;
  g_mutex_init(&rx->local_audio_mutex);
  rx->local_audio_buffer=NULL;
  rx->audio_name=NULL;
  rx->mute_when_not_active=0;
  rx->audio_channel=STEREO;
  rx->audio_device=-1;

  rx->low_latency=0;

  rx->squelch_enable=0;
  rx->squelch=0;

  rx->filter_high=525;
  rx->filter_low=275;

  rx->deviation=2500;

  rx->mute_radio=0;

  rx->fexchange_errors=0;

  rx->zoom=1;
  rx->pan=0;

  receiver_restore_state(rx);

  // allocate buffers
  rx->iq_input_buffer=g_new(double,2*rx->buffer_size);
  rx->audio_buffer_size=480;
  rx->audio_sequence=0L;
  rx->pixels=pixels*rx->zoom;
  rx->pixel_samples=g_new(float,rx->pixels);


g_print("%s (after restore): rx=%p id=%d audio_buffer_size=%d local_audio=%d\n",__FUNCTION__,rx,rx->id,rx->audio_buffer_size,rx->local_audio);
  int scale=rx->sample_rate/48000;
  rx->output_samples=rx->buffer_size/scale;
  rx->audio_output_buffer=g_new(gdouble,2*rx->output_samples);

g_print("%s: id=%d output_samples=%d audio_output_buffer=%p\n",__FUNCTION__,rx->id,rx->output_samples,rx->audio_output_buffer);

  rx->hz_per_pixel=(double)rx->sample_rate/(double)rx->pixels;

  // setup wdsp for this receiver

g_print("%s: id=%d after restore adc=%d\n",__FUNCTION__,rx->id, rx->adc);

g_print("%s: OpenChannel id=%d buffer_size=%d fft_size=%d sample_rate=%d\n",
        __FUNCTION__,
        rx->id,
        rx->buffer_size,
        rx->fft_size,
        rx->sample_rate);
  OpenChannel(rx->id,
              rx->buffer_size,
              rx->fft_size,
              rx->sample_rate,
              48000, // dsp rate
              48000, // output rate
              0, // receive
              1, // run
              0.010, 0.025, 0.0, 0.010, 0);

  //
  // NB noise blanker
  //
  create_anbEXT(rx->id,1,rx->buffer_size,rx->sample_rate,0.0001,0.0001,0.0001,0.05,20);
  SetEXTANBTau(rx->id, rx->nb_tau);
  SetEXTANBHangtime(rx->id, rx->nb_hang);
  SetEXTANBAdvtime(rx->id, rx->nb_advtime);
  SetEXTANBThreshold(rx->id, rx->nb_thresh);
  SetEXTANBRun(rx->id, (rx->nb == 1));

  //
  // NB2 noise blanker
  //
  create_nobEXT(rx->id,1,0,rx->buffer_size,rx->sample_rate,0.0001,0.0001,0.0001,0.05,20);
  SetEXTNOBMode(rx->id, rx->nb2_mode);
  SetEXTNOBTau(rx->id, rx->nb_tau);
  SetEXTNOBHangtime(rx->id, rx->nb_hang);
  SetEXTNOBAdvtime(rx->id, rx->nb_advtime);
  SetEXTNOBThreshold(rx->id, rx->nb_thresh);
  SetEXTNOBRun(rx->id, (rx->nb == 2));

  //
  // NR (default values, no GUI)
  //
  SetRXAANRVals(rx->id, 64, 16, 16e-4, 10e-7);
  SetRXAANRPosition(rx->id, rx->nr_agc);
  SetRXAANRRun(rx->id, (rx->nr == 1));

  //
  // NR2
  //
  SetRXAEMNRPosition(rx->id, rx->nr_agc);
  SetRXAEMNRgainMethod(rx->id, rx->nr2_gain_method);
  SetRXAEMNRnpeMethod(rx->id, rx->nr2_npe_method);
  SetRXAEMNRaeRun(rx->id, rx->nr2_ae);
  SetRXAEMNRRun(rx->id, (rx->nr == 2));

  //
  // ANF
  //
  SetRXAANFRun(rx->id, rx->anf);
  SetRXAANFPosition(rx->id, rx->nr_agc);

  //
  // SNB
  //
  SetRXASNBARun(rx->id, rx->snb);

#ifdef EXTNR
  //
  // NR3
  //
  SetRXARNNRRun(rx->id, (rx->nr==3));

  //
  // NR4
  //
  SetRXASBNRreductionAmount(rx->id, rx->nr4_reduction_amount);
  SetRXASBNRsmoothingFactor(rx->id, rx->nr4_smoothing_factor);
  SetRXASBNRwhiteningFactor(rx->id, rx->nr4_whitening_factor);
  SetRXASBNRnoiseRescale(rx->id, rx->nr4_noise_rescale);
  SetRXASBNRpostFilterThreshold(rx->id, rx->nr4_post_filter_threshold);

  SetRXASBNRRun(rx->id, (rx->nr == 4));
#endif

  RXASetNC(rx->id, rx->fft_size);     // length of all RXA filter impulse responses
  RXASetMP(rx->id, rx->low_latency);  // Linear phase or low latency

  SetRXAAMDSBMode(rx->id, 0);         // use both sidebands in SAM
  SetRXAShiftRun(rx->id, 0);          // Frequency shifter OFF


  //
  // Compute audio amplitude from the logarithmic "volume"
  //
  if (rx->volume < -39.5) {
    amplitude = 0.0;
  } else {
    amplitude=pow(10.0, 0.05*rx->volume);
  }
  SetRXAPanelGain1(rx->id, amplitude);
  SetRXAPanelBinaural(rx->id, binaural);
  SetRXAPanelRun(rx->id, 1);

  //
  // enable_rx_equalizer and rx_equalizer should be part of rx
  //
  if(enable_rx_equalizer) {
    SetRXAGrphEQ(rx->id, rx_equalizer);
    SetRXAEQRun(rx->id, 1);
  } else {
    SetRXAEQRun(rx->id, 0);
  }

  receiver_mode_changed(rx);

  int result;
  XCreateAnalyzer(rx->id, &result, 262144, 1, 1, NULL);
  if(result != 0) {
    g_print( "%s: XCreateAnalyzer id=%d failed: %d\n",__FUNCTION__,rx->id, result);
  } else {
    init_analyzer(rx);
  }

  SetDisplayDetectorMode(rx->id, 0, display_detector_mode);
  SetDisplayAverageMode(rx->id, 0,  display_average_mode);

  calculate_display_average(rx);

  create_visual(rx);

g_print("%s: rx=%p id=%d local_audio=%d\n",__FUNCTION__,rx,rx->id,rx->local_audio);
  if(rx->local_audio) {
    if(audio_open_output(rx)<0) {
      rx->local_audio=0;
    }
  }

  // defer set_agc until here, otherwise the AGC threshold is not computed correctly
  set_agc(rx, rx->agc);

  rx->txrxcount=0;
  rx->txrxmax=0;
  return rx;
}

void receiver_change_adc(RECEIVER *rx,int adc) {
  rx->adc=adc;
}

void receiver_change_sample_rate(RECEIVER *rx,int sample_rate) {

  g_mutex_lock(&rx->mutex);

  rx->sample_rate=sample_rate;
  int scale=rx->sample_rate/48000;
  rx->output_samples=rx->buffer_size/scale;
  rx->hz_per_pixel=(double)rx->sample_rate/(double)rx->width;

g_print("%s: id=%d rate=%d scale=%d buffer_size=%d output_samples=%d\n",__FUNCTION__,rx->id,sample_rate,scale,rx->buffer_size,rx->output_samples);

  //
  // In the old protocol, the RX_FEEDBACK sample rate is tied
  // to the radio's sample rate and therefore may vary.
  // Since there is no downstream WDSP receiver her, the only thing
  // we have to do here is to adapt the spectrum display of the
  // feedback and *must* then return (rx->id is not a WDSP channel!)
  //
  if (rx->id == PS_RX_FEEDBACK && protocol == ORIGINAL_PROTOCOL) {
    rx->pixels = 2* scale * rx->width;
    g_free(rx->pixel_samples);
    rx->pixel_samples=g_new(float,rx->pixels);
    init_analyzer(rx);
    g_print("%s: PS FEEDBACK: id=%d rate=%d buffer_size=%d output_samples=%d\n",
            __FUNCTION__,rx->id, rx->sample_rate, rx->buffer_size, rx->output_samples);
    g_mutex_unlock(&rx->mutex);
    return;
  }
  //
  // re-calculate AGC line for panadapter since it depends on sample rate
  //
  GetRXAAGCThresh(rx->id, &rx->agc_thresh, 4096.0, (double)rx->sample_rate);
  if (rx->audio_output_buffer != NULL) {
    g_free(rx->audio_output_buffer);
  }
  rx->audio_output_buffer=g_new(gdouble,2*rx->output_samples);

  SetChannelState(rx->id,0,1);
  init_analyzer(rx);
  SetInputSamplerate(rx->id, sample_rate);
  SetEXTANBSamplerate (rx->id, sample_rate);
  SetEXTNOBSamplerate (rx->id, sample_rate);
#ifdef SOAPYSDR
  if(protocol==SOAPYSDR_PROTOCOL) {
    soapy_protocol_change_sample_rate(rx);
    soapy_protocol_set_mic_sample_rate(rx->sample_rate);
  }
#endif

  SetChannelState(rx->id,1,0);

  //
  // for a non-PS receiver, adjust pixels and hz_per_pixel depending on the zoom value
  //
  rx->pixels=rx->width*rx->zoom;
  rx->hz_per_pixel=(double)rx->sample_rate/(double)rx->pixels;

  g_mutex_unlock(&rx->mutex);

g_print("%s: id=%d rate=%d buffer_size=%d output_samples=%d\n",__FUNCTION__,rx->id, rx->sample_rate, rx->buffer_size, rx->output_samples);
}

void receiver_set_frequency(RECEIVER *rx, long long f) {
  int id=rx->id;

  //
  // update VFO frequency, and let receiver_frequency_changed do the rest
  //
  if(vfo[id].ctun) {
    vfo[id].ctun_frequency=f;
  } else {
    vfo[id].frequency=f;
  }
  receiver_frequency_changed(rx);
}

void receiver_frequency_changed(RECEIVER *rx) {
  int id=rx->id;
  long long f;

  if(vfo[id].ctun) {
    long long frequency=vfo[id].frequency;
    long long half=(long long)rx->sample_rate/2LL;
    long long rx_low=vfo[id].ctun_frequency+rx->filter_low;
    long long rx_high=vfo[id].ctun_frequency+rx->filter_high;
    if (rx_low < frequency - half || rx_high > frequency+half) {
      //
      // Perhaps this is paranoia, but a "legal" VFO might turn
      // into an "illegal" when when reducing the sample rate,
      // thus narrowing the CTUN window.
      // If the "filter window" has left the CTUN range, CTUN is
      // reset such that the CTUN center frequency is placed at
      // the new frequency
      //
      g_print("%s: CTUN freq out of range\n", __FUNCTION__);
      vfo[id].frequency=vfo[id].ctun_frequency;
    }

    if(rx->zoom>1) {
      //
      // Adjust PAN if new filter width has moved out of
      // current display range
      // TODO: what if this happens with CTUN "off"?
      //
      long long min_display=frequency-half+(long long)((double)rx->pan*rx->hz_per_pixel);
      long long max_display=min_display+(long long)((double)rx->width*rx->hz_per_pixel);
      if(rx_low<=min_display) {
        rx->pan=rx->pan-(rx->width/2);
        if(rx->pan<0) rx->pan=0;
        set_pan(id,rx->pan);
      } else if(rx_high>=max_display) {
        rx->pan=rx->pan+(rx->width/2);
        if(rx->pan>(rx->pixels-rx->width)) rx->pan=rx->pixels-rx->width;
        set_pan(id,rx->pan);
      }
    }
    //
    // Compute new offset
    //
    vfo[id].offset=vfo[id].ctun_frequency-vfo[id].frequency;
    if(vfo[id].rit_enabled) {
      vfo[id].offset+=vfo[id].rit;
    }
  } else {
    //
    // This may be part of a CTUN ON->OFF transition
    //
    vfo[id].offset=0;
    if(vfo[id].rit_enabled) {
      vfo[id].offset=vfo[id].rit;
    }
  }

  //
  // Get new band from new frequency
  //
  f = vfo[id].ctun ? vfo[id].ctun_frequency : vfo[id].frequency;
  vfo[id].band=get_band_from_frequency(f);

  //
  // To make this bullet-proof, report the (possibly new) offset to WDSP
  // and send the (possibly changed) frequency to the radio in any case.
  //
  set_offset(rx,vfo[id].offset);
  switch(protocol) {
    case ORIGINAL_PROTOCOL:
      // P1 does this automatically
      break;
    case NEW_PROTOCOL:
      schedule_high_priority(); // send new frequency
      break;
#if SOAPYSDR
    case SOAPYSDR_PROTOCOL:
      soapy_protocol_set_rx_frequency(rx,id);
      break;
#endif
  }
}

void receiver_filter_changed(RECEIVER *rx) {
  set_filter(rx);
  if(can_transmit && transmitter!=NULL) {
    if(transmitter->use_rx_filter && rx==active_receiver) {
      tx_set_filter(transmitter);
    }
  }
  //
  // TODO: Filter window has possibly moved outside CTUN range
  //
}

void receiver_mode_changed(RECEIVER *rx) {
  set_mode(rx,vfo[rx->id].mode);
  receiver_filter_changed(rx);
}

void receiver_vfo_changed(RECEIVER *rx) {
  //
  // Called when the VFO controlling rx has changed,
  // e.g. after a "swap VFO" action
  //
  receiver_frequency_changed(rx);
  receiver_mode_changed(rx);
}

static void process_rx_buffer(RECEIVER *rx) {
  gdouble left_sample,right_sample;
  short left_audio_sample,right_audio_sample;
  int i;

  //g_print("%s: rx=%p id=%d output_samples=%d audio_output_buffer=%p\n",__FUNCTION__,rx,rx->id,rx->output_samples,rx->audio_output_buffer);

  for(i=0;i<rx->output_samples;i++) {
    if(isTransmitting() && (!duplex || mute_rx_while_transmitting)) {
      left_sample=0.0;
      right_sample=0.0;
      left_audio_sample=0;
      right_audio_sample=0;
    } else {
      left_sample=rx->audio_output_buffer[i*2];
      right_sample=rx->audio_output_buffer[(i*2)+1];
      left_audio_sample=(short)(left_sample*32767.0);
      right_audio_sample=(short)(right_sample*32767.0);
    }

    if(rx->local_audio) {
      //
      // I received many comments on the "expected" function of the
      // "Mute audio to radio" checkbox in the RX menu.
      //
      // 1    vote  was :  mute_radio should *only* mute the samples sent to the radio
      // Many votes were: mute_radio should *also* mute the samples sent to local audio
      //
      // So this is now reverted to the original situation, respecting the "majority"
      //
      if((rx!=active_receiver && rx->mute_when_not_active) || rx->mute_radio) {
        left_sample=0.0;
        right_sample=0.0;
      } else {
        switch(rx->audio_channel) {
          case STEREO:
            break;
          case LEFT:
            right_sample=0.0;
            break;
          case RIGHT:
            left_sample=0.0;
            break;
        }
      }
      audio_write(rx,(float)left_sample,(float)right_sample);
    }

#ifdef CLIENT_SERVER
    if(clients!=NULL) {
      remote_audio(rx,left_audio_sample,right_audio_sample);
    }
#endif

    if(rx==active_receiver) {
      switch(protocol) {
        case ORIGINAL_PROTOCOL:
          if(rx->mute_radio) {
            old_protocol_audio_samples(rx,(short)0,(short)0);
          } else {
            old_protocol_audio_samples(rx,left_audio_sample,right_audio_sample);
          }
          break;
        case NEW_PROTOCOL:
          if(!(echo&&isTransmitting())) {
            if(rx->mute_radio) {
              new_protocol_audio_samples(rx,(short)0,(short)0);
            } else {
              new_protocol_audio_samples(rx,left_audio_sample,right_audio_sample);
            }
          }
          break;
        case SOAPYSDR_PROTOCOL:
          break;
      }

#ifdef AUDIO_WATERFALL
      if(audio_samples!=NULL) {
        if(waterfall_samples==0) {
          audio_samples[audio_samples_index]=(float)left_audio_sample;
          audio_samples_index++;
          if(audio_samples_index>=AUDIO_WATERFALL_SAMPLES) {
            //Spectrum(CHANNEL_AUDIO,0,0,audio_samples,audio_samples);
            audio_samples_index=0;
          }
        }
        waterfall_samples++;
        if(waterfall_samples==waterfall_resample) {
          waterfall_samples=0;
        }
      }
#endif

    }

  }
}

void full_rx_buffer(RECEIVER *rx) {
  int error;

  //g_print("%s: rx=%p\n",__FUNCTION__,rx);
  g_mutex_lock(&rx->mutex);

  //
  // noise blanker works on original IQ samples
  //
  switch (rx->nb) {
    case 1:
      xanbEXT (rx->id, rx->iq_input_buffer, rx->iq_input_buffer);
      break;
    case 2:
      xnobEXT (rx->id, rx->iq_input_buffer, rx->iq_input_buffer);
      break;
    default:
      // do nothing
     break;
  }

  fexchange0(rx->id, rx->iq_input_buffer, rx->audio_output_buffer, &error);
  if(error!=0) {
    rx->fexchange_errors++;
  }

  if(rx->displaying) {
    g_mutex_lock(&rx->display_mutex);
    Spectrum0(1, rx->id, 0, 0, rx->iq_input_buffer);
    g_mutex_unlock(&rx->display_mutex);
  }

  process_rx_buffer(rx);
  g_mutex_unlock(&rx->mutex);
}

void add_iq_samples(RECEIVER *rx, double i_sample,double q_sample) {

  //
  // At the end of a TX/RX transition, txrxcount is set to zero,
  // and txrxmax to some suitable value.
  // Then, the first txrxmax RXIQ samples are "silenced"
  // This is necessary on systems where RX feedback samples
  // from cross-talk at the TRX relay arrive with some delay.
  //
  // If txrxmax is zero, no "silencing" takes place here,
  // this is the case for radios not showing this problem,
  // and generally if in CW mode or using duplex.
  //
  if (rx->txrxcount < rx->txrxmax) {
    i_sample=0.0;
    q_sample=0.0;
    rx->txrxcount++;
  }

  rx->iq_input_buffer[rx->samples*2]=i_sample;
  rx->iq_input_buffer[(rx->samples*2)+1]=q_sample;
  rx->samples=rx->samples+1;
  if(rx->samples>=rx->buffer_size) {
    full_rx_buffer(rx);
    rx->samples=0;
  }
}

//
// Note that we sum the second channel onto the first one.
//
void add_div_iq_samples(RECEIVER *rx, double i0, double q0, double i1, double q1) {
  double i_sample=i0 + (div_cos*i1 - div_sin*q1);
  double q_sample=q0 + (div_sin*i1 + div_cos*q1);
  //
  // The rest of the code is copied from add_iq_samples()
  //
  if (rx->txrxcount < rx->txrxmax) {
    i_sample=0.0;
    q_sample=0.0;
    rx->txrxcount++;
  }

  rx->iq_input_buffer[rx->samples*2]=i_sample;
  rx->iq_input_buffer[(rx->samples*2)+1]=q_sample;
  rx->samples=rx->samples+1;
  if(rx->samples>=rx->buffer_size) {
    full_rx_buffer(rx);
    rx->samples=0;
  }
}

void receiver_change_zoom(RECEIVER *rx,double zoom) {
  rx->zoom=(int)zoom;
  rx->pixels=rx->width*rx->zoom;
  rx->hz_per_pixel=(double)rx->sample_rate/(double)rx->pixels;
  if(zoom==1) {
    rx->pan=0;
  } else {
    if(vfo[rx->id].ctun) {
      long long min_frequency=vfo[rx->id].frequency-(long long)(rx->sample_rate/2);
      rx->pan=((vfo[rx->id].ctun_frequency-min_frequency)/rx->hz_per_pixel)-(rx->width/2);
      if(rx->pan<0) rx->pan=0;
      if(rx->pan>(rx->pixels-rx->width)) rx->pan=rx->pixels-rx->width;
    } else {
      rx->pan=(rx->pixels/2)-(rx->width/2);
    }
  }
#ifdef CLIENT_SERVER
  if(!radio_is_remote) {
#endif
    if(rx->pixel_samples!=NULL) {
      g_free(rx->pixel_samples);
    }
    rx->pixel_samples=g_new(float,rx->pixels);
    init_analyzer(rx);
#ifdef CLIENT_SERVER
  }
#endif
}

void receiver_change_pan(RECEIVER *rx,double pan) {
  if(rx->zoom>1) {
    rx->pan=(int)pan;
  }
}

#ifdef CLIENT_SERVER
void receiver_create_remote(RECEIVER *rx) {
  // receiver structure allready setup
  create_visual(rx);
}
#endif
