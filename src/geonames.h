/*
 * Copyright (C) 2009 Till Harbaum <till@harbaum.org>.
 *
 * Contributor: Damien Caliste 2013 <dcaliste@free.fr>
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

#include <glib.h>

#include "converter.h"

G_BEGIN_DECLS

struct _MaepGeonamesPlace {
  char *name, *country;
  coord_t pos;
};

struct _MaepGeonamesEntry {
  char *title, *summary;
  char *url, *thumbnail_url;
  coord_t pos;
};

typedef struct _MaepGeonamesPlace MaepGeonamesPlace;

typedef struct _MaepGeonamesEntry MaepGeonamesEntry;

void maep_geonames_place_free(MaepGeonamesPlace *geoname);
void maep_geonames_place_list_free(GSList *list);

MaepGeonamesEntry* maep_geonames_entry_copy(MaepGeonamesEntry *src);
void maep_geonames_entry_free(MaepGeonamesEntry *entry);
void maep_geonames_entry_list_free(GSList *list);

typedef void (*MaepGeonamesRequestCallback)(gpointer obj, GSList *list,
                                            GError *error);
void maep_geonames_entry_request(coord_t *pt1, coord_t *pt2,
                                 MaepGeonamesRequestCallback cb, gpointer obj);
void maep_geonames_place_request(const gchar *request,
                                 MaepGeonamesRequestCallback cb, gpointer obj);
void maep_nominatim_address_request(const gchar *request,
                                    MaepGeonamesRequestCallback cb, gpointer obj);
G_END_DECLS

#endif
