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
#include <gdk/gdk.h>
#include <math.h>
#include <semaphore.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <net/if_arp.h>
#include <net/if.h>
#include <ifaddrs.h>

#include "appearance.h"
#include "discovered.h"
#include "main.h"
#include "agc.h"
#include "mode.h"
#include "filter.h"
#include "bandstack.h"
#include "band.h"
#include "property.h"
#include "radio.h"
#include "receiver.h"
#include "transmitter.h"
#include "new_protocol.h"
#ifdef SOAPYSDR
#include "soapy_protocol.h"
#endif
#include "vfo.h"
#include "channel.h"
#include "toolbar.h"
#include "wdsp.h"
#include "new_menu.h"
#include "rigctl.h"
#ifdef CLIENT_SERVER
#include "client_server.h"
#endif
#include "ext.h"

static int my_width;
static int my_height;

static GtkWidget *vfo_panel;
static cairo_surface_t *vfo_surface = NULL;

int steps[]={1,10,25,50,100,250,500,1000,5000,9000,10000,100000,250000,500000,1000000};
char *step_labels[]={"1Hz","10Hz","25Hz","50Hz","100Hz","250Hz","500Hz","1kHz","5kHz","9kHz","10kHz","100kHz","250KHz","500KHz","1MHz"};

//
// Move frequency f by n steps, adjust to multiple of step size
// This should replace *all* divisions by the step size
//
#define ROUND(f,n)  (((f+step/2)/step + n)*step)

static GtkWidget* menu=NULL;
static GtkWidget* band_menu=NULL;

struct _vfo vfo[MAX_VFOS];
struct _mode_settings mode_settings[MODES];

static void vfo_save_bandstack() {
  BANDSTACK *bandstack=bandstack_get_bandstack(vfo[0].band);
  BANDSTACK_ENTRY *entry=&bandstack->entry[vfo[0].bandstack];
  entry->frequency=vfo[0].frequency;
  entry->mode=vfo[0].mode;
  entry->filter=vfo[0].filter;
  entry->ctun=vfo[0].ctun;
  entry->ctun_frequency=vfo[0].ctun_frequency;
}

void modesettings_save_state() {
  for (int i=0; i<MODES; i++) {
    char name[80];
    char value[80];
    sprintf(name,"modeset.%d.filter", i);
    sprintf(value,"%d", mode_settings[i].filter);
    setProperty(name,value);
    sprintf(name,"modeset.%d.nr", i);
    sprintf(value,"%d", mode_settings[i].nr);
    setProperty(name,value);
    sprintf(name,"modeset.%d.nr2", i);
    sprintf(value,"%d", mode_settings[i].nr2);
    setProperty(name,value);
    sprintf(name,"modeset.%d.nb", i);
    sprintf(value,"%d", mode_settings[i].nb);
    setProperty(name,value);
    sprintf(name,"modeset.%d.nb2", i);
    sprintf(value,"%d", mode_settings[i].nb2);
    setProperty(name,value);
    sprintf(name,"modeset.%d.anf", i);
    sprintf(value,"%d", mode_settings[i].anf);
    setProperty(name,value);
    sprintf(name,"modeset.%d.snb", i);
    sprintf(value,"%d", mode_settings[i].snb);
    setProperty(name,value);
    sprintf(name,"modeset.%d.en_txeq", i);
    sprintf(value,"%d", mode_settings[i].en_txeq);
    setProperty(name,value);
    sprintf(name,"modeset.%d.txeq.0", i);
    sprintf(value,"%d", mode_settings[i].txeq[0]);
    setProperty(name,value);
    sprintf(name,"modeset.%d.txeq.1", i);
    sprintf(value,"%d", mode_settings[i].txeq[1]);
    setProperty(name,value);
    sprintf(name,"modeset.%d.txeq.2", i);
    sprintf(value,"%d", mode_settings[i].txeq[2]);
    setProperty(name,value);
    sprintf(name,"modeset.%d.txeq.3", i);
    sprintf(value,"%d", mode_settings[i].txeq[3]);
    setProperty(name,value);
    sprintf(name,"modeset.%d.en_rxeq", i);
    sprintf(value,"%d", mode_settings[i].en_rxeq);
    setProperty(name,value);
    sprintf(name,"modeset.%d.rxeq.0", i);
    sprintf(value,"%d", mode_settings[i].rxeq[0]);
    setProperty(name,value);
    sprintf(name,"modeset.%d.rxeq.1", i);
    sprintf(value,"%d", mode_settings[i].rxeq[1]);
    setProperty(name,value);
    sprintf(name,"modeset.%d.rxeq.2", i);
    sprintf(value,"%d", mode_settings[i].rxeq[2]);
    setProperty(name,value);
    sprintf(name,"modeset.%d.rxeq.3", i);
    sprintf(value,"%d", mode_settings[i].rxeq[3]);
    setProperty(name,value);
    sprintf(name,"modeset.%d.step", i);
    sprintf(value,"%lld", mode_settings[i].step);
    setProperty(name,value);
    sprintf(name,"modeset.%d.compressor_level", i);
    sprintf(value,"%f", mode_settings[i].compressor_level);
    setProperty(name,value);
    sprintf(name,"modeset.%d.compressor", i);
    sprintf(value,"%d", mode_settings[i].compressor);
    setProperty(name,value);
  }
}

void modesettings_restore_state() {

  // set some reasonable defaults

  for (int i=0; i<MODES; i++) {
    char *value;
    char name[80];
    mode_settings[i].filter=filterF6;
    mode_settings[i].nr=0;
    mode_settings[i].nr2=0;
    mode_settings[i].nb=0;
    mode_settings[i].nb2=0;
    mode_settings[i].anf=0;
    mode_settings[i].snb=0;
    mode_settings[i].en_txeq=0;
    mode_settings[i].txeq[0]=0;
    mode_settings[i].txeq[1]=0;
    mode_settings[i].txeq[2]=0;
    mode_settings[i].txeq[3]=0;
    mode_settings[i].en_rxeq=0;
    mode_settings[i].rxeq[0]=0;
    mode_settings[i].rxeq[1]=0;
    mode_settings[i].rxeq[2]=0;
    mode_settings[i].rxeq[3]=0;
    mode_settings[i].step=100;
    mode_settings[i].compressor=0;
    mode_settings[i].compressor_level=0.0;

    sprintf(name,"modeset.%d.filter",i);
    value=getProperty(name);
    if(value) mode_settings[i].filter=atoi(value);
    sprintf(name,"modeset.%d.nr",i);
    value=getProperty(name);
    if(value) mode_settings[i].nr=atoi(value);
    sprintf(name,"modeset.%d.nr2",i);
    value=getProperty(name);
    if(value) mode_settings[i].nr2=atoi(value);
    sprintf(name,"modeset.%d.nb",i);
    value=getProperty(name);
    if(value) mode_settings[i].nb=atoi(value);
    sprintf(name,"modeset.%d.nb2",i);
    value=getProperty(name);
    if(value) mode_settings[i].nb2=atoi(value);
    sprintf(name,"modeset.%d.anf",i);
    value=getProperty(name);
    if(value) mode_settings[i].anf=atoi(value);
    sprintf(name,"modeset.%d.snb",i);
    value=getProperty(name);
    if(value) mode_settings[i].snb=atoi(value);
    sprintf(name,"modeset.%d.en_txeq",i);
    value=getProperty(name);
    if(value) mode_settings[i].en_txeq=atoi(value);
    sprintf(name,"modeset.%d.txeq.0",i);
    value=getProperty(name);
    if(value) mode_settings[i].txeq[0]=atoi(value);
    sprintf(name,"modeset.%d.txeq.1",i);
    value=getProperty(name);
    if(value) mode_settings[i].txeq[1]=atoi(value);
    sprintf(name,"modeset.%d.txeq.2",i);
    value=getProperty(name);
    if(value) mode_settings[i].txeq[2]=atoi(value);
    sprintf(name,"modeset.%d.txeq.3",i);
    value=getProperty(name);
    if(value) mode_settings[i].txeq[3]=atoi(value);
    sprintf(name,"modeset.%d.en_rxeq",i);
    value=getProperty(name);
    if(value) mode_settings[i].en_rxeq=atoi(value);
    sprintf(name,"modeset.%d.rxeq.0",i);
    value=getProperty(name);
    if(value) mode_settings[i].rxeq[0]=atoi(value);
    sprintf(name,"modeset.%d.rxeq.1",i);
    value=getProperty(name);
    if(value) mode_settings[i].rxeq[1]=atoi(value);
    sprintf(name,"modeset.%d.rxeq.2",i);
    value=getProperty(name);
    if(value) mode_settings[i].rxeq[2]=atoi(value);
    sprintf(name,"modeset.%d.rxeq.3",i);
    value=getProperty(name);
    if(value) mode_settings[i].rxeq[3]=atoi(value);
    sprintf(name,"modeset.%d.step",i);
    value=getProperty(name);
    if(value) mode_settings[i].step=atoll(value);
    sprintf(name,"modeset.%d.compressor_level",i);
    value=getProperty(name);
    if (value) mode_settings[i].compressor_level=atof(value);
    sprintf(name,"modeset.%d.compressor",i);
    value=getProperty(name);
    if (value) mode_settings[i].compressor=atoi(value);
  }
}

void vfo_save_state() {

  vfo_save_bandstack();

  for(int i=0;i<MAX_VFOS;i++) {
    char name[80];
    char value[80];
    sprintf(name,"vfo.%d.band",i);
    sprintf(value,"%d",vfo[i].band);
    setProperty(name,value);
    sprintf(name,"vfo.%d.frequency",i);
    sprintf(value,"%lld",vfo[i].frequency);
    setProperty(name,value);
    sprintf(name,"vfo.%d.ctun",i);
    sprintf(value,"%d",vfo[i].ctun);
    setProperty(name,value);
    sprintf(name,"vfo.%d.rit_enabled",i);
    sprintf(value,"%d",vfo[i].rit_enabled);
    setProperty(name,value);
    sprintf(name,"vfo.%d.rit",i);
    sprintf(value,"%lld",vfo[i].rit);
    setProperty(name,value);
    sprintf(name,"vfo.%d.lo",i);
    sprintf(value,"%lld",vfo[i].lo);
    setProperty(name,value);
    sprintf(name,"vfo.%d.ctun_frequency",i);
    sprintf(value,"%lld",vfo[i].ctun_frequency);
    setProperty(name,value);
    sprintf(name,"vfo.%d.offset",i);
    sprintf(value,"%lld",vfo[i].offset);
    setProperty(name,value);
    sprintf(name,"vfo.%d.mode",i);
    sprintf(value,"%d",vfo[i].mode);
    setProperty(name,value);
    sprintf(name,"vfo.%d.filter",i);
    sprintf(value,"%d",vfo[i].filter);
    setProperty(name,value);
  }
}

void vfo_restore_state() {

  for(int i=0;i<MAX_VFOS;i++) {
    char name[80];
    char *value;
    vfo[i].band=band20;
    vfo[i].bandstack=0;
    vfo[i].frequency=14010000;
    if(protocol==SOAPYSDR_PROTOCOL) {
      vfo[i].band=band430;
      vfo[i].bandstack=0;
      vfo[i].frequency=434010000;
    }
    vfo[i].mode=modeCWU;
    vfo[i].filter=filterF6;
    vfo[i].lo=0;
    vfo[i].offset=0;
    vfo[i].rit_enabled=0;
    vfo[i].rit=0;
    vfo[i].ctun=0;

    sprintf(name,"vfo.%d.band",i);
    value=getProperty(name);
    if(value) vfo[i].band=atoi(value);
    sprintf(name,"vfo.%d.frequency",i);
    value=getProperty(name);
    if(value) vfo[i].frequency=atoll(value);
    sprintf(name,"vfo.%d.ctun",i);
    value=getProperty(name);
    if(value) vfo[i].ctun=atoi(value);
    sprintf(name,"vfo.%d.ctun_frequency",i);
    value=getProperty(name);
    if(value) vfo[i].ctun_frequency=atoll(value);
    sprintf(name,"vfo.%d.rit",i);
    value=getProperty(name);
    if(value) vfo[i].rit=atoll(value);
    sprintf(name,"vfo.%d.rit_enabled",i);
    value=getProperty(name);
    if(value) vfo[i].rit_enabled=atoi(value);
    sprintf(name,"vfo.%d.lo",i);
    value=getProperty(name);
    if(value) vfo[i].lo=atoll(value);
    sprintf(name,"vfo.%d.offset",i);
    value=getProperty(name);
    if(value) vfo[i].offset=atoll(value);
    sprintf(name,"vfo.%d.mode",i);
    value=getProperty(name);
    if(value) vfo[i].mode=atoi(value);
    sprintf(name,"vfo.%d.filter",i);
    value=getProperty(name);
    if(value) vfo[i].filter=atoi(value);
    // Sanity check: if !ctun, offset must be zero
    if (!vfo[i].ctun) {
    vfo[i].offset=0;
    }
  }
}

void vfo_xvtr_changed() {
  if(vfo[0].band>=BANDS) {
    BAND *band=band_get_band(vfo[0].band);
    vfo[0].lo=band->frequencyLO+band->errorLO;
  }
  if(vfo[1].band>=BANDS) {
    BAND *band=band_get_band(vfo[1].band);
    vfo[1].lo=band->frequencyLO+band->errorLO;
  }
  if (protocol == NEW_PROTOCOL) {
    schedule_general();   // for disablePA
  }
}

void vfo_apply_mode_settings(RECEIVER *rx) {
  int id,m;

  id=rx->id;
  m=vfo[id].mode;

  vfo[id].filter       = mode_settings[m].filter;
  rx->nr               = mode_settings[m].nr;
  rx->nr2              = mode_settings[m].nr2;
  rx->nb               = mode_settings[m].nb;
  rx->nb2              = mode_settings[m].nb2;
  rx->anf              = mode_settings[m].anf;
  rx->snb              = mode_settings[m].snb;
  enable_rx_equalizer  = mode_settings[m].en_rxeq;
  rx_equalizer[0]      = mode_settings[m].rxeq[0];
  rx_equalizer[1]      = mode_settings[m].rxeq[1];
  rx_equalizer[2]      = mode_settings[m].rxeq[2];
  rx_equalizer[3]      = mode_settings[m].rxeq[3];
  step                 = mode_settings[m].step;

  //
  // Transmitter-specific settings are only changed if this VFO
  // controls the TX
  //
  if ((id == get_tx_vfo()) && can_transmit) {
    enable_tx_equalizer  = mode_settings[m].en_txeq;
    tx_equalizer[0]      = mode_settings[m].txeq[0];
    tx_equalizer[1]      = mode_settings[m].txeq[1];
    tx_equalizer[2]      = mode_settings[m].txeq[2];
    tx_equalizer[3]      = mode_settings[m].txeq[3];

    transmitter_set_compressor_level(transmitter, mode_settings[m].compressor_level);
    transmitter_set_compressor      (transmitter, mode_settings[m].compressor      );
  }
  //
  // make changes effective and put them on the VFO display
  //
  g_idle_add(ext_update_noise, NULL);
  g_idle_add(ext_update_eq   , NULL);
  g_idle_add(ext_vfo_update  , NULL);

}

void vfo_band_changed(int id,int b) {
  BANDSTACK *bandstack;

#ifdef CLIENT_SERVER
  if(radio_is_remote) {
    send_band(client_socket,id,b);
    return;
  }
#endif

  if(id==0) {
    vfo_save_bandstack();
  }
  if(b==vfo[id].band) {
    // same band selected - step to the next band stack
    bandstack=bandstack_get_bandstack(b);
    vfo[id].bandstack++;
    if(vfo[id].bandstack>=bandstack->entries) {
      vfo[id].bandstack=0;
    }
  } else {
    // new band - get band stack entry
    bandstack=bandstack_get_bandstack(b);
    vfo[id].bandstack=bandstack->current_entry;
  }

  BAND *band=band_set_current(b);
  BANDSTACK_ENTRY *entry=&bandstack->entry[vfo[id].bandstack];
  vfo[id].band=b;
  vfo[id].frequency=entry->frequency;
  vfo[id].ctun=entry->ctun;
  vfo[id].ctun_frequency=entry->ctun_frequency;
  vfo[id].mode=entry->mode;
  vfo[id].lo=band->frequencyLO+band->errorLO;

  //
  // In the case of CTUN, the offset is re-calculated
  // during receiver_vfo_changed ==> receiver_frequency_changed
  //

  if (id == 0) {
      bandstack->current_entry=vfo[id].bandstack;
  }
  if (id < receivers) {
    vfo_apply_mode_settings(receiver[id]);
    receiver_vfo_changed(receiver[id]);
  }

  tx_vfo_changed();
  set_alex_antennas();  // This includes scheduling hiprio and general packets
#ifdef SOAPYSDR
  //
  // This is strange, since it already done via receiver_vfo_changed()
  // correctly and the present code seems to be wrong if
  // (receivers == 1 && id == 1) or (receivers == 2 && id == 0)
  //
  if (protocol == SOAPYSDR_PROTOCOL) {
    soapy_protocol_set_rx_frequency(active_receiver,id);
  }
#endif
  g_idle_add(ext_vfo_update,NULL);
}

void vfo_bandstack_changed(int b) {
  int id=active_receiver->id;
  if(id==0) {
    vfo_save_bandstack();
  }
  vfo[id].bandstack=b;

  BANDSTACK *bandstack=bandstack_get_bandstack(vfo[id].band);
  BANDSTACK_ENTRY *entry=&bandstack->entry[vfo[id].bandstack];
  vfo[id].frequency=entry->frequency;
  vfo[id].ctun_frequency=entry->ctun_frequency;
  vfo[id].ctun=entry->ctun;
  vfo[id].mode=entry->mode;
  vfo[id].filter=entry->filter;

  if (id == 0) {
    bandstack->current_entry=vfo[id].bandstack;
  }
  if (id < receivers) {
    vfo_apply_mode_settings(receiver[id]);
    receiver_vfo_changed(receiver[id]);
  }

  tx_vfo_changed();
  set_alex_antennas();  // This includes scheduling hiprio and general packets
  g_idle_add(ext_vfo_update,NULL);
}

void vfo_mode_changed(int m) {
  int id=active_receiver->id;
#ifdef CLIENT_SERVER
  if(radio_is_remote) {
    send_mode(client_socket,id,m);
    return;
  }
#endif

  vfo[id].mode=m;

  if (id < receivers) {
    vfo_apply_mode_settings(receiver[id]);
    receiver_mode_changed(receiver[id]);
    receiver_filter_changed(receiver[id]);
  }

  if(can_transmit) {
    tx_set_mode(transmitter,get_tx_mode());
  }
  //
  // changing modes may change BFO frequency
  // and SDR need to be informed about "CW or not CW"
  //
  if (protocol == NEW_PROTOCOL) {
    schedule_high_priority();       // update frequencies
    schedule_transmit_specific();   // update "CW" flag
  }
  g_idle_add(ext_vfo_update,NULL);
}

void vfo_filter_changed(int f) {
  int id=active_receiver->id;
#ifdef CLIENT_SERVER
  if(radio_is_remote) {
    send_filter(client_socket,id,f);
    return;
  }
#endif

  // store changed filter in the mode settings
  mode_settings[vfo[id].mode].filter = f;

  vfo[id].filter=f;
  if (id < receivers) {
    receiver_filter_changed(receiver[id]);
  }

  g_idle_add(ext_vfo_update,NULL);
}

void vfo_a_to_b() {
  vfo[VFO_B].band=vfo[VFO_A].band;
  vfo[VFO_B].bandstack=vfo[VFO_A].bandstack;
  vfo[VFO_B].frequency=vfo[VFO_A].frequency;
  vfo[VFO_B].mode=vfo[VFO_A].mode;
  vfo[VFO_B].filter=vfo[VFO_A].filter;
  vfo[VFO_B].ctun=vfo[VFO_A].ctun;
  vfo[VFO_B].ctun_frequency=vfo[VFO_A].ctun_frequency;
  vfo[VFO_B].rit_enabled=vfo[VFO_A].rit_enabled;
  vfo[VFO_B].rit=vfo[VFO_A].rit;
  vfo[VFO_B].lo=vfo[VFO_A].lo;
  vfo[VFO_B].offset=vfo[VFO_A].offset;

  if(receivers==2) {
    receiver_vfo_changed(receiver[1]);
  }
  tx_vfo_changed();
  set_alex_antennas();  // This includes scheduling hiprio and general packets
  g_idle_add(ext_vfo_update,NULL);
}

void vfo_b_to_a() {
  vfo[VFO_A].band=vfo[VFO_B].band;
  vfo[VFO_A].bandstack=vfo[VFO_B].bandstack;
  vfo[VFO_A].frequency=vfo[VFO_B].frequency;
  vfo[VFO_A].mode=vfo[VFO_B].mode;
  vfo[VFO_A].filter=vfo[VFO_B].filter;
  vfo[VFO_A].ctun=vfo[VFO_B].ctun;
  vfo[VFO_A].ctun_frequency=vfo[VFO_B].ctun_frequency;
  vfo[VFO_A].rit_enabled=vfo[VFO_B].rit_enabled;
  vfo[VFO_A].rit=vfo[VFO_B].rit;
  vfo[VFO_A].lo=vfo[VFO_B].lo;
  vfo[VFO_A].offset=vfo[VFO_B].offset;

  receiver_vfo_changed(receiver[0]);
  tx_vfo_changed();
  set_alex_antennas();  // This includes scheduling hiprio and general packets
  g_idle_add(ext_vfo_update,NULL);
}

void vfo_a_swap_b() {
  int temp_band;
  int temp_bandstack;
  long long temp_frequency;
  int temp_mode;
  int temp_filter;
  int temp_ctun;
  long long temp_ctun_frequency;
  int temp_rit_enabled;
  long long temp_rit;
  long long temp_lo;
  long long temp_offset;

  temp_band=vfo[VFO_A].band;
  temp_bandstack=vfo[VFO_A].bandstack;
  temp_frequency=vfo[VFO_A].frequency;
  temp_mode=vfo[VFO_A].mode;
  temp_filter=vfo[VFO_A].filter;
  temp_ctun=vfo[VFO_A].ctun;
  temp_ctun_frequency=vfo[VFO_A].ctun_frequency;
  temp_rit_enabled=vfo[VFO_A].rit_enabled;
  temp_rit=vfo[VFO_A].rit;
  temp_lo=vfo[VFO_A].lo;
  temp_offset=vfo[VFO_A].offset;

  vfo[VFO_A].band=vfo[VFO_B].band;
  vfo[VFO_A].bandstack=vfo[VFO_B].bandstack;
  vfo[VFO_A].frequency=vfo[VFO_B].frequency;
  vfo[VFO_A].mode=vfo[VFO_B].mode;
  vfo[VFO_A].filter=vfo[VFO_B].filter;
  vfo[VFO_A].ctun=vfo[VFO_B].ctun;
  vfo[VFO_A].ctun_frequency=vfo[VFO_B].ctun_frequency;
  vfo[VFO_A].rit_enabled=vfo[VFO_B].rit_enabled;
  vfo[VFO_A].rit=vfo[VFO_B].rit;
  vfo[VFO_A].lo=vfo[VFO_B].lo;
  vfo[VFO_A].offset=vfo[VFO_B].offset;

  vfo[VFO_B].band=temp_band;
  vfo[VFO_B].bandstack=temp_bandstack;
  vfo[VFO_B].frequency=temp_frequency;
  vfo[VFO_B].mode=temp_mode;
  vfo[VFO_B].filter=temp_filter;
  vfo[VFO_B].ctun=temp_ctun;
  vfo[VFO_B].ctun_frequency=temp_ctun_frequency;
  vfo[VFO_B].rit_enabled=temp_rit_enabled;
  vfo[VFO_B].rit=temp_rit;
  vfo[VFO_B].lo=temp_lo;
  vfo[VFO_B].offset=temp_offset;

  receiver_vfo_changed(receiver[0]);
  if(receivers==2) {
    receiver_vfo_changed(receiver[1]);
  }
  tx_vfo_changed();
  set_alex_antennas();  // This includes scheduling hiprio and general packets
  g_idle_add(ext_vfo_update,NULL);
}

//
// here we collect various functions to
// get/set the VFO step size
//

int vfo_get_step_from_index(int index) {
  //
  // This function is used for some
  // extended CAT commands
  //
  if (index < 0) index=0;
  if (index >= STEPS) index=STEPS-1;
  return steps[index];
}

int vfo_get_stepindex() {
  //
  // return index of current step size in steps[] array
  //
  int i;
  for(i=0;i<STEPS;i++) {
    if(steps[i]==step) break;
  }
  //
  // If step size is not found (this should not happen)
  // report some "convenient" index at the small end
  // (here: index 4 corresponding to 100 Hz)
  //
  if (i >= STEPS) i=4;
  return i;
}

void vfo_set_step_from_index(int index) {
  //
  // Set VFO step size to steps[index], with range checking
  //
  if (index < 0)      index=0;
  if (index >= STEPS) index = STEPS-1;
  vfo_set_stepsize(steps[index]);
}

void vfo_set_stepsize(int newstep) {
  //
  // Set current VFO step size.
  // and store the value in mode_settings of the current mode
  //
  int id=active_receiver->id;
  int m=vfo[id].mode;

  step=newstep;
  mode_settings[m].step=newstep;
}

void vfo_step(int steps) {
  int id=active_receiver->id;
  long long delta;
  int sid;
  RECEIVER *other_receiver;

#ifdef CLIENT_SERVER
  if(radio_is_remote) {
    update_vfo_step(id,steps);
    return;
  }
#endif

  if(!locked) {

    if(vfo[id].ctun) {
      // don't let ctun go beyond end of passband
      long long frequency=vfo[id].frequency;
      long long rx_low =ROUND(vfo[id].ctun_frequency,steps)+active_receiver->filter_low;
      long long rx_high=ROUND(vfo[id].ctun_frequency,steps)+active_receiver->filter_high;
      long long half=(long long)active_receiver->sample_rate/2LL;
      long long min_freq=frequency-half;
      long long max_freq=frequency+half;

      if(rx_low<=min_freq) {
        return;
      } else if(rx_high>=max_freq) {
        return;
      }

      delta=vfo[id].ctun_frequency;
      vfo[id].ctun_frequency=ROUND(vfo[id].ctun_frequency,steps);
      delta=vfo[id].ctun_frequency - delta;
    } else {
      delta=vfo[id].frequency;
      vfo[id].frequency=ROUND(vfo[id].frequency, steps);
      delta = vfo[id].frequency - delta;
    }

    sid=id==0?1:0;
    if (sid < receivers) {
      other_receiver=receiver[sid];
    }
    // other_receiver will be accessed only if receivers == 2

    switch(sat_mode) {
      case SAT_NONE:
        break;
      case SAT_MODE:
        // A and B increment and decrement together
        if (vfo[sid].ctun) {
          vfo[sid].ctun_frequency += delta;
        } else {
          vfo[sid].frequency      += delta;
        }
        if(receivers==2) {
          receiver_frequency_changed(other_receiver);
        }
        break;
      case RSAT_MODE:
        // A increments and B decrements or A decrments and B increments
        if (vfo[sid].ctun) {
          vfo[sid].ctun_frequency -= delta;
        } else {
          vfo[sid].frequency      -= delta;
        }
        if(receivers==2) {
          receiver_frequency_changed(other_receiver);
        }
        break;
    }
    receiver_frequency_changed(active_receiver);
    g_idle_add(ext_vfo_update,NULL);
  }
}
//
// DL1YCF: essentially a duplicate of vfo_step but
//         changing a specific VFO freq instead of
//         changing the VFO of the active receiver
//
void vfo_id_step(int id, int steps) {

  if(!locked) {
    RECEIVER *other_receiver;
    long long delta;
    if(vfo[id].ctun) {
      delta=vfo[id].ctun_frequency;
      vfo[id].ctun_frequency=ROUND(vfo[id].ctun_frequency,steps);
      delta=vfo[id].ctun_frequency - delta;
    } else {
      delta=vfo[id].frequency;
      vfo[id].frequency=ROUND(vfo[id].frequency,steps);
      delta = vfo[id].frequency - delta;
    }

    int sid=id==0?1:0;
    if (sid < receivers) {
      other_receiver=receiver[sid];
    }

    switch(sat_mode) {
      case SAT_NONE:
        break;
      case SAT_MODE:
        // A and B increment and decrement together
        if (vfo[sid].ctun) {
          vfo[sid].ctun_frequency += delta;
        } else {
          vfo[sid].frequency      += delta;
        }
        if(receivers==2) {
          receiver_frequency_changed(other_receiver);
        }
        break;
      case RSAT_MODE:
        // A increments and B decrements or A decrments and B increments
        if (vfo[sid].ctun) {
          vfo[sid].ctun_frequency -= delta;
        } else {
          vfo[sid].frequency      -= delta;
        }
        if(receivers==2) {
          receiver_frequency_changed(other_receiver);
        }
        break;
    }

    receiver_frequency_changed(active_receiver);
    g_idle_add(ext_vfo_update,NULL);
  }
}

//
// vfo_move (and vfo_id_move) are exclusively used
// to update the radio while dragging with the 
// pointer device in the panadapter area. Therefore,
// the behaviour is different whether we use CTUN or not.
//
// In "normal" (non-CTUN) mode, we "drag the spectrum". This
// means, when dragging to the right the spectrum moves towards
// higher frequencies  this means the RX frequence is *decreased*.
//
// In "CTUN" mode, the spectrum is nailed to the display and we
// move the CTUN frequency. So dragging to the right
// *increases* the RX frequency.
//
void vfo_id_move(int id,long long hz,int round) {
  long long delta;
  int sid;
  RECEIVER *other_receiver;

#ifdef CLIENT_SERVER
  if(radio_is_remote) {
    //send_vfo_move(client_socket,id,hz,round);
    update_vfo_move(id,hz,round);
    return;
  }
#endif

  if(!locked) {
    if(vfo[id].ctun) {
      // don't let ctun go beyond end of passband
      long long frequency=vfo[id].frequency;
      long long rx_low=vfo[id].ctun_frequency+hz+active_receiver->filter_low;
      long long rx_high=vfo[id].ctun_frequency+hz+active_receiver->filter_high;
      long long half=(long long)active_receiver->sample_rate/2LL;
      long long min_freq=frequency-half;
      long long max_freq=frequency+half;

      if(rx_low<=min_freq) {
        return;
      } else if(rx_high>=max_freq) {
        return;
      }

      delta=vfo[id].ctun_frequency;
      // *Add* the shift (hz) to the ctun frequency
      vfo[id].ctun_frequency=vfo[id].ctun_frequency+hz;
      if(round && (vfo[id].mode!=modeCWL && vfo[id].mode!=modeCWU)) {
         vfo[id].ctun_frequency=ROUND(vfo[id].ctun_frequency,0);
      }
      delta=vfo[id].ctun_frequency - delta;
    } else {
      delta=vfo[id].frequency;
      // *Subtract* the shift (hz) from the VFO frequency
      vfo[id].frequency=vfo[id].frequency-hz;
      if(round && (vfo[id].mode!=modeCWL && vfo[id].mode!=modeCWU)) {
         vfo[id].frequency=ROUND(vfo[id].frequency,0);
      }
      delta = vfo[id].frequency - delta;
    }

    sid=id==0?1:0;
    if (sid < receivers) {
      other_receiver=receiver[sid];
    }

    switch(sat_mode) {
      case SAT_NONE:
        break;
      case SAT_MODE:
        // A and B increment and decrement together
        if (vfo[sid].ctun) {
          vfo[sid].ctun_frequency += delta;
        } else {
          vfo[sid].frequency      += delta;
        }
        if(receivers==2) {
          receiver_frequency_changed(other_receiver);
        }
        break;
      case RSAT_MODE:
        // A increments and B decrements or A decrments and B increments
        if (vfo[sid].ctun) {
          vfo[sid].ctun_frequency -= delta;
        } else {
          vfo[sid].frequency      -= delta;
        }
        if(receivers==2) {
          receiver_frequency_changed(other_receiver);
        }
        break;
    }
    receiver_frequency_changed(receiver[id]);
    g_idle_add(ext_vfo_update,NULL);
  }
}

void vfo_move(long long hz,int round) {
  vfo_id_move(active_receiver->id,hz,round);
}

void vfo_move_to(long long hz) {
  // hz is the offset from the min displayed frequency
  int id=active_receiver->id;
  long long offset=hz;
  long long half=(long long)(active_receiver->sample_rate/2);
  long long f;
  long long delta;
  int sid;
  RECEIVER *other_receiver;

#ifdef CLIENT_SERVER
  if(radio_is_remote) {
    send_vfo_move_to(client_socket,id,hz);
    return;
  }
#endif

  if(vfo[id].mode!=modeCWL && vfo[id].mode!=modeCWU) {
    offset=ROUND(hz,0);
  }
  f=(vfo[id].frequency-half)+offset+((double)active_receiver->pan*active_receiver->hz_per_pixel);

  if(!locked) {
    if(vfo[id].ctun) {
      delta=vfo[id].ctun_frequency;
      vfo[id].ctun_frequency=f;
      if(vfo[id].mode==modeCWL) {
        vfo[id].ctun_frequency+=cw_keyer_sidetone_frequency;
      } else if(vfo[id].mode==modeCWU) {
        vfo[id].ctun_frequency-=cw_keyer_sidetone_frequency;
      }
      delta=vfo[id].ctun_frequency - delta;
    } else {
      delta=vfo[id].frequency;
      vfo[id].frequency=f;
      if(vfo[id].mode==modeCWL) {
        vfo[id].frequency+=cw_keyer_sidetone_frequency;
      } else if(vfo[id].mode==modeCWU) {
        vfo[id].frequency-=cw_keyer_sidetone_frequency;
      }
      delta = vfo[id].frequency - delta;
    }

    sid=id==0?1:0;
    if (sid < receivers) {
      other_receiver=receiver[sid];
    }

    switch(sat_mode) {
      case SAT_NONE:
        break;
      case SAT_MODE:
        // A and B increment and decrement together
        if (vfo[sid].ctun) {
          vfo[sid].ctun_frequency += delta;
        } else {
          vfo[sid].frequency      += delta;
        }
        if(receivers==2) {
          receiver_frequency_changed(other_receiver);
        }
        break;
      case RSAT_MODE:
        // A increments and B decrements or A decrements and B increments
        if (vfo[sid].ctun) {
          vfo[sid].ctun_frequency -= delta;
        } else {
          vfo[sid].frequency      -= delta;
        }
        if(receivers==2) {
          receiver_frequency_changed(other_receiver);
        }
        break;
    }

    receiver_vfo_changed(active_receiver);

    g_idle_add(ext_vfo_update,NULL);
  }
}

static gboolean
vfo_scroll_event_cb (GtkWidget      *widget,
               GdkEventScroll *event,
               gpointer        data)
{
  if(event->direction==GDK_SCROLL_UP) {
    vfo_step(1);
  } else {
    vfo_step(-1);
  }
  return FALSE;
}


static gboolean vfo_configure_event_cb (GtkWidget         *widget,
            GdkEventConfigure *event,
            gpointer           data)
{
  if (vfo_surface)
    cairo_surface_destroy (vfo_surface);

  vfo_surface = gdk_window_create_similar_surface (gtk_widget_get_window (widget),
                                       CAIRO_CONTENT_COLOR,
                                       gtk_widget_get_allocated_width (widget),
                                       gtk_widget_get_allocated_height (widget));

  /* Initialize the surface to black */
  cairo_t *cr;
  cr = cairo_create (vfo_surface);
  cairo_set_source_rgba(cr, COLOUR_VFO_BACKGND);
  cairo_paint (cr);
  cairo_destroy(cr);
  g_idle_add(ext_vfo_update,NULL);
  return TRUE;
}

static gboolean vfo_draw_cb (GtkWidget *widget,
 cairo_t   *cr,
 gpointer   data)
{
  cairo_set_source_surface (cr, vfo_surface, 0.0, 0.0);
  cairo_paint (cr);
  return FALSE;
}

void vfo_update() {

    int id=active_receiver->id;
    int txvfo=get_tx_vfo();

    FILTER* band_filters=filters[vfo[id].mode];
    FILTER* band_filter=&band_filters[vfo[id].filter];
    if(vfo_surface) {
        char temp_text[32];
        cairo_t *cr;
        cr = cairo_create (vfo_surface);
        cairo_set_source_rgba(cr, COLOUR_VFO_BACKGND);
        cairo_paint (cr);

        cairo_select_font_face(cr, DISPLAY_FONT,
            CAIRO_FONT_SLANT_NORMAL,
            CAIRO_FONT_WEIGHT_BOLD);

        switch(vfo[id].mode) {
          case modeFMN:
            //
            // filter edges are +/- 5500 if deviation==2500,
            //              and +/- 8000 if deviation==5000
            if(active_receiver->deviation==2500) {
              sprintf(temp_text,"%s 11k",mode_string[vfo[id].mode]);
            } else {
              sprintf(temp_text,"%s 16k",mode_string[vfo[id].mode]);
            }
            break;
          case modeCWL:
          case modeCWU:
            sprintf(temp_text,"%s %s %d wpm %d Hz",mode_string[vfo[id].mode],band_filter->title,cw_keyer_speed,cw_keyer_sidetone_frequency);
            break;
          case modeLSB:
          case modeUSB:
          case modeDSB:
          case modeAM:
            sprintf(temp_text,"%s %s",mode_string[vfo[id].mode],band_filter->title);
            break;
          default:
            sprintf(temp_text,"%s %s",mode_string[vfo[id].mode],band_filter->title);
            break;
        }
        cairo_set_font_size(cr, DISPLAY_FONT_SIZE2);
        cairo_set_source_rgba(cr, COLOUR_ATTN);
        cairo_move_to(cr, 5, 15);
        cairo_show_text(cr, temp_text);

        // In what follows, we want to display the VFO frequency
        // on which we currently transmit a signal with red colour.
        // If it is out-of-band, we display "Out of band" in red.
        // Frequencies we are not transmitting on are displayed in green
        // (dimmed if the freq. does not belong to the active receiver).

        // Frequencies of VFO A and B

        long long af = vfo[0].ctun ? vfo[0].ctun_frequency : vfo[0].frequency;
        long long bf = vfo[1].ctun ? vfo[1].ctun_frequency : vfo[1].frequency;

        if(vfo[0].entering_frequency) {
          af=vfo[0].entered_frequency;
        }
        if(vfo[1].entering_frequency) {
          bf=vfo[1].entered_frequency;
        }

#if 0
//
// DL1YCF: code still here but deactivated:
// there is no consensus whether the "VFO display frequency" should move if
// RIT/XIT values are changed. My Kenwood TS590 does, but some popular
// other SDR software does not.
// So although I do not feel too well if the actual TX frequency is not
// that on the display, I deactivate the code but leave it here so it
// can quickly be re-activated if one wants.
//
        //
        // If RIT or XIT is active, add this to displayed VFO frequency
        //
        // Adjust VFO_A frequency
        //
        if (isTransmitting() && txvfo == 0) {
          if (transmitter->xit_enabled) af += transmitter->xit;
        } else {
          if (vfo[0].rit_enabled) af += vfo[0].rit;
        }
        //
        // Adjust VFO_B frequency
        //
        if (isTransmitting() && txvfo == 1) {
          if (transmitter->xit_enabled) bf += transmitter->xit;
        } else {
          if (vfo[1].rit_enabled) bf += vfo[0].rit;
        }
#endif

        int oob=0;
        if (can_transmit) oob=transmitter->out_of_band;

        sprintf(temp_text,"VFO A: %0lld.%06lld",af/(long long)1000000,af%(long long)1000000);
        if(txvfo == 0 && (isTransmitting() || oob)) {
            if (oob) sprintf(temp_text,"VFO A: Out of band");
            cairo_set_source_rgba(cr, COLOUR_ALARM);
        } else {
            if(vfo[0].entering_frequency) {
              cairo_set_source_rgba(cr, COLOUR_ATTN);
            } else if(id==0) {
              cairo_set_source_rgba(cr, COLOUR_OK);
            } else {
              cairo_set_source_rgba(cr, COLOUR_OK_WEAK);
            }
        }
        cairo_move_to(cr, 5, 38);
        cairo_set_font_size(cr, DISPLAY_FONT_SIZE4);
        cairo_show_text(cr, temp_text);

        sprintf(temp_text,"VFO B: %0lld.%06lld",bf/(long long)1000000,bf%(long long)1000000);
        if(txvfo == 1 && (isTransmitting() || oob)) {
            if (oob) sprintf(temp_text,"VFO B: Out of band");
            cairo_set_source_rgba(cr, COLOUR_ALARM);
        } else {
            if(vfo[1].entering_frequency) {
              cairo_set_source_rgba(cr, COLOUR_ATTN);
            } else if(id==1) {
              cairo_set_source_rgba(cr, COLOUR_OK);
            } else {
              cairo_set_source_rgba(cr, COLOUR_OK_WEAK);
            }
        }
        cairo_move_to(cr, 300, 38);
        cairo_show_text(cr, temp_text);

        if(can_transmit) {
          cairo_move_to(cr, 120, 50);
          if(transmitter->puresignal) {
            cairo_set_source_rgba(cr, COLOUR_ATTN);
          } else {
            cairo_set_source_rgba(cr, COLOUR_SHADE);
          }
          cairo_set_font_size(cr, DISPLAY_FONT_SIZE2);
          cairo_show_text(cr, "PS");
        }

        cairo_move_to(cr, 55, 50);
        if(active_receiver->zoom>1) {
          cairo_set_source_rgba(cr, COLOUR_ATTN);
        } else {
          cairo_set_source_rgba(cr, COLOUR_SHADE);
        }
        cairo_set_font_size(cr, DISPLAY_FONT_SIZE2);
        sprintf(temp_text,"Zoom x%d",active_receiver->zoom);
        cairo_show_text(cr, temp_text);

        if(vfo[id].rit_enabled==0) {
            cairo_set_source_rgba(cr, COLOUR_SHADE);
        } else {
            cairo_set_source_rgba(cr, COLOUR_ATTN);
        }
        sprintf(temp_text,"RIT: %lldHz",vfo[id].rit);
        cairo_move_to(cr, 170, 15);
        cairo_set_font_size(cr, DISPLAY_FONT_SIZE2);
        cairo_show_text(cr, temp_text);


        if(can_transmit) {
          if(transmitter->xit_enabled==0) {
              cairo_set_source_rgba(cr, COLOUR_SHADE);
          } else {
              cairo_set_source_rgba(cr, COLOUR_ATTN);
          }
          sprintf(temp_text,"XIT: %lldHz",transmitter->xit);
          cairo_move_to(cr, 310, 15);
          cairo_set_font_size(cr, DISPLAY_FONT_SIZE2);
          cairo_show_text(cr, temp_text);
        }

        // NB and NB2 are mutually exclusive, therefore
        // they are put to the same place in order to save
        // some space
        cairo_move_to(cr, 145, 50);
        if(active_receiver->nb) {
          cairo_set_source_rgba(cr, COLOUR_ATTN);
          cairo_show_text(cr, "NB");
        } else if (active_receiver->nb2) {
          cairo_set_source_rgba(cr, COLOUR_ATTN);
          cairo_show_text(cr, "NB2");
        } else {
          cairo_set_source_rgba(cr, COLOUR_SHADE);
          cairo_show_text(cr, "NB");
        }

        // NR and NR2 are mutually exclusive
        cairo_move_to(cr, 175, 50);
        if(active_receiver->nr) {
          cairo_set_source_rgba(cr, COLOUR_ATTN);
          cairo_show_text(cr, "NR");
        } else if (active_receiver->nr2) {
          cairo_set_source_rgba(cr, COLOUR_ATTN);
          cairo_show_text(cr, "NR2");
        } else {
          cairo_set_source_rgba(cr, COLOUR_SHADE);
          cairo_show_text(cr, "NR");
        }

        cairo_move_to(cr, 200, 50);
        if(active_receiver->anf) {
          cairo_set_source_rgba(cr, COLOUR_ATTN);
        } else {
          cairo_set_source_rgba(cr, COLOUR_SHADE);
        }
        cairo_show_text(cr, "ANF");

        cairo_move_to(cr, 230, 50);
        if(active_receiver->snb) {
          cairo_set_source_rgba(cr, COLOUR_ATTN);
        } else {
          cairo_set_source_rgba(cr, COLOUR_SHADE);
        }
        cairo_show_text(cr, "SNB");

        cairo_move_to(cr, 265, 50);
        switch(active_receiver->agc) {
          case AGC_OFF:
            cairo_set_source_rgba(cr, COLOUR_SHADE);
            cairo_show_text(cr, "AGC OFF");
            break;
          case AGC_LONG:
            cairo_set_source_rgba(cr, COLOUR_ATTN);
            cairo_show_text(cr, "AGC LONG");
            break;
          case AGC_SLOW:
            cairo_set_source_rgba(cr, COLOUR_ATTN);
            cairo_show_text(cr, "AGC SLOW");
            break;
          case AGC_MEDIUM:
            cairo_set_source_rgba(cr, COLOUR_ATTN);
            cairo_show_text(cr, "AGC MED");
            break;
          case AGC_FAST:
            cairo_set_source_rgba(cr, COLOUR_ATTN);
            cairo_show_text(cr, "AGC FAST");
            break;
        }

        //
        // Since we can now change it by a MIDI controller,
        // we should display the compressor (level)
        //
        if(can_transmit) {
          cairo_move_to(cr, 335, 50);
          if (transmitter->compressor) {
              sprintf(temp_text,"CMPR %d",(int) transmitter->compressor_level);
              cairo_set_source_rgba(cr, COLOUR_ATTN);
              cairo_show_text(cr, temp_text);
          } else {
              cairo_set_source_rgba(cr, COLOUR_SHADE);
              cairo_show_text(cr, "CMPR");
          }
        }
        //
        // Indicate whether an equalizer is active
        //
        cairo_move_to(cr, 400, 50);
        if ((isTransmitting() && enable_tx_equalizer) || (!isTransmitting() && enable_rx_equalizer)) {
          cairo_set_source_rgba(cr, COLOUR_ATTN);
        } else {
          cairo_set_source_rgba(cr, COLOUR_SHADE);
        }
        cairo_show_text(cr, "EQ");

        cairo_move_to(cr, 500, 50);
        if(diversity_enabled) {
          cairo_set_source_rgba(cr, COLOUR_ATTN);
        } else {
          cairo_set_source_rgba(cr, COLOUR_SHADE);
        }
        cairo_show_text(cr, "DIV");

        int s;
        for(s=0;s<STEPS;s++) {
          if(steps[s]==step) break;
        }
        if(s>=STEPS) s=0;

        sprintf(temp_text,"Step %s",step_labels[s]);
        cairo_move_to(cr, 400, 15);
        cairo_set_source_rgba(cr, COLOUR_ATTN);
        cairo_show_text(cr, temp_text);

        cairo_move_to(cr, 425, 50);
        if(vfo[id].ctun) {
          cairo_set_source_rgba(cr, COLOUR_ATTN);
        } else {
          cairo_set_source_rgba(cr, COLOUR_SHADE);
        }
        cairo_show_text(cr, "CTUN");

        cairo_move_to(cr, 468, 50);
        if(cat_control>0) {
          cairo_set_source_rgba(cr, COLOUR_ATTN);
        } else {
          cairo_set_source_rgba(cr, COLOUR_SHADE);
        }
        cairo_show_text(cr, "CAT");

        if(can_transmit) {
          cairo_move_to(cr, 500, 15);
          if(vox_enabled) {
            cairo_set_source_rgba(cr, COLOUR_ALARM);
          } else {
            cairo_set_source_rgba(cr, COLOUR_SHADE);
          }
          cairo_show_text(cr, "VOX");
        }

        cairo_move_to(cr, 5, 50);
        if(locked) {
          cairo_set_source_rgba(cr, COLOUR_ALARM);
        } else {
          cairo_set_source_rgba(cr, COLOUR_SHADE);
        }
        cairo_show_text(cr, "Locked");

        cairo_move_to(cr, 265, 15);
        if(split) {
          cairo_set_source_rgba(cr, COLOUR_ALARM);
        } else {
          cairo_set_source_rgba(cr, COLOUR_SHADE);
        }
        cairo_show_text(cr, "Split");

        cairo_move_to(cr, 265, 27);
        if(sat_mode!=SAT_NONE) {
          cairo_set_source_rgba(cr, COLOUR_ALARM);
        } else {
          cairo_set_source_rgba(cr, COLOUR_SHADE);
        }
        if(sat_mode==SAT_NONE || sat_mode==SAT_MODE) {
          cairo_show_text(cr, "SAT");
        } else {
          cairo_show_text(cr, "RSAT");
        }


        if(duplex) {
            cairo_set_source_rgba(cr, COLOUR_ALARM);
        } else {
            cairo_set_source_rgba(cr, COLOUR_SHADE);
        }
        sprintf(temp_text,"DUP");
        cairo_move_to(cr, 265, 39);
        cairo_set_font_size(cr, DISPLAY_FONT_SIZE2);
        cairo_show_text(cr, temp_text);

        cairo_destroy (cr);
        gtk_widget_queue_draw (vfo_panel);
    } else {
g_print("%s: no surface!\n",__FUNCTION__);
    }
}

static gboolean
vfo_press_event_cb (GtkWidget *widget,
               GdkEventButton *event,
               gpointer        data)
{
  start_vfo(event->x<300?VFO_A:VFO_B);
  return TRUE;
}

GtkWidget* vfo_init(int width,int height) {

  my_width=width;
  my_height=height;

  vfo_panel = gtk_drawing_area_new ();
  gtk_widget_set_size_request (vfo_panel, width, height);

  g_signal_connect (vfo_panel,"configure-event",
            G_CALLBACK (vfo_configure_event_cb), NULL);
  g_signal_connect (vfo_panel, "draw",
            G_CALLBACK (vfo_draw_cb), NULL);

  /* Event signals */
  g_signal_connect (vfo_panel, "button-press-event",
            G_CALLBACK (vfo_press_event_cb), NULL);
  g_signal_connect(vfo_panel,"scroll_event",
            G_CALLBACK(vfo_scroll_event_cb),NULL);
  gtk_widget_set_events (vfo_panel, gtk_widget_get_events (vfo_panel)
                     | GDK_BUTTON_PRESS_MASK
                     | GDK_SCROLL_MASK);

  return vfo_panel;
}

//
// Some utility functions to get characteristics of the current
// transmitter. These functions can be used even if there is no
// transmitter (transmitter->mode may segfault).
//

int get_tx_vfo() {
  int txvfo=active_receiver->id;
  if (split) txvfo = 1 - txvfo;
  return txvfo;
}

int get_tx_mode() {
  int txvfo=active_receiver->id;
  if (split) txvfo = 1 - txvfo;
  if (can_transmit) {
    return vfo[txvfo].mode;
  } else {
    return modeUSB;
  }
}

long long get_tx_freq() {
  int txvfo=active_receiver->id;
  if (split) txvfo = 1 - txvfo;
  if (vfo[txvfo].ctun) {
    return  vfo[txvfo].ctun_frequency;
  } else {
    return vfo[txvfo].frequency;
  }
}
void vfo_rit_update(int id) {
  vfo[id].rit_enabled=vfo[id].rit_enabled==1?0:1;
  if (id < receivers) {
    receiver_frequency_changed(receiver[id]);
  }
  g_idle_add(ext_vfo_update, NULL);
}

void vfo_rit_clear(int id) {
  vfo[id].rit=0;
  vfo[id].rit_enabled=0;
  if (id < receivers) {
    receiver_frequency_changed(receiver[id]);
  }
  g_idle_add(ext_vfo_update, NULL);
}

void vfo_rit(int id,int i) {
  double value=(double)vfo[id].rit;
  value+=(double)(i*rit_increment);
  if(value<-10000.0) {
    value=-10000.0;
  } else if(value>10000.0) {
    value=10000.0;
  }
  vfo[id].rit=value;
  vfo[id].rit_enabled=(value!=0);
  if (id < receivers) {
    receiver_frequency_changed(receiver[id]);
  }
  g_idle_add(ext_vfo_update,NULL);
}

//
// Interface to set the frequency, including
// "long jumps", for which we may have to
// change the band. This is solely used for
//
// - FREQ MENU
// - MIDI or GPIO NumPad
// - CAT "set frequency" command
//
void vfo_set_frequency(int v,long long f) {
  int b=get_band_from_frequency(f);
  if (b != vfo[v].band) {
    vfo_band_changed(v, b);
  }
  if (v == VFO_A) receiver_set_frequency(receiver[0], f);
  if (v == VFO_B) {
    //
    // If there is only one receiver, there is no RX running that
    // is controlled by VFO_B, so just update the frequency of the
    // VFO without telling WDSP about it.
    // If VFO_B controls a (running) receiver, do the "full job".
    //
    if (receivers == 2) {
      receiver_set_frequency(receiver[1], f);
    } else {
      vfo[v].frequency=f;
      if (vfo[v].ctun) {
        vfo[v].ctun=FALSE;
        vfo[v].offset=0;
        vfo[v].ctun_frequency=vfo[v].frequency;
      }
    }
  }
  g_idle_add(ext_vfo_update, NULL);
}

//
// Set CTUN state of a VFO
//
void vfo_ctun_update(int id,int state) {
  //
  // Note: if this VFO does not control a (running) receiver,
  //       receiver_set_frequency is *not* called therefore
  //       we should update ctun_frequency and offset
  //
  if (vfo[id].ctun == state) return;  // no-op if no change
  vfo[id].ctun=state;
  if(vfo[id].ctun) {
    // CTUN turned OFF->ON
    vfo[id].ctun_frequency=vfo[id].frequency;
    vfo[id].offset=0;
    if (id < receivers) {
      receiver_set_frequency(receiver[id],vfo[id].ctun_frequency);
    }
  } else {
    // CTUN turned ON->OFF: keep frequency
    vfo[id].frequency=vfo[id].ctun_frequency;
    vfo[id].offset=0;
    if (id < receivers) {
      receiver_set_frequency(receiver[id],vfo[id].ctun_frequency);
    }
  }
}

//
// helper function for numerically entering a new VFO frequency
//
void num_pad(int val) {
  //
  // The numpad may be difficult to use since the frequency has to be given in Hz
  // TODO: add a multiplier button like "kHz"
  // TODO: display the current "entered_frequency" somewhere
  //       (not all of us are good in typing blind)
  //
  RECEIVER *rx=active_receiver;
  if(!vfo[rx->id].entering_frequency) {
    vfo[rx->id].entered_frequency=0;
    vfo[rx->id].entering_frequency=TRUE;
  }
  switch(val) {
    case -1: // clear
      vfo[rx->id].entered_frequency=0;
      vfo[rx->id].entering_frequency=FALSE;
      break;
    case -2: // enter
      if(vfo[rx->id].entered_frequency!=0) {
        receiver_set_frequency(rx, vfo[rx->id].entered_frequency);
        g_idle_add(ext_vfo_update, NULL);
      }
      vfo[rx->id].entering_frequency=FALSE;
      break;
    default:
      vfo[rx->id].entered_frequency=(vfo[rx->id].entered_frequency*10)+val;
      break;
  }
}

