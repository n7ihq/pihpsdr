/* Copyright (C)
* 2015 - John Melton, G0ORX/N6LYT
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

#ifndef _MAIN_H
#define _MAIN_H

#include <sys/utsname.h>
extern struct utsname unameData;

enum {
  NO_CONTROLLER = 0,
  CONTROLLER1,
  CONTROLLER2_V1,
  CONTROLLER2_V2,
  G2_FRONTPANEL
};

extern gint controller;

extern GdkScreen *screen;
extern gint display_width;
extern gint display_height;
extern gint screen_width;
extern gint screen_height;
extern gint this_monitor;

extern gint full_screen;
extern GtkWidget *top_window;
extern GtkWidget *topgrid;
extern void status_text(const char *text);

extern gboolean keypress_cb(GtkWidget *widget, GdkEventKey *event, gpointer data);
#endif
