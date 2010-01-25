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
#include <string.h>

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

typedef struct {
  GdkPixbuf *pix;
  char *name;
} icon_reg_t;

static void icon_free(gpointer data, gpointer user_data) {
  icon_reg_t *icon_reg = (icon_reg_t*)data;

  gdk_pixbuf_unref(icon_reg->pix);
  if(icon_reg->name) g_free(icon_reg->name);
  g_free(icon_reg);
}

static void on_parent_destroy(GtkWidget *widget, gpointer data) {
  GSList *list = g_object_get_data(G_OBJECT(widget), "icons");
  g_assert(list);

  g_slist_foreach(list, icon_free, NULL);
  g_slist_free(list);
}

void icon_register_pixbuf(GtkWidget *parent, const char *name, GdkPixbuf *pix) {
  icon_reg_t *icon_reg = g_new0(icon_reg_t, 1);
  icon_reg->pix = pix;
  if(name) icon_reg->name = g_strdup(name);

  /* append to list of associated icons for the parent widget */
  /* if no such list exists also create a destroy handler so */
  /* the icons can be freed on destruction of the parent */
  GSList *list = g_object_get_data(G_OBJECT(parent), "icons");
  if(!list)
    g_signal_connect(G_OBJECT(parent), "destroy", 
		     G_CALLBACK(on_parent_destroy), NULL);

  list = g_slist_append (list, icon_reg);
  g_object_set_data(G_OBJECT(parent), "icons", list);
}

static gint icon_compare(gconstpointer a, gconstpointer b) {
  const icon_reg_t *icon_reg = (icon_reg_t*)a;
  const char *name = (const char*)b;

  if(!icon_reg->name) return -1;

  return strcmp(icon_reg->name, name);
}

/* check if an icon of that name has already been registered and use */
/* that if present */
GdkPixbuf *icon_register_check(GtkWidget *parent, const char *name) {
  GSList *list = g_object_get_data(G_OBJECT(parent), "icons");
  if(!list) return NULL;

  list = g_slist_find_custom(list, name, icon_compare);
  if(!list) return NULL;

  return ((icon_reg_t*)list->data)->pix;
}

GdkPixbuf *icon_get_pixbuf(GtkWidget *parent, const char *name) {
  /* check if we already have loaded this icon */
  GdkPixbuf *pix = icon_register_check(parent, name);

  /* if not: load it */
  if(!pix) {
    pix = icon_load(name);
    if(!pix) return NULL;

    icon_register_pixbuf(parent, name, pix);
  }

  return pix;
}

GtkWidget *icon_get_widget(GtkWidget *parent, const char *name) {
  GdkPixbuf *pix = icon_get_pixbuf(parent, name);
  return gtk_image_new_from_pixbuf(pix);
}

