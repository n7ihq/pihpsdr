#include <gtk/gtk.h>

//
// Set the color of a widget using a style provider.
// This is normally used for button texts.
// Upon the first call, the actual color is queried
// and stored, and assumed to be the default color for
// the GTK theme actually in use.
//
// Later, if the argument "color" equals "default", then
// this default color is used.
//
// The idea of this is that it works both with a normal
// (default = black) and dark (default = white) GTK theme.
//
// There is still an implicit assumption in piHPSDR, namely
// that orange and red button texts are compativle with the
// GTK theme.
//

void set_button_text_color(GtkWidget *widget,char *color) {
  GtkStyleContext *style_context;
  GtkCssProvider *provider = gtk_css_provider_new ();
  gchar tmp[64];

  GdkRGBA RGBA;
  static int first=1;
  static char default_color[128];

  style_context = gtk_widget_get_style_context(widget);

  if (first) {
    int alpha1,alpha2;
    //
    // In the very first call to this function, probe the button
    // colour since this is the default one for the GTK theme in use.
    // From then on, always use default_color when "default" is requested
    // This way, piHPSDR works with the "dark" GTK theme.
    //
    gtk_style_context_get_color(style_context, GTK_STATE_FLAG_NORMAL ,&RGBA);
    first=0;
    //
    // This is now really odd. Some systems with national language support
    // (e.g. in Germany) print floating point constants with a decimal comma
    // rather than a decimal point, and this cannot be digested by GTK.
    // Therefore we build the floating point constant manually.
    alpha1=0;
    alpha2=1000*RGBA.alpha;
    if (alpha2 > 999) {
      alpha1 = 1;
      alpha2 = 0;
    }
    g_snprintf(default_color, sizeof(default_color), "rgba(%d,%d,%d,%d.%03d)",
		    (int) (RGBA.red   * 255),
		    (int) (RGBA.green * 255),
		    (int) (RGBA.blue  * 255),
		    alpha1,alpha2);
    g_print("Default Color Probed: %s\n",default_color);
  }  

  //
  // Replace "default" by the default color
  //
  if (!strcmp(color,"default")) {
    color = default_color;
  }

  //
  // Add the (up to now) empty provider to the style context
  //
  gtk_style_context_add_provider(style_context, GTK_STYLE_PROVIDER(provider), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

  //
  // Load color into the provider
  //
  if(gtk_minor_version>=20)  {
    g_snprintf(tmp, sizeof tmp, "button, label { color: %s; }", color);
  } else {
    g_snprintf(tmp, sizeof tmp, "GtkButton, GtkLabel { color: %s; }", color);
  }
  gtk_css_provider_load_from_data(GTK_CSS_PROVIDER(provider), tmp, -1, NULL);
  g_object_unref (provider);
}

