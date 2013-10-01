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

#ifndef GEONAMES_H
#define GEONAMES_H

#include <gtk/gtk.h>
#include "osm-gps-map/osm-gps-map.h"

void geonames_enable_search(GtkWidget *widget);
void geonames_enable_wikipedia(GtkWidget *widget, OsmGpsMap *map, gboolean enable);

#endif // GEONAMES_H
