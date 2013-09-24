/*
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

/* This file implements menus for all three supported plattforms:
   - plain gtk+
   - maemo chinook/diablo
   - maemo fremantle
*/

#include <gtk/gtk.h>

#include "config.h"
#include "about.h"
#include "track.h"
#include "geonames.h"
#include "misc.h"
#include "osm-gps-map-osd-classic.h"

extern void hxm_enable(GtkWidget *map, gboolean enable);

#include <string.h>

#ifdef MAEMO5
#include <hildon/hildon-button.h>
#include <hildon/hildon-check-button.h>
#endif

typedef enum { MENU_ENTRY, MENU_SUBMENU, MENU_CHECK_ENTRY, 
	       MENU_SEPARATOR, MENU_END } menu_type_t;

typedef struct menu_entry_s {
  menu_type_t type;
  char *title;
  union {
    void(*cb)(GtkWidget *, gpointer);
    const struct menu_entry_s *submenu;
  };
} menu_entry_t;

static gboolean menu_get_active(GtkWidget *item) {
#ifdef MAEMO5
  return hildon_check_button_get_active(HILDON_CHECK_BUTTON(item));
#else
  return gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(item));
#endif
}

static void 
cb_menu_about(GtkWidget *item, gpointer data) {
  GtkWidget *toplevel = gtk_widget_get_toplevel(GTK_WIDGET(data));
  about_box(toplevel);
}

static void 
cb_menu_search(GtkWidget *item, gpointer data) {
  geonames_enable_search(GTK_WIDGET(data));
}

static void 
cb_menu_wikipedia(GtkWidget *item, gpointer data) {
  geonames_enable_wikipedia(GTK_WIDGET(data), menu_get_active(item)); 
}

static void 
cb_menu_hr(GtkWidget *item, gpointer data) {
  hxm_enable(GTK_WIDGET(data), menu_get_active(item)); 
}

#ifdef MAEMO5
static void 
cb_menu_rotation(GtkWidget *item, gpointer data) {
  GtkWidget *toplevel = gtk_widget_get_toplevel(GTK_WIDGET(data));

  g_object_set_data(G_OBJECT(data), GCONF_KEY_SCREEN_ROTATE, 
		    (gpointer)menu_get_active(item));

  if(menu_get_active(item))  rotation_enable(toplevel);
  else                       rotation_disable(toplevel);
}
#endif

static void 
cb_menu_track_import(GtkWidget *item, gpointer data) {
  track_import(GTK_WIDGET(data));
}

static void 
cb_menu_track_clear(GtkWidget *item, gpointer data) {
  track_clear(GTK_WIDGET(data));
}

static void 
cb_menu_track_capture(GtkWidget *item, gpointer data) {
  track_capture_enable(GTK_WIDGET(data), menu_get_active(item)); 
}

static void 
cb_menu_track_export(GtkWidget *item, gpointer data) {
  track_export(GTK_WIDGET(data));
}

static void 
cb_menu_track_graph(GtkWidget *item, gpointer data) {
  track_graph(GTK_WIDGET(data));
}

static const menu_entry_t track_menu[] = {
  { MENU_CHECK_ENTRY, "Capture",    { .cb = cb_menu_track_capture } },
  { MENU_ENTRY,       "Clear",      { .cb = cb_menu_track_clear } } ,
  { MENU_ENTRY,       "Import",     { .cb = cb_menu_track_import } },
  { MENU_ENTRY,       "Export",     { .cb = cb_menu_track_export } },
  //  { MENU_ENTRY,       "Graph",      { .cb = cb_menu_track_graph } },

  { MENU_END,         NULL,        { NULL } }
};

static const menu_entry_t main_menu[] = {
  { MENU_ENTRY,       "About",     { .cb = cb_menu_about } },
  { MENU_SEPARATOR,   NULL,        { NULL } },
  { MENU_SUBMENU,     "Track",     { .submenu = track_menu } },
  { MENU_SEPARATOR,   NULL,        { NULL } },
  { MENU_ENTRY,       "Search",    { .cb = cb_menu_search } },
  { MENU_CHECK_ENTRY, "Wikipedia", { .cb = cb_menu_wikipedia } },
  { MENU_CHECK_ENTRY, "Heart Rate",{ .cb = cb_menu_hr } },
#ifdef MAEMO5
  { MENU_CHECK_ENTRY, "Screen Rotation",  { .cb = cb_menu_rotation } },
#endif
  { MENU_END,         NULL,        { NULL } }
};

#ifdef MAEMO5
void on_submenu_entry_clicked(GtkButton *button, gpointer user) {
  GtkWidget *toplevel = gtk_widget_get_toplevel(GTK_WIDGET(button));

  /* force closing of submenu dialog */
  gtk_dialog_response(GTK_DIALOG(toplevel), GTK_RESPONSE_NONE);
  gtk_widget_hide(toplevel);
  
  /* let gtk clean up */
  while(gtk_events_pending()) 
    gtk_main_iteration();
}

static int menu_entries(const menu_entry_t *menu) {
  int entries = 0;

  while(menu->type != MENU_END) {
    if(menu->type != MENU_SEPARATOR)
      entries++;

    menu++;
  }

  return entries;
}

/* MAEMO5: whenever a button linking to a submenu is destroyed, the linked */
/* menu itself will in turn be destroyed */
static void on_submenu_button_destroy(GtkWidget *button, gpointer data) {
  GtkWidget *menu = GTK_WIDGET(g_object_get_data(G_OBJECT(button), "submenu_dialog"));
  g_assert(menu);
  gtk_widget_destroy(menu);
}

/* MAEMO5: the user clicked a button linking to a submenu. display it! */
void on_submenu_clicked(GtkButton *button, gpointer user) {
  GtkWidget *menu = GTK_WIDGET(g_object_get_data(G_OBJECT(button), "submenu_dialog"));
  g_assert(menu);

  gtk_widget_show_all(menu);
  gtk_dialog_run(GTK_DIALOG(menu));
  gtk_widget_hide(menu);
}

static void maemo5_submenu_append(GtkWidget *table, GtkWidget *item) {
  int index = (int)g_object_get_data(G_OBJECT(table), "index");

  /* the submenu has two columns of buttons */
  int x = index % 2;
  int y = index / 2;

  gtk_table_attach_defaults(GTK_TABLE(table), item, x, x+1, y, y+1);
  g_object_set_data(G_OBJECT(table), "index", (gpointer)(index+1));
}

#endif // MAEMO5

void menu_build(GtkWidget *window, GtkWidget *map, GtkWidget *menu, 
		const menu_entry_t *entry, gboolean is_root) {

#ifdef MAEMO5
  GtkWidget *table = NULL;
  /* a submenu consits of a table containing two columns of buttons */
  if(!is_root)
    table = gtk_table_new(menu_entries(entry)/2, 2, TRUE);
#endif

  while(entry->type != MENU_END) {
    GtkWidget *item = NULL;

    switch(entry->type) {
    case MENU_ENTRY:
#ifndef MAEMO5
      item = gtk_menu_item_new_with_label( _(entry->title) );
      gtk_menu_append(GTK_MENU_SHELL(menu), item);
      if(entry->cb)
	g_signal_connect(item, "activate", GTK_SIGNAL_FUNC(entry->cb), map);
#else
      item = hildon_button_new_with_text(
	     HILDON_SIZE_FINGER_HEIGHT | HILDON_SIZE_AUTO_WIDTH,
	     HILDON_BUTTON_ARRANGEMENT_VERTICAL,
	     _(entry->title), NULL);

      g_signal_connect_after(item, "clicked", G_CALLBACK(entry->cb), map);
      
      if(is_root) 
	hildon_app_menu_append(HILDON_APP_MENU(menu), GTK_BUTTON(item));
      else {
	maemo5_submenu_append(table, item);
	g_signal_connect(item, "clicked", G_CALLBACK(on_submenu_entry_clicked), NULL); 
      }
      
#endif
      break;

    case MENU_CHECK_ENTRY:
#ifndef MAEMO5
      item = gtk_check_menu_item_new_with_label( _(entry->title) );
      gtk_menu_append(GTK_MENU_SHELL(menu), item);
      if(entry->cb)
	g_signal_connect(item, "toggled", GTK_SIGNAL_FUNC(entry->cb), map);
#else
      item = hildon_check_button_new(HILDON_SIZE_FINGER_HEIGHT | HILDON_SIZE_AUTO_WIDTH);
      gtk_button_set_label(GTK_BUTTON(item), _(entry->title));
      gtk_button_set_alignment(GTK_BUTTON(item), 0.5, 0.5);

      g_signal_connect_after(item, "clicked", G_CALLBACK(entry->cb), map);

      if(is_root)
	hildon_app_menu_append(HILDON_APP_MENU(menu), GTK_BUTTON(item));
      else {
	maemo5_submenu_append(table, item);
	g_signal_connect(item, "clicked", G_CALLBACK(on_submenu_entry_clicked), NULL); 
      }
#endif
      break;


    case MENU_SEPARATOR:
#ifndef MAEMO5
      gtk_menu_append(GTK_MENU_SHELL(menu), gtk_separator_menu_item_new());
#else
      /* no seperators used in maemo5 menus */
#endif
      break;

    case MENU_SUBMENU:
#ifndef MAEMO5
      item = gtk_menu_item_new_with_mnemonic( _(entry->title) );
      gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
      GtkWidget *submenu = gtk_menu_new();
      menu_build(window, map, submenu, entry->submenu, FALSE); 
      gtk_menu_item_set_submenu(GTK_MENU_ITEM(item), submenu);
#else
      item = hildon_button_new_with_text(
	     HILDON_SIZE_FINGER_HEIGHT | HILDON_SIZE_AUTO_WIDTH,
	     HILDON_BUTTON_ARRANGEMENT_VERTICAL,
	     _(entry->title), NULL);

      /* create an oridinary dialog box for the fremantle submenu */
      GtkWidget *submenu = gtk_dialog_new();
      gtk_window_set_title(GTK_WINDOW(submenu), _(entry->title));
      gtk_window_set_modal(GTK_WINDOW(submenu), TRUE);
      gtk_window_set_transient_for(GTK_WINDOW(submenu), GTK_WINDOW(window));
      gtk_dialog_set_has_separator(GTK_DIALOG(submenu), FALSE);
      
      menu_build(window, map, submenu, entry->submenu, FALSE); 

      g_object_set_data(G_OBJECT(item), "submenu_dialog", submenu);
      g_signal_connect_after(item, "clicked", G_CALLBACK(on_submenu_clicked), NULL);
      g_signal_connect(item, "destroy", G_CALLBACK(on_submenu_button_destroy), NULL);

      if(is_root) 
	hildon_app_menu_append(HILDON_APP_MENU(menu), GTK_BUTTON(item));
      else 
	maemo5_submenu_append(table, item);
#endif
      g_object_set_data(G_OBJECT(item), "submenu", submenu);
      break;

    default:
      g_warning(__FILE__ ": Unsupported menu entry type\n");
      break;
    }

    /* save item reference if requested */
    if(entry->title) 
      g_object_set_data(G_OBJECT(menu), entry->title, item);

    entry++;
  }

#ifdef MAEMO5
  if(!is_root)
    gtk_box_pack_start_defaults(GTK_BOX(GTK_DIALOG(menu)->vbox), table);
#endif
}

void menu_create(GtkWidget *vbox, GtkWidget *map) {
  GtkWidget *toplevel = gtk_widget_get_toplevel(GTK_WIDGET(vbox));

#ifdef MAEMO5
  HildonAppMenu *menu = HILDON_APP_MENU(hildon_app_menu_new());
  menu_build(toplevel, map, GTK_WIDGET(menu), main_menu, TRUE);
  g_object_set_data(G_OBJECT(toplevel), "menu", menu);
  gtk_widget_show_all(GTK_WIDGET(menu));
  hildon_window_set_app_menu(HILDON_WINDOW(toplevel), menu);
  return;
#else
  GtkWidget *menu = gtk_menu_new();
  menu_build(toplevel, map, menu, main_menu, TRUE);
  g_object_set_data(G_OBJECT(toplevel), "menu", menu);

#ifdef USE_MAEMO
  hildon_window_set_menu(HILDON_WINDOW(toplevel), GTK_MENU(menu));
  return;
#else
  /* attach ordinary gtk menu */
  GtkWidget *menu_bar = gtk_menu_bar_new();

  GtkWidget *root_menu = gtk_menu_item_new_with_label( _("Menu") );
  gtk_widget_show(root_menu);
  gtk_menu_bar_append(menu_bar, root_menu); 
  gtk_menu_item_set_submenu(GTK_MENU_ITEM (root_menu), menu);

  gtk_widget_show(menu_bar);

  gtk_box_pack_start(GTK_BOX(vbox), menu_bar, FALSE, FALSE, 0);

  return;
#endif
#endif 
}

static void menu_enable_cb(GtkWidget *item, gboolean enable) {
  gtk_widget_set_sensitive(item, enable);
}

static void menu_check_set_active_cb(GtkWidget *item, gboolean enable) {
#ifdef MAEMO5
  hildon_check_button_set_active(HILDON_CHECK_BUTTON(item), enable); 
#else
  gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(item), enable);
#endif
}

static void menu_item_do(GtkWidget *window, const char *id, gboolean enable,
			 void(*cb)(GtkWidget *, gboolean)) {
  GtkWidget *menu = g_object_get_data(G_OBJECT(window), "menu");
  g_assert(menu);

  char *slash = NULL;
  do {
    slash = strchr(id, '/');
    if(slash) {
      char *tmp = g_strndup(id, slash-id);

      GtkWidget *item = g_object_get_data(G_OBJECT(menu), tmp);
      g_assert(item);
      menu = g_object_get_data(G_OBJECT(item), "submenu");
      g_assert(menu);

      g_free(tmp);
      id = slash+1;
    } else {
      GtkWidget *item = g_object_get_data(G_OBJECT(menu), id);

      if(item)
	cb(item, enable);
      else 
	g_warning("Menu: item %s does not exist", id);
    }
  } while(slash);
}

void menu_enable(GtkWidget *window, const char *id, gboolean enable) {
  menu_item_do(window, id, enable, menu_enable_cb);
}

void menu_check_set_active(GtkWidget *window, const char *id, gboolean active) {
  menu_item_do(window, id, active, menu_check_set_active_cb);
}
