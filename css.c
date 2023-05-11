#include <gtk/gtk.h>
#include "css.h"

//
// Unless we want a special effect, do not specify colors so
// we inherit them from the GTK them.
//
// Here, only the color for the "selected" state
// (white text on a red button) is specified
// 
//
char *css=
"  @define-color TOGGLE_ON  rgb(100%,0%,0%);\n"
"  @define-color COLOR_ON   rgb(100%,100%,100%);\n"
"  #small_button {\n"
"    padding: 1px;\n"
"    font-family: Sans;\n"
"    font-size: 15px;\n"
"    }\n"
"  #small_toggle_button {\n"
"    padding: 1px;\n"
"    font-family: Sans;\n"
"    font-size: 15px;\n"
"    background-image: none;\n"
"    }\n"
"  #small_toggle_button:checked {\n"
"    padding: 1px;\n"
"    font-family: Sans;\n"
"    font-size: 15px;\n"
"    background-image: none;\n"           // if checked, white text on red background
"    background-color: @TOGGLE_ON;\n"
"    color: @COLOR_ON;\n"
"    }\n"

;

void load_css() {
  GtkCssProvider *provider;
  GdkDisplay *display;
  GdkScreen *screen;

  g_print("%s\n",__FUNCTION__);
  provider = gtk_css_provider_new ();
  display = gdk_display_get_default ();
  screen = gdk_display_get_default_screen (display);
  gtk_style_context_add_provider_for_screen (screen,
                                             GTK_STYLE_PROVIDER(provider),
                                             GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

  gtk_css_provider_load_from_data(provider, css, -1, NULL);
  g_object_unref (provider);
}
