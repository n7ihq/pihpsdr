/*
 * This file contains lots of #defines that control the appearance
 * of piHPSDR, e.g.
 * - window sizes
 * - font sizes
 * colours.
 *
 * The purpose of this file is that the appearance can be
 * changed easily at compile time.
 *
 * DO NOT CHANGE the "Default" values in the comments, since
 * these define the original look-and-feel of piHPSDR.
 *
 * IMPORTANT: think twice before adding new colours or font  sizes,
 *            and then decide to re-use an existing one.
 */


// Define maximum window size.
// Standard values 800 and 480: suitable for RaspberryBi 7-inch screen

#ifdef ANDROMEDA
#define MAX_DISPLAY_WIDTH  1200
#define MAX_DISPLAY_HEIGHT 600
#else
#define MAX_DISPLAY_WIDTH  800                      // Default: 800
#define MAX_DISPLAY_HEIGHT 480                      // Default: 480
#endif

//
// Fonts and sizes for VFO, meter, panadapter etc.
//
#define DISPLAY_FONT "FreeSans"                     // Default: FreeSans
#define DISPLAY_FONT_SIZE1 10                       // Default: 10
#define DISPLAY_FONT_SIZE2 12                       // Default: 12
#define DISPLAY_FONT_SIZE3 16                       // Default: 16
#define DISPLAY_FONT_SIZE4 22                       // Default: 22

#define SLIDERS_FONT "FreeSans Bold 10"             // Default: FreeSans Bold 10

//
// Colours. They are given as a 4-tuple (RGB and opacity).
// The default value for the opacity (1.0) is used  in most cases.
// "weak" versions of some colours (e.g. for the non-active receiver) are also available
//
// It is intended that these three colours are used in most of the cases.
//

//
// There are three "traffic light" colors ALARM, ATTN, OK (default: red, yellow, green)
// that are used in various places. All three colours should be clearly readable
// when written on a (usually dark) background.
//
#define COLOUR_ALARM         1.00, 0.00, 0.00, 1.00 // Default: 1.00, 0.00, 0.00, 1.00
#define COLOUR_ALARM_WEAK    0.50, 0.00, 0.00, 1.00 // Default: 0.50, 0.00, 0.00, 1.00
#define COLOUR_ATTN          1.00, 1.00, 0.00, 1.00 // Default: 1.00, 1.00, 0.00, 1.00
#define COLOUR_ATTN_WEAK     0.50, 0.50, 0.00, 1.00 // Default: 0.50, 0.50, 0.00, 1.00
#define COLOUR_OK            0.00, 1.00, 0.00, 1.00 // Default: 0.00, 1.00, 0.00, 1.00
#define COLOUR_OK_WEAK       0.00, 0.50, 0.00, 1.00 // Default: 0.00, 0.50, 0.00, 1.00
 
//
// Colours for drawing horizontal (dBm) and vertical (Frequency)
// lines in the panadapters, and indicating filter passbands and
// 60m band segments.
//
// The PAN_FILTER must be somewhat transparent, such that it does not hide a PAN_LINE.
//

#define COLOUR_PAN_FILTER    0.30, 0.30, 0.30, 0.66 // Default: 0.25, 0.25, 0.25, 0.75
#define COLOUR_PAN_LINE      0.00, 1.00, 1.00, 1.00 // Default: 0.00, 1.00, 1.00, 1.00
#define COLOUR_PAN_LINE_WEAK 0.00, 0.50, 0.50, 1.00 // Default: 0.00, 0.50, 0.50, 1.00
#define COLOUR_PAN_60M       0.60, 0.30, 0.30, 1.00 // Default: 0.60, 0.30, 0.30, 1.00

//
// Main background colours, allowing different colors for the panadapters and 
// the VFO/meter bar.
// Writing with SHADE on a BACKGND should be visible,
// but need not be "alerting"
// METER is a special colour for data/ticks in the "meter" surface
//
// MENU_BACKGND *was* used for all menus, the top window, the zoom/pan and slider area.
// howewer, meanwhile this is de-activated since the background color must be compatible
// with the GTK theme the user chose.
//

#define COLOUR_MENU_BACKGND  1.00, 1.00, 0.95, 1.00 // Default: 1.00, 1.00, 1.00, 1.00 
#define COLOUR_PAN_BACKGND   0.15, 0.15, 0.15, 1.00 // Default: 0.00, 0.00, 0.00, 1.00
#define COLOUR_VFO_BACKGND   0.15, 0.15, 0.15, 1.00 // Default: 0.00, 0.00, 0.00, 1.00
#define COLOUR_SHADE         0.70, 0.70, 0.70, 1.00 // Default: 0.70, 0.70, 0.70, 1.00
#define COLOUR_METER         1.00, 1.00, 1.00, 1.00 // Default: 1.00, 1.00, 1.00, 1.00

//
// Settings for a coloured (gradient) spectrum, only availabe for RX.
// The first and last colour are also used for the digital S-meter bar graph
//

#define COLOUR_GRAD1         0.00, 1.00, 0.00, 1.00 // Default: 0.00, 1.00, 0.00, 1.0
#define COLOUR_GRAD2         1.00, 0.66, 0.00, 1.00 // Default: 1.00, 0.66, 0.00, 1.00
#define COLOUR_GRAD3         1.00, 1.00, 0.00, 1.00 // Default: 1.00, 1.00, 0.00, 1.00
#define COLOUR_GRAD4         1.00, 0.00, 0.00, 1.00 // Default: 1.00, 0.00, 0.00, 1.00
#define COLOUR_GRAD1_WEAK    0.00, 0.50, 0.00, 1.00 // Default: 0.00, 0.50, 0.00, 1.00
#define COLOUR_GRAD2_WEAK    0.50, 0.33, 0.00, 1.00 // Default: 0.50, 0.33, 0.00, 1.00
#define COLOUR_GRAD3_WEAK    0.50, 0.50, 0.00, 1.00 // Default: 0.50, 0.50, 0.00, 1.00
#define COLOUR_GRAD4_WEAK    0.50, 0.00, 0.00, 1.00 // Default: 0.50, 0.00, 0.00, 1.00

//
// Settings for a "black and white" spectrum (not the TX spectrum is always B&W).
//
// FILL1 is used for a filled spectrum of a non-active receiver
// FILL2 is used for a filled spectrum of an active receiver,
//           and for a line spectrum of a non-active receiver
// FILL3 is used for a line spectrum of an active receiver
//

#define COLOUR_PAN_FILL1     1.00, 1.00, 1.00, 0.25 // Default: 1.00, 1.00, 1.00, 0.25
#define COLOUR_PAN_FILL2     1.00, 1.00, 1.00, 0.50 // Default: 1.00, 1.00, 1.00, 0.50
#define COLOUR_PAN_FILL3     1.00, 1.00, 1.00, 0.75 // Default: 1.00, 1.00, 1.00, 0.75
