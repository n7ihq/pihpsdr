/* Copyright (C)
* 2015 - John Melton, G0ORX/N6LYT,  2016 - Steve Wilson, KA6S
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bandstack.h"
#include "band.h"
#include "filter.h"
#include "mode.h"
#include "property.h"
#include "store.h"
#include "store_menu.h"
#include "radio.h"
#include "ext.h"
#include "vfo.h"
#include "message.h"

MEM mem[NUM_OF_MEMORYS];  // This makes it a compile time option

void memSaveState() {
  char name[128];
  char value[128];

  for (int b = 0; b < NUM_OF_MEMORYS; b++) {
    SetPropI1("mem.%d.freqA", b,           mem[b].frequency);
    SetPropI1("mem.%d.mode", b,            mem[b].mode);
    SetPropI1("mem.%d.filter", b,          mem[b].filter);
    SetPropI1("mem.%d.deviation", b,       mem[b].deviation);
    SetPropI1("mem.%d.ctcss_enabled", b,   mem[b].ctcss_enabled);
    SetPropI1("mem.%d.ctcss", b,           mem[b].ctcss);
  }
}

void memRestoreState() {
  char name[128];
  char *value;

  for (int b = 0; b < NUM_OF_MEMORYS; b++) {
    //
    // Set defaults
    //
    mem[b].frequency = 28010000LL;
    mem[b].mode = modeCWU;
    mem[b].filter = filterF5;
    mem[b].deviation = 2500;
    mem[b].ctcss_enabled = 0;
    mem[b].ctcss = 0;
    //
    // Read from props
    //
    GetPropI1("mem.%d.freqA", b,           mem[b].frequency);
    GetPropI1("mem.%d.mode", b,            mem[b].mode);
    GetPropI1("mem.%d.filter", b,          mem[b].filter);
    GetPropI1("mem.%d.deviation", b,       mem[b].deviation);
    GetPropI1("mem.%d.ctcss_enabled", b,   mem[b].ctcss_enabled);
    GetPropI1("mem.%d.ctcss", b,           mem[b].ctcss);
  }
}

void recall_memory_slot(int index) {
  long long new_freq;
  new_freq = mem[index].frequency;
  //
  // Recalling a memory slot is equivalent to the following actions
  //
  // - set new CTCSS parameters
  // - set the new frequency
  // - set the new mode
  // - set the new filter and deviation
  //
  // This automatically restores the filter, noise reduction, and
  // equalizer settings stored with that mode
  //
  // This will not only change the filter but also store the new setting
  // with that mode.
  //

  if (can_transmit) {
    transmitter_set_ctcss(transmitter, mem[index].ctcss_enabled, mem[index].ctcss);
  }

  vfo_deviation_changed(mem[index].deviation);
  vfo_set_frequency(active_receiver->id, new_freq);
  vfo_mode_changed(mem[index].mode);
  vfo_filter_changed(mem[index].filter);
  g_idle_add(ext_vfo_update, NULL);
}

void store_memory_slot(int index) {
  int id = active_receiver->id;

  //
  // Store current frequency, mode, and filter in slot #index
  //
  if (vfo[id].ctun) {
    mem[index].frequency = vfo[id].ctun_frequency;
  } else {
    mem[index].frequency = vfo[id].frequency;
  }

  mem[index].mode = vfo[id].mode;
  mem[index].filter = vfo[id].filter;

  if (can_transmit) {
    mem[index].ctcss_enabled = transmitter->ctcss_enabled;
    mem[index].ctcss = transmitter->ctcss;
  }

  //memSaveState();
}

