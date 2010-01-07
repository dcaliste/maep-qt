/*
 * Copyright (C) 2009-2010 Till Harbaum <till@harbaum.org>.
 *
 * This file is part of Maep.
 *
 * Maep is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Maep is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Maep.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"
#include "misc.h"
#include "icon.h"
#include "about.h"

#ifndef MAEMO5
#define LINK_COLOR "blue"
#else
#include <hildon/hildon-pannable-area.h>
#define LINK_COLOR "lightblue"
#endif

#ifdef ENABLE_BROWSER_INTERFACE
#include <strings.h>
#ifndef USE_MAEMO
#include <libgnome/gnome-url.h>
#else
#include <tablet-browser-interface.h>
#endif
#endif

#ifdef ENABLE_BROWSER_INTERFACE
#ifndef USE_MAEMO
static void browser_url(char *url) {
  /* taken from gnome-open, part of libgnome */
  GError *err = NULL;
  gnome_url_show(url, &err);
}
#else
static void browser_url(osso_context_t *osso_context, char *url) {
  osso_rpc_run_with_defaults(osso_context, "osso_browser",
			     OSSO_BROWSER_OPEN_NEW_WINDOW_REQ, NULL,
			     DBUS_TYPE_STRING, url,
			     DBUS_TYPE_BOOLEAN, FALSE, DBUS_TYPE_INVALID);
}
#endif

static gboolean on_link_clicked(GtkWidget *widget, GdkEventButton *event,
				gpointer user_data) {

  const char *str = 
    gtk_label_get_text(GTK_LABEL(gtk_bin_get_child(GTK_BIN(widget))));
  
  browser_url(
#ifdef USE_MAEMO
	      (osso_context_t *)user_data,
#endif
	      (char*)str);  

  return TRUE;
}
#endif

static GtkWidget *link_new(GtkWidget *parent, const char *url) {
#ifdef ENABLE_BROWSER_INTERFACE
  if(!strncasecmp(url, "http://", 7)) {
    GtkWidget *label = gtk_label_new("");
    char *str = g_strdup_printf("<span color=\"" LINK_COLOR 
				"\"><u>%s</u></span>", url);
    gtk_label_set_markup(GTK_LABEL(label), str);
    g_free(str);
    
    GtkWidget *eventbox = gtk_event_box_new();
    gtk_container_add(GTK_CONTAINER(eventbox), label);
    
    g_signal_connect(eventbox, "button-press-event", 
		     G_CALLBACK(on_link_clicked), 
#ifndef USE_MAEMO
		     NULL
#else
		     g_object_get_data(G_OBJECT(parent), "osso-context")
#endif
		     );

    return eventbox;
  }
#endif
  GtkWidget *label = gtk_label_new("");
  char *str = g_strdup_printf("<span color=\"" LINK_COLOR "\">%s</span>", url);
  gtk_label_set_markup(GTK_LABEL(label), str);
  g_free(str);
  return label;
}

#ifdef ENABLE_BROWSER_INTERFACE
void on_paypal_button_clicked(GtkButton *button, gpointer user_data) {
  browser_url(
#ifdef USE_MAEMO
	      (osso_context_t *)user_data,
#endif
	      "https://www.paypal.com/cgi-bin/webscr?"
	      "cmd=_s-xclick&hosted_button_id=7400558");
}
#endif

GtkWidget *label_big(char *str) {
  GtkWidget *label = gtk_label_new("");
  char *markup = 
    g_markup_printf_escaped("<span size='x-large'>%s</span>", str);
  gtk_label_set_markup(GTK_LABEL(label), markup);
  g_free(markup);
  return label;
}

GtkWidget *label_xbig(char *str) {
  GtkWidget *label = gtk_label_new("");
  char *markup = 
    g_markup_printf_escaped("<span size='xx-large'>%s</span>", str);
  gtk_label_set_markup(GTK_LABEL(label), markup);
  g_free(markup);
  return label;
}

static void  
on_label_realize(GtkWidget *widget, gpointer user_data)  {
  /* get parent size (which is a container) */
  gtk_widget_set_size_request(widget, widget->parent->allocation.width, -1);
}

GtkWidget *label_wrap(char *str) {
  GtkWidget *label = gtk_label_new(str);

  gtk_label_set_line_wrap_mode(GTK_LABEL(label), PANGO_WRAP_WORD);
  gtk_label_set_line_wrap(GTK_LABEL(label), TRUE);

  g_signal_connect(G_OBJECT(label), "realize",
		   G_CALLBACK(on_label_realize), NULL);

  return label;
}

GtkWidget *license_page_new(GtkWidget *parent) {
  char *name = find_file("COPYING");

  GtkWidget *label = label_wrap("");

  /* load license into buffer */
  FILE *file = fopen(name, "r");
  g_free(name);

  if(file) {
    fseek(file, 0l, SEEK_END);
    int flen = ftell(file);
    fseek(file, 0l, SEEK_SET);

    char *buffer = g_malloc(flen+1);
    if(fread(buffer, 1, flen, file) != flen)
      gtk_label_set_text(GTK_LABEL(label), _("Load error"));
    else {
      buffer[flen]=0;
      gtk_label_set_text(GTK_LABEL(label), buffer);
    }

    fclose(file);
    g_free(buffer);
  } else
    gtk_label_set_text(GTK_LABEL(label), _("Open error"));

#ifndef MAEMO5
  GtkWidget *scrolled_window = gtk_scrolled_window_new(NULL, NULL);
  gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled_window), 
  				 GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
  gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(scrolled_window), 
					label);
  gtk_scrolled_window_set_shadow_type( GTK_SCROLLED_WINDOW(scrolled_window),
				       GTK_SHADOW_IN);
  return scrolled_window;
#else
  GtkWidget *pannable_area = hildon_pannable_area_new();
  hildon_pannable_area_add_with_viewport(HILDON_PANNABLE_AREA(pannable_area), 
					label);
  return pannable_area;
#endif
}

GtkWidget *copyright_page_new(GtkWidget *parent) {
  GtkWidget *vbox = gtk_vbox_new(FALSE, 0);

  /* ------------------------ */
  GtkWidget *ivbox = gtk_vbox_new(FALSE, 0);

  GtkWidget *hbox = gtk_hbox_new(FALSE, 0);
  GtkWidget *ihbox = gtk_hbox_new(FALSE, 20);
  gtk_box_pack_start(GTK_BOX(ihbox), 
#ifdef MAEMO5
		     icon_get(ihbox, "maep.64"),
#else
		     icon_get(ihbox, "maep.32"),
#endif
		     FALSE, FALSE, 0);

  gtk_box_pack_start(GTK_BOX(ihbox), label_xbig("Mæp"), 
		     FALSE, FALSE, 0);

  gtk_box_pack_start(GTK_BOX(hbox), ihbox, TRUE, FALSE, 0);
  gtk_box_pack_start_defaults(GTK_BOX(ivbox), hbox);

  gtk_box_pack_start_defaults(GTK_BOX(ivbox), 
		      label_big(_("A simple tile map")));

  gtk_box_pack_start(GTK_BOX(vbox), ivbox, TRUE, FALSE, 0);

  /* ------------------------ */
  ivbox = gtk_vbox_new(FALSE, 0);

  gtk_box_pack_start(GTK_BOX(ivbox), 
		      gtk_label_new("Version " VERSION), FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(ivbox), 
		      gtk_label_new(__DATE__ " " __TIME__), FALSE, FALSE, 0);

  gtk_box_pack_start(GTK_BOX(vbox), ivbox, TRUE, FALSE, 0);

  /* ------------------------ */
  ivbox = gtk_vbox_new(FALSE, 0);

  gtk_box_pack_start(GTK_BOX(ivbox), 
	      gtk_label_new(_("Copyright 2009-2010")), FALSE, FALSE, 0);

  gtk_box_pack_start(GTK_BOX(ivbox), 
	     link_new(parent, "http://www.harbaum.org/till/maemo#maep"),
	     FALSE, FALSE, 0);

  gtk_box_pack_start(GTK_BOX(vbox), ivbox, TRUE, FALSE, 0);

  return vbox;
}

/* a label that is left aligned */
GtkWidget *left_label(char *str) {
  GtkWidget *widget = gtk_label_new(str);
  gtk_misc_set_alignment(GTK_MISC(widget), 0.0f, 0.5f);
  return widget;
}

static void author_add(GtkWidget *box, char *str) {
  gtk_box_pack_start(GTK_BOX(box), left_label(str), FALSE, FALSE, 0);
}

GtkWidget *authors_page_new(GtkWidget *parent) {
  GtkWidget *ivbox, *vbox = gtk_vbox_new(FALSE, 16);

  /* -------------------------------------------- */
  ivbox = gtk_vbox_new(FALSE, 0);
  author_add(ivbox, _("Main developers:"));
  author_add(ivbox, "Till Harbaum <till@harbaum.org>");
  gtk_box_pack_start(GTK_BOX(vbox), ivbox, TRUE, FALSE, 0);

  /* -------------------------------------------- */
  ivbox = gtk_vbox_new(FALSE, 0);
  author_add(ivbox, _("Original map widget by:"));
  author_add(ivbox, "John Stowers <john.stowers@gmail.com>");
  author_add(ivbox, "Marcus Bauer <marcus.bauer@gmail.com>"),
  gtk_box_pack_start(GTK_BOX(vbox), ivbox, TRUE, FALSE, 0);

#ifndef MAEMO5
  GtkWidget *scrolled_window = gtk_scrolled_window_new(NULL, NULL);
  gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled_window), 
  				 GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
  gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(scrolled_window), 
					vbox);
  gtk_scrolled_window_set_shadow_type( GTK_SCROLLED_WINDOW(scrolled_window),
				       GTK_SHADOW_IN);
  return scrolled_window;
#else
  GtkWidget *pannable_area = hildon_pannable_area_new();
  hildon_pannable_area_add_with_viewport(HILDON_PANNABLE_AREA(pannable_area), 
					vbox);
  return pannable_area;
#endif
}  

GtkWidget *donate_page_new(GtkWidget *parent) {
  GtkWidget *vbox = gtk_vbox_new(FALSE, 0);

  gtk_box_pack_start_defaults(GTK_BOX(vbox), 
      label_wrap(_("If you like Mæp and want to support its future development "
		   "please consider donating to the developer. You can either "
		   "donate via paypal to")));
  
  gtk_box_pack_start_defaults(GTK_BOX(vbox), 
		      link_new(parent, "till@harbaum.org"));
  
#ifdef ENABLE_BROWSER_INTERFACE
  gtk_box_pack_start_defaults(GTK_BOX(vbox), 
      label_wrap(_("or you can just click the button below which will open "
		   "the appropriate web page in your browser.")));

  GtkWidget *ihbox = gtk_hbox_new(FALSE, 0);
  GtkWidget *button = gtk_button_new();
  gtk_button_set_image(GTK_BUTTON(button), 
#ifdef MAEMO5
		     icon_get(ihbox, "paypal.64")
#else
		     icon_get(ihbox, "paypal.32")
#endif
		       );
  gtk_button_set_relief(GTK_BUTTON(button), GTK_RELIEF_NONE);
  g_signal_connect(button, "clicked", 
		   G_CALLBACK(on_paypal_button_clicked), 
#ifndef USE_MAEMO
		   NULL
#else
		   g_object_get_data(G_OBJECT(parent), "osso-context")
#endif
		   );
 
  gtk_box_pack_start(GTK_BOX(ihbox), button, TRUE, FALSE, 0);
  gtk_box_pack_start_defaults(GTK_BOX(vbox), ihbox);
#endif

  return vbox;
}  

GtkWidget *bugs_page_new(GtkWidget *parent) {
  GtkWidget *vbox = gtk_vbox_new(FALSE, 0);

  gtk_box_pack_start_defaults(GTK_BOX(vbox), 
      label_wrap(_("Please report bugs or feature requests via the Mæp "
		   "bug tracker. This bug tracker can directly be reached via "
		   "the following link:")));

  gtk_box_pack_start_defaults(GTK_BOX(vbox), 
      link_new(parent, "http://garage.maemo.org/tracker/?group_id=1150"));

  gtk_box_pack_start_defaults(GTK_BOX(vbox), 
      label_wrap(_("You might also be interested in joining the mailing lists "
		   "or the forum:")));

  gtk_box_pack_start_defaults(GTK_BOX(vbox), 
      link_new(parent, "http://garage.maemo.org/projects/maep/"));

  gtk_box_pack_start_defaults(GTK_BOX(vbox), 
      label_wrap(_("Thank you for contributing!")));

  return vbox;
}  

void about_box(GtkWidget *parent) {
  GtkWidget *dialog = gtk_dialog_new_with_buttons(_("About Mæp"),
	  GTK_WINDOW(parent), GTK_DIALOG_MODAL,
          GTK_STOCK_CLOSE, GTK_RESPONSE_CLOSE, NULL);

#ifdef USE_MAEMO
  gtk_window_set_default_size(GTK_WINDOW(dialog), 640, 480);
#else
  gtk_window_set_default_size(GTK_WINDOW(dialog), 400, 200);
#endif

  GtkWidget *notebook = notebook_new();

  notebook_append_page(notebook, copyright_page_new(parent), _("Copyright"));
  notebook_append_page(notebook, license_page_new(parent),   _("License"));
  notebook_append_page(notebook, authors_page_new(parent),   _("Authors"));
  notebook_append_page(notebook, donate_page_new(parent),    _("Donate"));
  notebook_append_page(notebook, bugs_page_new(parent),      _("Bugs"));

  gtk_box_pack_start_defaults(GTK_BOX((GTK_DIALOG(dialog))->vbox),
			      notebook);

  gtk_widget_show_all(dialog);

  gtk_dialog_run(GTK_DIALOG(dialog));
  gtk_widget_destroy(dialog);
}
