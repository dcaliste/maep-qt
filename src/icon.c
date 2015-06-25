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

static cairo_surface_t* icon_load(const char *name, gchar **fullname) {
  if(!name) return NULL;                               

  *fullname = icon_file_exists(name);
  if(*fullname) {
    cairo_surface_t *surf = cairo_image_surface_create_from_png(*fullname);
    if (cairo_surface_status(surf) == CAIRO_STATUS_SUCCESS)
      return surf;
  }

  g_warning("Icon %s not found\n", name);
  return NULL;
}

typedef struct {
  cairo_surface_t *cr_surf;
  char *name, *fullname;
} icon_reg_t;

static void icon_free(gpointer data, G_GNUC_UNUSED gpointer user_data) {
  icon_reg_t *icon_reg = (icon_reg_t*)data;

  cairo_surface_destroy(icon_reg->cr_surf);
  g_free(icon_reg->name);
  g_free(icon_reg->fullname);
  g_free(icon_reg);
}

static void free_list(gpointer data)
{
  GSList *list = (GSList*)data;
  g_assert(list);

  g_slist_foreach(list, icon_free, NULL);
  g_slist_free(list);
}

static icon_reg_t* icon_register(GObject *parent, const char *name) {
  icon_reg_t *icon_reg;
  cairo_surface_t *cr_surf;
  gchar *fullname;

  cr_surf = icon_load(name, &fullname);
  if (!cr_surf) return NULL;

  icon_reg = g_new0(icon_reg_t, 1);
  icon_reg->cr_surf = cr_surf;
  icon_reg->name = g_strdup(name);
  icon_reg->fullname = fullname;

  /* append to list of associated icons for the parent widget */
  /* if no such list exists also create a destroy handler so */
  /* the icons can be freed on destruction of the parent */
  GSList *list = g_object_get_data(G_OBJECT(parent), "icons");
  if(!list)
    {
      list = g_slist_append (list, icon_reg);
      g_object_set_data_full(G_OBJECT(parent), "icons", list, free_list);
    }
  else
    list = g_slist_append (list, icon_reg);

  return icon_reg;
}

static gint icon_compare(gconstpointer a, gconstpointer b) {
  const icon_reg_t *icon_reg = (icon_reg_t*)a;
  const char *name = (const char*)b;

  if(!icon_reg->name) return -1;

  return strcmp(icon_reg->name, name);
}

/* check if an icon of that name has already been registered and use */
/* that if present */
static icon_reg_t *icon_register_check(GObject *parent, const char *name) {
  GSList *list = g_object_get_data(parent, "icons");
  if(!list) return NULL;

  list = g_slist_find_custom(list, name, icon_compare);
  if(!list) return NULL;

  return (icon_reg_t*)list->data;
}

cairo_surface_t *icon_get_surface(GObject *parent, const char *name) {
  /* check if we already have loaded this icon */
  icon_reg_t *icon;

  g_return_val_if_fail(parent && name, NULL);

  icon = icon_register_check(parent, name);
  if(!icon)
    icon = icon_register(parent, name);

  return icon->cr_surf;
}

#ifdef WITH_GTK
GtkWidget *icon_get_widget(GObject *parent, const char *name) {
  icon_reg_t *icon;

  g_return_val_if_fail(parent && name, NULL);

  icon = icon_register_check(parent, name);
  if(!icon)
    icon = icon_register(parent, name);

  return gtk_image_new_from_file(icon->fullname);
}
#endif
