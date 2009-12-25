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

typedef enum { MENU_ENTRY, MENU_CHECK_ENTRY, 
	       MENU_SEPARATOR, MENU_END } menu_type_t;

typedef struct {
  menu_type_t type;
  char *title;
  /* callback */
} menu_entry_t;

static const menu_entry_t main_menu[] = {
  { MENU_ENTRY,           "About"         },
  { MENU_SEPARATOR,       NULL            },
  { MENU_CHECK_ENTRY,     "Capture Track" },
  { MENU_ENTRY,           "Clear Track"   },
  { MENU_ENTRY,           "Import Track"  },
  { MENU_ENTRY,           "Export Track"  },

  { MENU_END,             NULL            }
};

static void 
cb_menu_about(GtkMenuItem *item, gpointer data) {
}

void menu_build(GtkWidget *menu, const menu_entry_t *entry) {

  while(entry->type != MENU_END) {
    GtkWidget *item = NULL;

    switch(entry->type) {
    case MENU_ENTRY:
      item = gtk_menu_item_new_with_label( _(entry->title) );
      gtk_menu_append(GTK_MENU_SHELL(menu), item);
      break;

    case MENU_SEPARATOR:
      gtk_menu_append(GTK_MENU_SHELL(menu), gtk_separator_menu_item_new());
      break;

    default:
      printf("unsupported menu type\n");
      break;
    }

    //    g_signal_connect(item, "activate", GTK_SIGNAL_FUNC(cb_menu_close), appdata);
  
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
