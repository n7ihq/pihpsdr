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
const VFO_BAR_LAYOUT vfo_layout_list[] =
{
  //
  // A layout tailored for a screen 1024 px wide:
  // a Layout with dial digits of size 40, and a "LED" size 17
  // which requires a width of 745 and a height of 78
  //
  {
   .width=745,
   .height=78,
   .size1=17,
   .size2=26,
   .size3=40,
   .vfo_a_x=5,
   .vfo_a_y=59,
   .vfo_b_x=420,
   .vfo_b_y=59,
   .mode_x=5,
   .mode_y=21,
   .zoom_x=80,
   .zoom_y=78,
   .ps_x=335,
   .ps_y=78,
   .rit_x=245,
   .rit_y=21,
   .xit_x=375,
   .xit_y=21,
   .nb_x=375,
   .nb_y=40,
   .nr_x=335,
   .nr_y=40,
   .anf_x=335,
   .anf_y=59,
   .snb_x=375,
   .snb_y=59,
   .agc_x=520,
   .agc_y=78,
   .cmpr_x=635,
   .cmpr_y=78,
   .eq_x=635,
   .eq_y=21,
   .div_x=375,
   .div_y=78,
   .step_x=515,
   .step_y=21,
   .ctun_x=420,
   .ctun_y=78,
   .cat_x=155,
   .cat_y=78,
   .vox_x=700,
   .vox_y=21,
   .lock_x=5,
   .lock_y=78,
   .split_x=235,
   .split_y=78,
   .sat_x=195,
   .sat_y=78,
   .dup_x=285,
   .dup_y=78
  },
  //
  // A layout tailored for a screen 900 px wide:
  // a Layout with dial digits of size 32, and a "LED" size 14
  // which requires a width of 620 and a height of 66
  //
  {
   .width=620,
   .height=66,
   .size1=14,
   .size2=22,
   .size3=32,
   .vfo_a_x=5,
   .vfo_a_y=50,
   .vfo_b_x=360,
   .vfo_b_y=50,
   .mode_x=5,
   .mode_y=18,
   .zoom_x=65,
   .zoom_y=66,
   .ps_x=280,
   .ps_y=66,
   .rit_x=205,
   .rit_y=18,
   .xit_x=325,
   .xit_y=18,
   .nb_x=325,
   .nb_y=34,
   .nr_x=280,
   .nr_y=34,
   .anf_x=280,
   .anf_y=50,
   .snb_x=325,
   .snb_y=50,
   .agc_x=430,
   .agc_y=66,
   .cmpr_x=530,
   .cmpr_y=66,
   .eq_x=530,
   .eq_y=18,
   .div_x=325,
   .div_y=66,
   .step_x=430,
   .step_y=18,
   .ctun_x=360,
   .ctun_y=66,
   .cat_x=130,
   .cat_y=66,
   .vox_x=590,
   .vox_y=18,
   .lock_x=5,
   .lock_y=66,
   .split_x=200,
   .split_y=66,
   .sat_x=165,
   .sat_y=66,
   .dup_x=240,
   .dup_y=66
  },  
  //
  // The standard piHPSDR layout for a 800x480 screen:
  // a Layout with dial digits of size 26, and a "LED" size 12
  // which requires a width of 530 and a height of 55
  //
  {
   .width=530,
   .height=55,
   .size1=12,
   .size2=20,
   .size3=26,
   .vfo_a_x=5,
   .vfo_a_y=41,
   .vfo_b_x=300,
   .vfo_b_y=41,
   .mode_x=5,
   .mode_y=15,
   .zoom_x=55,
   .zoom_y=54,
   .ps_x=240,
   .ps_y=54,
   .rit_x=170,
   .rit_y=15,
   .xit_x=270,
   .xit_y=15,
   .nb_x=270,
   .nb_y=28,
   .nr_x=240,
   .nr_y=28,
   .anf_x=240,
   .anf_y=41,
   .snb_x=270,
   .snb_y=41,
   .agc_x=360,
   .agc_y=54,
   .cmpr_x=440,
   .cmpr_y=54,
   .eq_x=440,
   .eq_y=15,
   .div_x=270,
   .div_y=54,
   .step_x=360,
   .step_y=15,
   .ctun_x=300,
   .ctun_y=54,
   .cat_x=110,
   .cat_y=54,
   .vox_x=500,
   .vox_y=15,
   .lock_x=5,
   .lock_y=54,
   .split_x=175,
   .split_y=54,
   .sat_x=145,
   .sat_y=54,
   .dup_x=210,
   .dup_y=54
  },
  //
  // The last "layout" must have a negative width to
  // mark the end of the list
  //
  {
    .width=-1
  }
};

const VFO_BAR_LAYOUT *vfo_layout=NULL;
