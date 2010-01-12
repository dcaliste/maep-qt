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

#include <glib.h>
#include <gtk/gtk.h>

#include "icon.h"
#include "misc.h"

/* supported icon file extensions */
static const char *icon_exts[] = {
  "gif", "png", "jpg", ""
};

static gchar*
icon_file_exists(const gchar *file) {
  gchar *filename, *fullname;
  gint idx = 0;

  while(icon_exts[idx][0]) {
    filename = g_strdup_printf("%s.%s", file, icon_exts[idx]);
    fullname = find_file(filename);
    g_free(filename);

    if(fullname)
      return fullname;

    idx++;
  }
  return NULL;
}

GdkPixbuf *icon_load(const char *name) {
  if(!name) return NULL;                               

  gchar *fullname = icon_file_exists(name);
  if(fullname) {
    GdkPixbuf *pix = gdk_pixbuf_new_from_file(fullname, NULL);
    g_free(fullname);

    return pix;
  }

  printf("Icon %s not found\n", name);
  return NULL;
}

static void icon_free(gpointer data, gpointer user_data) {
  gdk_pixbuf_unref(GDK_PIXBUF(data));
}

static void on_parent_destroy(GtkWidget *widget, gpointer data) {
  GSList *list = g_object_get_data(G_OBJECT(widget), "icons");
  g_assert(list);

  g_slist_foreach(list, icon_free, NULL);
  g_slist_free(list);
}

GtkWidget *icon_get(GtkWidget *parent, const char *name) {
  GdkPixbuf *pix = icon_load(name);
  if(!pix) return NULL;

  /* append to list of associated icons for the parent widget */
  /* if no such list exists also create a destroy handler so */
  /* the icons can be freed on destruction of the parent */
  GSList *list = g_object_get_data(G_OBJECT(parent), "icons");
  if(!list)
    g_signal_connect(G_OBJECT(parent), "destroy", 
		     G_CALLBACK(on_parent_destroy), NULL);

  list = g_slist_append (list, pix);
  g_object_set_data(G_OBJECT(parent), "icons", list);

  return gtk_image_new_from_pixbuf(pix);
}

