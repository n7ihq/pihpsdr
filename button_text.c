/* Copyright (C)
* 2016 - John Melton, G0ORX/N6LYT
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

#include <gtk/gtk.h>

#include "message.h"

//
// set_button_default_color:
// The actual text color of the button is recorded and stored
// in the static string "default_color".

// set_button_text_color:
// Set the color of a widget using a style provider.
// This is normally used for button texts.
// If the color equals "default", then the color stored
// in default_color is used.
//
// The idea of this is that it works both with a normal
// (default = black) and dark (default = white) GTK theme.
//
// There is still an implicit assumption in piHPSDR, namely
// that orange and red button texts are compatible with the
// GTK theme.
//
// TODO: change this to adding and removing classes.
//

static char default_color[128] = "green";

void set_button_default_color(GtkWidget *widget) {
  GtkStyleContext *style_context;
  GdkRGBA RGBA;
  int alpha1, alpha2;
  //
  // Call this on the very first button that is generated.
  // The colour is probed and remembered since it depends on the GTK theme in use.
  // From then on, always use default_color when "default" is requested
  // This way, piHPSDR works with the "dark" GTK theme.
  //
  style_context = gtk_widget_get_style_context(widget);
  gtk_style_context_get_color(style_context, GTK_STATE_FLAG_NORMAL, &RGBA);
  //
  // This is now really odd. Some systems with national language support
  // (e.g. in Germany) print floating point constants with a decimal comma
  // rather than a decimal point, and this cannot be digested by GTK.
  // Therefore we build the floating point constant manually.
  alpha1 = 0;
  alpha2 = 1000 * RGBA.alpha;

  if (alpha2 > 999) {
    alpha1 = 1;
    alpha2 = 0;
  }

  g_snprintf(default_color, sizeof(default_color), "rgba(%d,%d,%d,%d.%03d)",
             (int) (RGBA.red   * 255),
             (int) (RGBA.green * 255),
             (int) (RGBA.blue  * 255),
             alpha1, alpha2);
  t_print("Default Color Probed: %s\n", default_color);
}

void set_button_text_color(GtkWidget *widget, char *color) {
  GtkStyleContext *style_context;
  GtkCssProvider *provider = gtk_css_provider_new ();
  gchar tmp[64];
  style_context = gtk_widget_get_style_context(widget);

  //
  // Replace "default" by the default color
  //
  if (!strcmp(color, "default")) {
    color = default_color;
  }

  //
  // Add the (up to now) empty provider to the style context
  //
  gtk_style_context_add_provider(style_context, GTK_STYLE_PROVIDER(provider), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

  //
  // Load color into the provider
  //
  if (gtk_minor_version >= 20)  {
    g_snprintf(tmp, sizeof tmp, "button, label { color: %s; }", color);
  } else {
    g_snprintf(tmp, sizeof tmp, "GtkButton, GtkLabel { color: %s; }", color);
  }

  gtk_css_provider_load_from_data(GTK_CSS_PROVIDER(provider), tmp, -1, NULL);
  g_object_unref (provider);
}

