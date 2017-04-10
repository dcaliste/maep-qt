/* -*- Mode: C; indent-tabs-mode: nil; c-basic-offset: 2; tab-width: 2 -*- */
/*
 * Copyright (C) 2009 Till Harbaum <till@harbaum.org>.
 * Copyright (C) 2017 Damien Caliste <dcaliste@free.fr>.
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

#include <dconf.h>
#include <string.h>

#include "config.h"
#include "conf.h"

#define MAEP_CONF_PATH       "/apps/" APP "/%s"
#define OLD_PATH         "/apps/maep/%s"

static DConfClient* dconfClient = NULL;
static DConfClient* dconf_client_get_default()
{
  if (!dconfClient)
    dconfClient = dconf_client_new();
  return dconfClient;
}

void maep_conf_unset_key(const gchar *m_key) {
  DConfClient *client = dconf_client_get_default();
  gchar *key = g_strdup_printf(MAEP_CONF_PATH, m_key);
  GError *error = NULL;

  dconf_client_write_sync(client, key, NULL, NULL, NULL, &error);
  g_free(key);
  if (error) {
    g_warning("%s", error->message);
    g_error_free(error);
  }
}

gchar** maep_conf_get_list(const gchar *m_key) {
  DConfClient *client = dconf_client_get_default();
  gchar *key = g_strdup_printf(MAEP_CONF_PATH, m_key);
  gchar **list;
  
  list = dconf_client_list(client, key, NULL);
  g_free(key);

  return list;
}

void maep_conf_set_string(const gchar *m_key, const gchar *str) {
  /* GConfClient *client = maep_conf_client_get_default(); */
  DConfClient *client = dconf_client_get_default();
  gchar *key = g_strdup_printf(MAEP_CONF_PATH, m_key);
  GError *error = NULL;
  GVariant *var;
  if(!str || !strlen(str))
    var = g_variant_ref_sink(g_variant_new_string(""));
  else
    var = g_variant_ref_sink(g_variant_new_string(str));
  dconf_client_write_sync(client, key, var, NULL, NULL, &error);
  /* maep_conf_client_set_float(client, key, value, NULL); */
  g_free(key);
  if (error) {
    g_warning("%s", error->message);
    g_error_free(error);
  }
  g_variant_unref(var);
}

char *maep_conf_get_string(const gchar *m_key) {
  /* GConfClient *client = maep_conf_client_get_default(); */
  DConfClient *client = dconf_client_get_default();

  gchar *key = g_strdup_printf(MAEP_CONF_PATH, m_key);
  /* GConfValue *value = maep_conf_client_get(client, key, NULL); */
  GVariant *value = dconf_client_read(client, key);
  if(!value) {
    g_free(key);
    key = g_strdup_printf(OLD_PATH, m_key);
    value = dconf_client_read(client, key);
    if (!value) {
      g_free(key);
      return NULL;
    }
  }  

  /* gchar *ret = maep_conf_client_get_string(client, key, NULL); */
  gsize len;
  gchar *ret = g_variant_dup_string(value, &len);
  g_free(key);
  g_variant_unref(value);
  return ret;
}

void maep_conf_set_bool(const gchar *m_key, gboolean value) {
  /* GConfClient *client = maep_conf_client_get_default(); */
  DConfClient *client = dconf_client_get_default();
  gchar *key = g_strdup_printf(MAEP_CONF_PATH, m_key);
  GError *error = NULL;
  GVariant *var = g_variant_ref_sink(g_variant_new_boolean(value));
  dconf_client_write_sync(client, key, var, NULL, NULL, &error);
  /* maep_conf_client_set_float(client, key, value, NULL); */
  g_free(key);
  if (error) {
    g_warning("%s", error->message);
    g_error_free(error);
  }
  g_variant_unref(var);
}

gboolean maep_conf_get_bool(const gchar *m_key, gboolean default_value) {
  /* GConfClient *client = maep_conf_client_get_default(); */
  DConfClient *client = dconf_client_get_default();

  gchar *key = g_strdup_printf(MAEP_CONF_PATH, m_key);
  /* GConfValue *value = maep_conf_client_get(client, key, NULL); */
  GVariant *value = dconf_client_read(client, key);
  if(!value) {
    key = g_strdup_printf(OLD_PATH, m_key);
    value = dconf_client_read(client, key);
    if (!value) {
      g_free(key);
      return default_value;
    }
  }

  if(!g_variant_is_of_type(value, G_VARIANT_TYPE_BOOLEAN)) {
    g_message("wrong type returning %d", default_value);
    g_free(key);
    g_variant_unref(value);
    return default_value;
  }
  /* gboolean ret = maep_conf_client_get_bool(client, key, NULL); */
  gboolean ret = g_variant_get_boolean(value);
  g_free(key);
  g_variant_unref(value);
  return ret;
}

void maep_conf_set_int(const gchar *m_key, gint value) {
  /* GConfClient *client = maep_conf_client_get_default(); */
  DConfClient *client = dconf_client_get_default();
  gchar *key = g_strdup_printf(MAEP_CONF_PATH, m_key);
  GError *error = NULL;
  GVariant *var = g_variant_ref_sink(g_variant_new_int32(value));
  dconf_client_write_sync(client, key, var, NULL, NULL, &error);
  /* maep_conf_client_set_float(client, key, value, NULL); */
  g_free(key);
  if (error) {
    g_warning("%s", error->message);
    g_error_free(error);
  }
  g_variant_unref(var);
}

gint maep_conf_get_int(const gchar *m_key, gint def_value) {
  /* GConfClient *client = maep_conf_client_get_default(); */
  DConfClient *client = dconf_client_get_default();

  gchar *key = g_strdup_printf(MAEP_CONF_PATH, m_key);
  /* GConfValue *value = maep_conf_client_get(client, key, NULL); */
  GVariant *value = dconf_client_read(client, key);
  if(!value) {
    key = g_strdup_printf(OLD_PATH, m_key);
    value = dconf_client_read(client, key);
    if (!value) {
      g_free(key);
      return def_value;
    }
  }

  /* gint ret = maep_conf_client_get_int(client, key, NULL); */
  if(!g_variant_is_of_type(value, G_VARIANT_TYPE_INT32)) {
    g_message("wrong type returning %d", def_value);
    g_free(key);
    g_variant_unref(value);
    return def_value;
  }
  gint ret = g_variant_get_int32(value);
  g_free(key);
  g_variant_unref(value);
  return ret;
}

void maep_conf_set_float(const gchar *m_key, gfloat value) {
  /* GConfClient *client = maep_conf_client_get_default(); */
  DConfClient *client = dconf_client_get_default();
  gchar *key = g_strdup_printf(MAEP_CONF_PATH, m_key);
  GError *error = NULL;
  GVariant *var = g_variant_ref_sink(g_variant_new_double(value));
  dconf_client_write_sync(client, key, var, NULL, NULL, &error);
  /* maep_conf_client_set_float(client, key, value, NULL); */
  g_free(key);
  if (error) {
    g_warning("%s", error->message);
    g_error_free(error);
  }
  g_variant_unref(var);
}

gfloat maep_conf_get_float(const gchar *m_key, gfloat def_value) {
  /* GConfClient *client = maep_conf_client_get_default(); */
  DConfClient *client = dconf_client_get_default();

  gchar *key = g_strdup_printf(MAEP_CONF_PATH, m_key);
  /* GConfValue *value = maep_conf_client_get(client, key, NULL); */
  GVariant *value = dconf_client_read(client, key);
  if(!value) {
    key = g_strdup_printf(OLD_PATH, m_key);
    value = dconf_client_read(client, key);
    if (!value) {
      g_free(key);
      return def_value;
    }
  }

  if(!g_variant_is_of_type(value, G_VARIANT_TYPE_DOUBLE)) {
    g_message("wrong type returning %g", def_value);
    g_free(key);
    g_variant_unref(value);
    return def_value;
  }
  /* gfloat ret = maep_conf_client_get_float(client, key, NULL); */
  gfloat ret = g_variant_get_double(value);
  g_free(key);
  g_variant_unref(value);
  return ret;
}

void maep_conf_set_color(const gchar *m_key, gdouble vals[4]) {
  /* GConfClient *client = maep_conf_client_get_default(); */
  DConfClient *client = dconf_client_get_default();
  gchar *key = g_strdup_printf(MAEP_CONF_PATH, m_key);
  GError *error = NULL;
  GVariant *children[] = {g_variant_new_double(vals[0]),
                          g_variant_new_double(vals[1]),
                          g_variant_new_double(vals[2]),
                          g_variant_new_double(vals[3])};
  GVariant *var = g_variant_ref_sink(g_variant_new_tuple(children, 4));
  dconf_client_write_sync(client, key, var, NULL, NULL, &error);
  /* maep_conf_client_set_float(client, key, value, NULL); */
  g_free(key);
  if (error) {
    g_warning("%s", error->message);
    g_error_free(error);
  }
  g_variant_unref(var);
}

void maep_conf_get_color(const gchar *m_key, gdouble vals[4], const gdouble def_value[4]) {
  /* GConfClient *client = maep_conf_client_get_default(); */
  DConfClient *client = dconf_client_get_default();

  gchar *key = g_strdup_printf(MAEP_CONF_PATH, m_key);
  /* GConfValue *value = maep_conf_client_get(client, key, NULL); */
  GVariant *value = dconf_client_read(client, key);

  vals[0] = def_value[0];
  vals[1] = def_value[1];
  vals[2] = def_value[2];
  vals[3] = def_value[3];

  g_free(key);

  if (!value) {
    key = g_strdup_printf(OLD_PATH, m_key);
    value = dconf_client_read(client, key);
    g_free(key);
    if (!value)
      return;
  }

  if (!g_variant_is_of_type(value, G_VARIANT_TYPE_TUPLE) ||
      g_variant_n_children(value) != 4) {
    g_message("wrong type returning default");
    g_variant_unref(value);
    return;
  }
  GVariant *child;
  child = g_variant_get_child_value(value, 0);
  vals[0] = g_variant_get_double(child);
  g_variant_unref(child);

  child = g_variant_get_child_value(value, 1);
  vals[1] = g_variant_get_double(child);
  g_variant_unref(child);

  child = g_variant_get_child_value(value, 2);
  vals[2] = g_variant_get_double(child);
  g_variant_unref(child);

  child = g_variant_get_child_value(value, 3);
  vals[3] = g_variant_get_double(child);
  g_variant_unref(child);

  g_variant_unref(value);
  return;
}

void maep_conf_set_uint_list(const char *m_key, const guint *values, gsize ln)
{
  DConfClient *client = dconf_client_get_default();
  gchar *key = g_strdup_printf(MAEP_CONF_PATH, m_key);
  GError *error = NULL;
  GVariant **children;
  GVariant *var;
  gsize i;

  if (!ln || !values) {
    maep_conf_unset_key(m_key);
    return;
  }

  children = g_malloc(sizeof(GVariant*) * ln);
  for (i = 0; i < ln; i++)
    children[i] = g_variant_new_uint32(values[i]);
  var = g_variant_ref_sink(g_variant_new_tuple(children, ln));
  g_free(children);
  
  dconf_client_write_sync(client, key, var, NULL, NULL, &error);
  g_free(key);
  if (error) {
    g_warning("%s", error->message);
    g_error_free(error);
  }
  g_variant_unref(var);
}

guint* maep_conf_get_uint_list(const char *m_key, gsize *ln)
{
  DConfClient *client = dconf_client_get_default();
  gchar *key;
  GVariant *value;
  GVariant *child;
  gsize i;
  guint *values;

  if (!ln)
    return NULL;
  *ln = 0;

  key = g_strdup_printf(MAEP_CONF_PATH, m_key);
  value = dconf_client_read(client, key);
  g_free(key);

  if (!value) {
    key = g_strdup_printf(OLD_PATH, m_key);
    value = dconf_client_read(client, key);
    g_free(key);
    if (!value)
      return NULL;
  }

  if (!g_variant_is_of_type(value, G_VARIANT_TYPE_TUPLE)) {
    g_message("wrong type returning NULL");
    g_variant_unref(value);
    return NULL;
  }

  *ln = g_variant_n_children(value);
  values = g_malloc(sizeof(guint) * *ln);
  for (i = 0; i < *ln; i++) {
    child = g_variant_get_child_value(value, i);
    if (g_variant_is_of_type(child, G_VARIANT_TYPE_UINT32))
      values[i] = g_variant_get_uint32(child);
    else
      values[i] = 0;
    g_variant_unref(child);
  }
  g_variant_unref(value);
  
  return values;
}

