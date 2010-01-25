/*
 * Copyright (C) 2008 Till Harbaum <till@harbaum.org>.
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

#ifndef ICON_H
#define ICON_H

GtkWidget *icon_get_widget(GtkWidget *parent, const char *name);
GdkPixbuf *icon_get_pixbuf(GtkWidget *parent, const char *name);
void icon_register_pixbuf(GtkWidget *parent, const char *name, GdkPixbuf *pix);
GdkPixbuf *icon_register_check(GtkWidget *parent, const char *name);

#endif // ICON_H
