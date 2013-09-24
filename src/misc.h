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

#ifndef MISC_H
#define MISC_H

#include <gtk/gtk.h>

char *find_file(char *name);

GtkWidget *notebook_new(void);
void notebook_append_page(GtkWidget *notebook, GtkWidget *page, char *label);
GtkWidget *notebook_get_gtk_notebook(GtkWidget *notebook);

void gconf_set_string(const char *key, const char *str);
char *gconf_get_string(const char *key);
void gconf_set_bool(char *key, gboolean value);
gboolean gconf_get_bool(char *key, gboolean default_value);
void gconf_set_int(char *key, gint value);
gint gconf_get_int(char *key, gint default_value);
void gconf_set_float(char *key, gfloat value);
gfloat gconf_get_float(char *key, gfloat default_value);

gboolean yes_no_f(GtkWidget *parent, char *title, const char *fmt, ...);
void errorf(GtkWidget *parent, const char *fmt, ...);

char *url_encode(char *str);

GtkWidget *button_new(void);
GtkWidget *button_new_with_label(char *label);
GtkWidget *entry_new(void);
GtkWidget *label_big_new(char *str);
GtkWidget *label_left_new(char *str);
GtkWidget *label_right_new(char *str);
GtkWidget *label_wrap_new(char *str);
GtkWidget *scrolled_window_new(GtkPolicyType hp,  GtkPolicyType vp);
void scrolled_window_add_with_viewport(GtkWidget *win, GtkWidget *child);
void scrolled_window_add(GtkWidget *win, GtkWidget *child);

void browser_url(GtkWidget *root, char *url);

#ifdef MAEMO5
#define GCONF_KEY_SCREEN_ROTATE "screen-rotate"

gboolean is_portrait(void);
void rotation_enable(GtkWidget *window);
void rotation_disable(GtkWidget *window);
#endif

#endif // MISC_H
