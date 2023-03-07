/*
 * This file contains lots of #defines that control the appearance
 * of piHPSDR, e.g.
 * - window sizes
 * - font sizes
 * colours.
 *
 * The purpose of this file is that the appearance can be
 * changed easily at compile time.
 */


// Define maximum window size.
// Standard values 800 and 480: suitable for RaspberryBi 7-inch screen

#define MAX_DISPLAY_WIDTH  800
#define MAX_DISPLAY_HEIGHT 480

//
// Fonts and sizes for VFO, meter, panadapter etc.
//
#define DISPLAY_FONT "FreeSans"   // FreeMono in original piHPSDR
#define DISPLAY_FONT_SIZE1 10     // sizes are as in original piHPSDR
#define DISPLAY_FONT_SIZE2 12
#define DISPLAY_FONT_SIZE3 16
#define DISPLAY_FONT_SIZE4 22

#define SLIDERS_FONT "FreeSans Bold 10"   // Used for description in the sliders/zoompan area

//
// Colours. They are given as a 4-tuple (RGB and opacity). The default value for the
// opacity is 1.0
//

//
// There are three "traffic light" colors ALARM, ATTN, OK (default: red, yellow, green)
// that are used in various places, and are also used for a "gradient" panadapter
// For that purpose, we also need them in a "transparent" version
//
#define COLOUR_ALARM      1.00, 0.00, 0.00, 1.00
#define COLOUR_ALARM_WEAK 0.50, 0.00, 0.00, 1.00  // non-active version
#define COLOUR_ATTN       1.00, 1.00, 0.00, 1.00
#define COLOUR_ATTN_WEAK  0.50, 0.50, 0.00, 1.00  // non-active  version
#define COLOUR_OK         0.00, 1.00, 0.00, 1.00
#define COLOUR_OK_WEAK    0.00, 0.50, 0.00, 1.00  // non-active version
#define COLOUR_METER      1.00, 1.00, 1.00, 1.00  // for drawing in the "meter" section
//
// Panadapter background colour             (default value: black             0.00, 0.00, 0.00, 1.00)
// Panadapter "filter passband" colour      (default value: gray              0.25, 0.25, 0.25, 1.00)
// Panadapter thick lines (dBm/Freq)        (default value: heavy turqouise   0.00, 1.00, 1.00, 1.00)
// Panadapter thin lines  /dBm/Freq)        (default value: light turqouise   0.00, 0.50, 0.50, 1.00)
//
#define COLOUR_PAN_BACKGND   0.00, 0.00, 0.00, 1.00   // Background: black
#define COLOUR_PAN_FILTER    0.25, 0.25, 0.25, 0.75   // Filter area: dark gray
#define COLOUR_PAN_LINE      0.00, 1.00, 1.00, 1.00   // dBm/Freq main lines: turquoise
#define COLOUR_PAN_LINE_WEAK 0.00, 0.50, 0.50, 1.00   // dBm/Freq aux  lines: light turquoise
#define COLOUR_PAN_60M       0.60, 0.30, 0.30, 1.00   // 60m band segments: reddish

//
// Colours for the VFO/Meter bar
//
#define COLOUR_VFO_BACKGND  0.00, 0.00, 0.00, 1.00
#define COLOUR_SHADE        0.70, 0.70, 0.70, 1.00   // VFO elements currently inactive

//
// Panadapter gradient colours
//
#define COLOUR_GRAD1        0.00, 1.00, 0.00, 1.00  // green
#define COLOUR_GRAD2        1.00, 0.66, 0.00, 1.00  // orange
#define COLOUR_GRAD3        1.00, 1.00, 0.00, 1.00  // yellow
#define COLOUR_GRAD4        1.00, 0.00, 0.00, 1.00  // red
#define COLOUR_GRAD1_WEAK   0.00, 0.50, 0.00, 1.00  // non-active version
#define COLOUR_GRAD2_WEAK   0.50, 0.33, 0.00, 1.00  // non-active version
#define COLOUR_GRAD3_WEAK   0.50, 0.50, 0.00, 1.00  // non-active version
#define COLOUR_GRAD4_WEAK   0.50, 0.00, 0.00, 1.00  // non-active version
//
// Panadapter "filled" in black/white, different intensities
//
#define COLOUR_PAN_FILL1    1.00, 1.00, 1.00, 0.25  // nearly transparent white
#define COLOUR_PAN_FILL2    1.00, 1.00, 1.00, 0.50  // half transparent white
#define COLOUR_PAN_FILL3    1.00, 1.00, 1.00, 0.75  // nearly opaque white

//
// Menu background color
//
#define COLOUR_MENU_BACKGND 1.00, 1.00, 1.00, 1.00
