/* Copyright (C)
* 2023 - Christoph van Wüllen, DL1YCF
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
 * This file contains data (tables) which describe the layout
 * e.g. of the VFO bar. The layout contains (x,y) coordinates of
 * the individual elements as well as font sizes.
 *
 * There can be more than one "layout", characterized by its size
 * request. So the program can choose the largest layout that
 * fits into the allocated area.
 *
 * What this should do is, that if the user increases the width of
 * the screen and the VFO bar, the program can automatically
 * switch to a larger font.
 */

#include <stdlib.h>

#include "appearance.h"

//
// Note the first layout that fits into the actual size of
// the VFO bar is taken, so the largest one come first, and
// the smallest one last.
//
const VFO_BAR_LAYOUT vfo_layout_list[] = {
  //
  // A layout tailored for a screen 1024 px wide:
  // a Layout with dial digits of size 40, and a "LED" size 17
  // which requires a width of 745 and a height of 78
  //
  {
    .description = "VFO bar for 1024px windows",
    .width = 745,
    .height = 82,
    .size1 = 14,
    .size2 = 26,
    .size3 = 40,

    .vfo_a_x = -5,
    .vfo_a_y = 59,
    .vfo_b_x = -490,
    .vfo_b_y = 59,

    .mode_x = 5,
    .mode_y = 21,
    .agc_x = 220,
    .agc_y = 21,
    .nr_x = 330,
    .nr_y = 21,
    .nb_x = 370,
    .nb_y = 21,
    .anf_x = 410,
    .anf_y = 21,
    .snb_x = 450,
    .snb_y = 21,
    .div_x = 490,
    .div_y = 21,
    .eq_x = 530,
    .eq_y = 21,
    .cat_x = 570,
    .cat_y = 21,

    .cmpr_x = 330,
    .cmpr_y = 40,
    .ps_x = 410,
    .ps_y = 40,

    .vox_x = 330,
    .vox_y = 59,
    .dup_x = 410,
    .dup_y = 59,

    .lock_x = 5,
    .lock_y = 78,
    .zoom_x = 80,
    .zoom_y = 78,
    .ctun_x = 155,
    .ctun_y = 78,
    .step_x = 220,
    .step_y = 78,
    .split_x = 330,
    .split_y = 78,
    .sat_x = 410,
    .sat_y = 78,
    .rit_x = 490,
    .rit_y = 78,
    .xit_x = 580,
    .xit_y = 78,
    .filter_x = 630,
    .filter_y = 21,
    .multifn_x = 675,
    .multifn_y = 78
  },
  {
    .description = "VFO bar for 900px windows",
    .width = 630,
    .height = 82,
    .size1 = 14,
    .size2 = 26,
    .size3 = 40,
    .vfo_a_x = -5,
    .vfo_a_y = 59,
    .vfo_b_x = -375,
    .vfo_b_y = 59,
    .lock_x = 5,
    .lock_y = 78,
    .mode_x = 5,
    .mode_y = 21,
    .agc_x = 190,
    .agc_y = 21,
    .nr_x = 290,
    .nr_y = 21,
    .nb_x = 330,
    .nb_y = 21,
    .anf_x = 375,
    .anf_y = 21,
    .snb_x = 415,
    .snb_y = 21,
    .div_x = 465,
    .div_y = 21,
    .cmpr_x = 500,
    .cmpr_y = 21,
    .cat_x = 590,
    .cat_y = 21,

    .vox_x = 290,
    .vox_y = 59,
    .dup_x = 330,
    .dup_y = 59,

    .eq_x = 290,
    .eq_y = 40,
    .ps_x = 330,
    .ps_y = 40,

    .zoom_x = 70,
    .zoom_y = 78,
    .ctun_x = 135,
    .ctun_y = 78,
    .step_x = 190,
    .step_y = 78,
    .split_x = 290,
    .split_y = 78,
    .sat_x = 330,
    .sat_y = 78,
    .rit_x = 375,
    .rit_y = 78,
    .xit_x = 465,
    .xit_y = 78,
    .filter_x = 0,
    .multifn_x = 560,
    .multifn_y = 78
  },
  {
    .description = "VFO bar for 800px windows",
    .width = 530,
    .height = 68,
    .size1 = 13,
    .size2 = 20,
    .size3 = 36,

    .vfo_a_x = -5,
    .vfo_a_y = 47,
    .vfo_b_x = -310,
    .vfo_b_y = 47,

    .mode_x = 5,
    .mode_y = 15,
    .agc_x = 175,
    .agc_y = 15,
    .nr_x = 240,
    .nr_y = 15,
    .nb_x = 275,
    .nb_y = 15,
    .anf_x = 310,
    .anf_y = 15,
    .snb_x = 350,
    .snb_y = 15,
    .div_x = 390,
    .div_y = 15,
    .cmpr_x = 430,
    .cmpr_y = 15,
    .cat_x = 500,
    .cat_y = 15,

    .eq_x = 240,
    .eq_y = 31,
    .ps_x = 275,
    .ps_y = 31,

    .vox_x = 240,
    .vox_y = 47,
    .dup_x = 275,
    .dup_y = 47,

    .lock_x = 5,
    .lock_y = 63,
    .zoom_x = 60,
    .zoom_y = 63,
    .ctun_x = 115,
    .ctun_y = 63,
    .step_x = 160,
    .step_y = 63,
    .split_x = 240,
    .split_y = 63,
    .sat_x = 275,
    .sat_y = 63,
    .rit_x = 310,
    .rit_y = 63,
    .xit_x = 390,
    .xit_y = 63,
    .filter_x = 0,
    .multifn_x = 470,
    .multifn_y = 63
  },

  //
  // This is for those who want to run piHPDSR on a 640x480 screen
  //
  {
    .description = "Squeezed Layout for 640px windows",
    .width = 370,
    .height = 84,
    .size1 = 13,
    .size2 = 20,
    .size3 = 26,
    .vfo_a_x = 5,
    .vfo_a_y = 41,
    .vfo_b_x = 200,
    .vfo_b_y = 41,
    .mode_x = 5,
    .mode_y = 15,
    .zoom_x = 65,
    .zoom_y = 54,
    .ps_x = 5,
    .ps_y = 68,
    .rit_x = 170,
    .rit_y = 15,
    .xit_x = 270,
    .xit_y = 15,
    .nb_x = 35,
    .nb_y = 82,
    .nr_x = 5,
    .nr_y = 82,
    .anf_x = 65,
    .anf_y = 82,
    .snb_x = 95,
    .snb_y = 82,
    .agc_x = 140,
    .agc_y = 82,
    .cmpr_x = 65,
    .cmpr_y = 68,
    .eq_x = 140,
    .eq_y = 68,
    .div_x = 35,
    .div_y = 68,
    .step_x = 210,
    .step_y = 82,
    .ctun_x = 210,
    .ctun_y = 68,
    .cat_x = 270,
    .cat_y = 54,
    .vox_x = 270,
    .vox_y = 68,
    .lock_x = 5,
    .lock_y = 54,
    .split_x = 170,
    .split_y = 54,
    .sat_x = 140,
    .sat_y = 54,
    .dup_x = 210,
    .dup_y = 54,
    .filter_x = 0,
    .multifn_x = 310,
    .multifn_y = 82
  },
  //
  // The last "layout" must have a negative width to
  // mark the end of the list
  //
  {
    .width = -1
  }
};

int vfo_layout = 0;
