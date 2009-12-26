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

#include <gtk/gtk.h>

#include "config.h"
#include "about.h"

typedef enum { MENU_ENTRY, MENU_CHECK_ENTRY, 
	       MENU_SEPARATOR, MENU_END } menu_type_t;

typedef struct menu_entry_s {
  menu_type_t type;
  char *title;
  void(*cb)(GtkWidget *, gpointer);
} menu_entry_t;

static void 
cb_menu_about(GtkWidget *item, gpointer data) {
  about_box(gtk_widget_get_toplevel(GTK_WIDGET(item)));
}

static const menu_entry_t main_menu[] = {
  { MENU_ENTRY,           "About",         cb_menu_about },
  { MENU_SEPARATOR,       NULL,            NULL },
  { MENU_CHECK_ENTRY,     "Capture Track", NULL },
  { MENU_ENTRY,           "Clear Track",   NULL },
  { MENU_ENTRY,           "Import Track",  NULL },
  { MENU_ENTRY,           "Export Track",  NULL },

  { MENU_END,             NULL,            NULL }
};

void menu_build(GtkWidget *menu, const menu_entry_t *entry) {

  while(entry->type != MENU_END) {
    GtkWidget *item = NULL;

    switch(entry->type) {
    case MENU_ENTRY:
      item = gtk_menu_item_new_with_label( _(entry->title) );
      gtk_menu_append(GTK_MENU_SHELL(menu), item);
      if(entry->cb)
	g_signal_connect(item, "activate", GTK_SIGNAL_FUNC(entry->cb), NULL);
      
      break;

    case MENU_SEPARATOR:
      gtk_menu_append(GTK_MENU_SHELL(menu), gtk_separator_menu_item_new());
      break;

    default:
      printf("unsupported menu type\n");
      break;
    }

    entry++;
  }
}

GtkWidget *menu_create(GtkWidget *window) {

  printf("creating menu\n");

  GtkWidget *menu = gtk_menu_new();
  menu_build(menu, main_menu);

#ifdef USE_MAEMO
  hildon_window_set_menu(HILDON_WINDOW(window), GTK_MENU(menu));
  return window;
#else
  /* attach ordinary gtk menu */
  GtkWidget *menu_bar = gtk_menu_bar_new();

  GtkWidget *root_menu = gtk_menu_item_new_with_label( _("Menu") );
  gtk_widget_show(root_menu);
  gtk_menu_bar_append(menu_bar, root_menu); 
  gtk_menu_item_set_submenu(GTK_MENU_ITEM (root_menu), menu);

  gtk_widget_show(menu_bar);

  GtkWidget *vbox = gtk_vbox_new(FALSE, 0);
  gtk_container_add(GTK_CONTAINER(window), vbox);
  gtk_box_pack_start(GTK_BOX(vbox), menu_bar, FALSE, FALSE, 0);

  GtkWidget *bin = gtk_event_box_new();
  gtk_box_pack_start_defaults(GTK_BOX(vbox), bin);

  return bin;
#endif 
}
