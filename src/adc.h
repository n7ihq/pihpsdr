/* Copyright (C)
* 2018 - John Melton, G0ORX/N6LYT
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

#ifndef ADC_H
#define ADC_H

#include <gtk/gtk.h>

enum {
  AUTOMATIC = 0,
  MANUAL
};

enum {
  BYPASS = 0,
  HPF_1_5,
  HPF_6_5,
  HPF_9_5,
  HPF_13,
  HPF_20
};

enum {
  LPF_160 = 0,
  LPF_80,
  LPF_60_40,
  LPF_30_20,
  LPF_17_15,
  LPF_12_10,
  LPF_6
};

enum {
  ANTENNA_1 = 0,
  ANTENNA_2,
  ANTENNA_3,
  ANTENNA_XVTR,
  ANTENNA_EXT1,
  ANTENNA_EXT2
};

typedef struct _adc {
  int filters;
  int hpf;
  int lpf;
  int antenna;
  uint8_t dither;
  uint8_t random;
  uint8_t preamp;
  int attenuation;
  uint8_t enable_step_attenuation;
  uint8_t agc;
  double gain;
  double min_gain;
  double max_gain;
} ADC;

#endif
