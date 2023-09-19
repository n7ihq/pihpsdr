/* Copyright (C)
* 2019 - Christoph van Wüllen, DL1YCF
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

/*
 * Layer-3 of MIDI support
 *
 * In most cases, a certain action only makes sense for a specific
 * type. For example, changing the VFO frequency will only be implemeted
 * for MIDI_WHEEL, and TUNE off/on only with MIDI_KNOB.
 *
 * However, changing the volume makes sense both with MIDI_KNOB and MIDI_WHEEL.
 */
#include <gtk/gtk.h>

#include "actions.h"
#include "midi.h"

void DoTheMidi(int action, enum ACTIONtype type, int val) {
  //t_print("%s: action=%d type=%d val=%d\n",__FUNCTION__,action,type,val);
  switch (type) {
  case MIDI_KEY:
    schedule_action(action, val ? PRESSED : RELEASED, 0);
    break;

  case MIDI_KNOB:
    schedule_action(action, ABSOLUTE, val);
    break;

  case MIDI_WHEEL:
    schedule_action(action, RELATIVE, val);
    break;

  default:
    // other types cannot happen for MIDI
    break;
  }
}
