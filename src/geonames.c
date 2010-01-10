/*
 * Copyright (C) 2009 Till Harbaum <till@harbaum.org>.
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

#include "net_io.h"
#include "geonames.h"
#include "misc.h"
#include "menu.h"

#define GEONAMES_SEARCH "geonames_search"

static void on_search_close_clicked(GtkButton *button, gpointer user) {
  GtkWidget *parent = GTK_WIDGET(user);

  GtkWidget *hbox = g_object_get_data(G_OBJECT(parent), GEONAMES_SEARCH);
  g_assert(hbox);

  menu_enable(parent, "Search", TRUE); 
  g_object_set_data(G_OBJECT(parent), GEONAMES_SEARCH, NULL);
  gtk_widget_destroy(hbox);
}

void geonames_enable_search(GtkWidget *parent) {
  GtkWidget *hbox = g_object_get_data(G_OBJECT(parent), GEONAMES_SEARCH);
  g_assert(!hbox);

  menu_enable(parent, "Search", FALSE); 

  /* no widget present -> build and install one */
  hbox = gtk_hbox_new(FALSE, 0);
  gtk_box_pack_start_defaults(GTK_BOX(hbox), gtk_entry_new());
  gtk_box_pack_start(GTK_BOX(hbox), 
		     button_new_with_label("Search"), FALSE, FALSE, 0);
  
  /* create close button */
  GtkWidget *button = button_new();
  g_signal_connect_after(button, "clicked", 
			 G_CALLBACK(on_search_close_clicked), parent);
  GtkWidget *image = 
    gtk_image_new_from_stock(GTK_STOCK_CLOSE, GTK_ICON_SIZE_BUTTON);
  gtk_button_set_image(GTK_BUTTON(button), image);
  gtk_box_pack_start(GTK_BOX(hbox), button, FALSE, FALSE, 0);
  
  /* the parent is a container with a vbox being its child */
  GList *children = gtk_container_get_children(GTK_CONTAINER(parent));
  g_assert(g_list_length(children) == 1);
  
  gtk_widget_show_all(hbox);
  gtk_box_pack_start(GTK_BOX(children->data), hbox, FALSE, FALSE, 0);
  
  g_object_set_data(G_OBJECT(parent), GEONAMES_SEARCH, hbox);
}
