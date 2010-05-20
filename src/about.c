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
#include <hildon/hildon-text-view.h>
#include <hildon/hildon-window-stack.h>
#include <hildon/hildon-gtk.h>
#define LINK_COLOR "lightblue"
#endif

#ifdef ENABLE_BROWSER_INTERFACE
static gboolean on_link_clicked(GtkWidget *widget, GdkEventButton *event,
				gpointer user_data) {

  const char *str = 
    gtk_label_get_text(GTK_LABEL(gtk_bin_get_child(GTK_BIN(widget))));
  
  browser_url(GTK_WIDGET(user_data), (char*)str);  

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
		     G_CALLBACK(on_link_clicked), parent);

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
  browser_url(GTK_WIDGET(user_data),
	      "https://www.paypal.com/cgi-bin/webscr?"
	      "cmd=_s-xclick&hosted_button_id=7400558");
}
#endif

static GtkWidget *label_big(char *str) {
  GtkWidget *label = gtk_label_new("");
  char *markup = 
    g_markup_printf_escaped("<span size='x-large'>%s</span>", str);
  gtk_label_set_markup(GTK_LABEL(label), markup);
  g_free(markup);
  return label;
}

static GtkWidget *label_xbig(char *str) {
  GtkWidget *label = gtk_label_new("");
  char *markup = 
    g_markup_printf_escaped("<span size='xx-large'>%s</span>", str);
  gtk_label_set_markup(GTK_LABEL(label), markup);
  g_free(markup);
  return label;
}

static GtkWidget *text_wrap(char *str) {
  GtkTextBuffer *buffer = gtk_text_buffer_new(NULL);

#ifndef MAEMO5
  GtkWidget *view = gtk_text_view_new_with_buffer(buffer);
#else
  GtkWidget *view = hildon_text_view_new();
  hildon_text_view_set_buffer(HILDON_TEXT_VIEW(view), buffer);
#endif

  gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(view), GTK_WRAP_WORD);
  gtk_text_view_set_editable(GTK_TEXT_VIEW(view), FALSE);
  gtk_text_view_set_cursor_visible(GTK_TEXT_VIEW(view), FALSE);
  gtk_text_view_set_justification(GTK_TEXT_VIEW(view), GTK_JUSTIFY_LEFT); 

  gtk_text_buffer_set_text(buffer, str, -1);

#ifdef MAEMO5
  /* in fremantle this is really tricky and we need to inherit the */
  /* style from the topmost window in the stack */
  HildonWindowStack *stack = hildon_window_stack_get_default();
  GList *list = hildon_window_stack_get_windows(stack);
  gtk_widget_set_style(view, GTK_WIDGET(list->data)->style);
  g_list_free(list);
#else
  GtkStyle *style = gtk_widget_get_style(view);
  gtk_widget_modify_base(view, GTK_STATE_NORMAL, &style->bg[GTK_STATE_NORMAL]);
#endif 

  return view;
}

static void text_set(GtkWidget *view, char *str) {
  GtkTextBuffer *buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(view));
  gtk_text_buffer_set_text(buffer, str, -1);
}


GtkWidget *license_page_new(GtkWidget *parent) {
  char *name = find_file("COPYING");

  GtkWidget *label = text_wrap("");

  /* load license into buffer */
  FILE *file = fopen(name, "r");
  g_free(name);

  if(file) {
    fseek(file, 0l, SEEK_END);
    int flen = ftell(file);
    fseek(file, 0l, SEEK_SET);

    char *buffer = g_malloc(flen+1);
    if(fread(buffer, 1, flen, file) != flen)
      text_set(label, _("Load error"));
    else {
      buffer[flen]=0;
      text_set(label, buffer);
    }

    fclose(file);
    g_free(buffer);
  } else
    text_set(label, _("Open error"));

#ifndef MAEMO5
  GtkWidget *scrolled_window = gtk_scrolled_window_new(NULL, NULL);
  gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled_window), 
  				 GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
  gtk_container_add(GTK_CONTAINER(scrolled_window), label);
  gtk_scrolled_window_set_shadow_type( GTK_SCROLLED_WINDOW(scrolled_window),
				       GTK_SHADOW_IN);
  return scrolled_window;
#else
  GtkWidget *pannable_area = hildon_pannable_area_new();
  gtk_container_add(GTK_CONTAINER(pannable_area), label);
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
		     icon_get_widget(ihbox, "maep.64"),
#else
		     icon_get_widget(ihbox, "maep.32"),
#endif
		     FALSE, FALSE, 0);

  gtk_box_pack_start(GTK_BOX(ihbox), label_xbig("Mæp"), 
		     FALSE, FALSE, 0);

  gtk_box_pack_start(GTK_BOX(hbox), ihbox, TRUE, FALSE, 0);
  gtk_box_pack_start_defaults(GTK_BOX(ivbox), hbox);

  gtk_box_pack_start_defaults(GTK_BOX(ivbox), 
	      label_big(_("A small and fast tile map")));

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

static void link_add(GtkWidget *box, GtkWidget *parent, char *str) {
  GtkWidget *link = link_new(parent, str);
  if(GTK_IS_LABEL(link))
    gtk_misc_set_alignment(GTK_MISC(link), 0.0f, 0.5f);
  else
    gtk_misc_set_alignment(GTK_MISC(gtk_bin_get_child(GTK_BIN(link))), 
			   0.0f, 0.5f);

  gtk_box_pack_start(GTK_BOX(box), link, FALSE, FALSE, 0);
}

GtkWidget *authors_page_new(GtkWidget *parent) {
  GtkWidget *ivbox, *vbox = gtk_vbox_new(FALSE, 16);

  /* -------------------------------------------- */
  ivbox = gtk_vbox_new(FALSE, 0);
  author_add(ivbox, _("Main developer:"));
  author_add(ivbox, "Till Harbaum <till@harbaum.org>");
  gtk_box_pack_start(GTK_BOX(vbox), ivbox, TRUE, FALSE, 0);

  /* -------------------------------------------- */
  ivbox = gtk_vbox_new(FALSE, 0);
  author_add(ivbox, _("Original osm-gps-map widget by:"));
  author_add(ivbox, "John Stowers <john.stowers@gmail.com>");
  author_add(ivbox, "Marcus Bauer <marcus.bauer@gmail.com>"),
  gtk_box_pack_start(GTK_BOX(vbox), ivbox, TRUE, FALSE, 0);

  /* -------------------------------------------- */
  ivbox = gtk_vbox_new(FALSE, 0);
  author_add(ivbox, _("Patches/bug fixes by:"));
  author_add(ivbox, "Roman Kapusta");
  author_add(ivbox, "Charles Clément");
  gtk_box_pack_start(GTK_BOX(vbox), ivbox, TRUE, FALSE, 0);

  /* -------------------------------------------- */
  ivbox = gtk_vbox_new(FALSE, 0);
  author_add(ivbox, _("Search and wikipedia feature provided by:"));
  link_add(ivbox, parent, _("http://geonames.org"));
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
      text_wrap(_("If you like Mæp and want to support its future development "
	        "please consider donating to the developer. You can either "
	        "donate via paypal to")));
  
  gtk_box_pack_start_defaults(GTK_BOX(vbox), 
		      link_new(parent, "till@harbaum.org"));
  
#ifdef ENABLE_BROWSER_INTERFACE
  gtk_box_pack_start_defaults(GTK_BOX(vbox), 
      text_wrap(_("or you can just click the button below which will open "
		   "the appropriate web page in your browser.")));

  GtkWidget *ihbox = gtk_hbox_new(FALSE, 0);
  GtkWidget *button = gtk_button_new();
  gtk_button_set_image(GTK_BUTTON(button), 
#ifdef MAEMO5
		     icon_get_widget(ihbox, "paypal.64")
#else
		     icon_get_widget(ihbox, "paypal.32")
#endif
		       );
  gtk_button_set_relief(GTK_BUTTON(button), GTK_RELIEF_NONE);
  g_signal_connect(button, "clicked", 
		   G_CALLBACK(on_paypal_button_clicked), parent);
 
  gtk_box_pack_start(GTK_BOX(ihbox), button, TRUE, FALSE, 0);
  gtk_box_pack_start_defaults(GTK_BOX(vbox), ihbox);
#endif

  return vbox;
}  

GtkWidget *bugs_page_new(GtkWidget *parent) {
  GtkWidget *vbox = gtk_vbox_new(FALSE, 0);

  gtk_box_pack_start_defaults(GTK_BOX(vbox), 
      text_wrap(_("Please report bugs or feature requests via the Mæp "
		   "bug tracker. This bug tracker can directly be reached via "
		   "the following link:")));

  gtk_box_pack_start_defaults(GTK_BOX(vbox), 
      link_new(parent, "http://garage.maemo.org/tracker/?group_id=1155"));

  gtk_box_pack_start_defaults(GTK_BOX(vbox), 
      text_wrap(_("You might also be interested in joining the mailing lists "
		   "or the forum:")));

  gtk_box_pack_start_defaults(GTK_BOX(vbox), 
      link_new(parent, "http://garage.maemo.org/projects/maep/"));

  gtk_box_pack_start_defaults(GTK_BOX(vbox), 
      text_wrap(_("Thank you for contributing!")));

  return vbox;
}  

void about_box(GtkWidget *parent) {
  GtkWidget *dialog = gtk_dialog_new_with_buttons(_("About Mæp"),
	  GTK_WINDOW(parent), GTK_DIALOG_MODAL,
          GTK_STOCK_CLOSE, GTK_RESPONSE_CLOSE, NULL);

#ifdef USE_MAEMO
#ifndef MAEMO5
  gtk_window_set_default_size(GTK_WINDOW(dialog), 640, 480);
#else
  gtk_window_set_default_size(GTK_WINDOW(dialog), 800, 800);
#endif
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
