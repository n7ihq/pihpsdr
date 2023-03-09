#include <gtk/gtk.h>
#include "appearance.h"

void set_button_text_color(GtkWidget *widget,char *color) {
  GtkStyleContext *style_context;
  GtkCssProvider *provider = gtk_css_provider_new ();
  gchar tmp[64];

#ifdef ALLOW_DARK_THEME  
  GdkRGBA RGBA;
  static int first=1;
  static char default_color[128];
#endif  

  style_context = gtk_widget_get_style_context(widget);

#ifdef ALLOW_DARK_THEME  
  if (first) {
    int alpha1,alpha2;
    //
    // In the very first call to this function, probe the button
    // colour since this is the default one for the GTK theme in use.
    // From then on, always use default_color when "black" is requested
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
      alpha1=1;
      alpha2 = alpha2 - 1000;
    }
    g_snprintf(default_color, sizeof(default_color), "rgba(%d,%d,%d,%d.%03d)",
		    (int) (RGBA.red   * 255),
		    (int) (RGBA.green * 255),
		    (int) (RGBA.blue  * 255),
		    alpha1,alpha2);
    g_print("Default Color Probed: %s\n",default_color);
  }  
#endif  
  gtk_style_context_add_provider(style_context, GTK_STYLE_PROVIDER(provider), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

#ifdef ALLOW_DARK_THEME  
  if (!strcmp(color,"black")) {
    color = default_color;
  }
#endif  

  if(gtk_minor_version>=20)  {
    g_snprintf(tmp, sizeof tmp, "button, label { color: %s; }", color);
  } else {
    g_snprintf(tmp, sizeof tmp, "GtkButton, GtkLabel { color: %s; }", color);
  }
  gtk_css_provider_load_from_data(GTK_CSS_PROVIDER(provider), tmp, -1, NULL);
  g_object_unref (provider);
}

