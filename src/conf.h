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

#ifndef CONF_H
#define CONF_H

#include <glib.h>

G_BEGIN_DECLS

void     maep_conf_unset_key (const char *key);

void     maep_conf_set_string(const char *key, const char *str);
gchar *  maep_conf_get_string(const char *key);

void     maep_conf_set_bool  (const char *key, gboolean value);
gboolean maep_conf_get_bool  (const char *key, gboolean default_value);

void     maep_conf_set_int   (const char *key, gint value);
gint     maep_conf_get_int   (const char *key, gint default_value);

void     maep_conf_set_uint_list(const char *m_key, const guint *values, gsize ln);
guint*   maep_conf_get_uint_list(const char *m_key, gsize *ln);

void     maep_conf_set_float (const char *key, gfloat value);
gfloat   maep_conf_get_float (const char *key, gfloat default_value);

void     maep_conf_set_color (const char *m_key, gdouble vals[4]);
void     maep_conf_get_color (const char *m_key, gdouble vals[4],
                              const gdouble def_value[4]);

G_END_DECLS

#endif // CONF_H
