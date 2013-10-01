/* -*- Mode: C; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4 -*- */
/* vim:set et sw=4 ts=4 cino=t0,(0: */
/*
 * osm-gps-map.h
 * Copyright (C) Marcus Bauer 2008 <marcus.bauer@gmail.com>
 * Copyright (C) John Stowers 2009 <john.stowers@gmail.com>
 * Copyright (C) Till Harbaum 2009 <till@harbaum.org>
 * Copyright (C) Damien Caliste 2013 <dcaliste@free.fr>
 *
 * Contributions by
 * Everaldo Canuto 2009 <everaldo.canuto@gmail.com>
 *
 * This is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 2.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#ifndef _OSM_GPS_MAP_GTK_H_
#define _OSM_GPS_MAP_GTK_H_

#include "config.h"

#include <gtk/gtk.h>

#include "osm-gps-map.h"

G_BEGIN_DECLS

#define OSM_TYPE_GPS_MAP_GTK             (osm_gps_map_gtk_get_type ())
#define OSM_GPS_MAP_GTK(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), OSM_TYPE_GPS_MAP_GTK, OsmGpsMapGtk))
#define OSM_GPS_MAP_GTK_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), OSM_TYPE_GPS_MAP_GTK, OsmGpsMapGtkClass))
#define OSM_IS_GPS_MAP_GTK(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), OSM_TYPE_GPS_MAP_GTK))
#define OSM_IS_GPS_MAP_GTK_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), OSM_TYPE_GPS_MAP_GTK))
#define OSM_GPS_MAP_GTK_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), OSM_TYPE_GPS_MAP_GTK, OsmGpsMapGtkClass))

typedef struct _OsmGpsMapGtkClass OsmGpsMapGtkClass;
typedef struct _OsmGpsMapGtk OsmGpsMapGtk;
typedef struct _OsmGpsMapGtkPrivate OsmGpsMapGtkPrivate;

struct _OsmGpsMapGtkClass
{
    GtkDrawingAreaClass parent_class;
};

struct _OsmGpsMapGtk
{
    GtkDrawingArea parent_instance;
    OsmGpsMapGtkPrivate *priv;
};

typedef enum {
    OSM_GPS_MAP_GTK_KEY_FULLSCREEN,
    OSM_GPS_MAP_GTK_KEY_ZOOMIN,
    OSM_GPS_MAP_GTK_KEY_ZOOMOUT,
    OSM_GPS_MAP_GTK_KEY_UP,
    OSM_GPS_MAP_GTK_KEY_DOWN,
    OSM_GPS_MAP_GTK_KEY_LEFT,
    OSM_GPS_MAP_GTK_KEY_RIGHT,
    OSM_GPS_MAP_GTK_KEY_MAX
} OsmGpsMapGtkKey_t;

GType       osm_gps_map_gtk_get_type (void) G_GNUC_CONST;

GtkWidget*  osm_gps_map_gtk_new      (OsmGpsMap *map);
OsmGpsMap*  osm_gps_map_gtk_get_map  (OsmGpsMapGtk *widget);

void osm_gps_map_gtk_set_keyboard_shortcut(OsmGpsMapGtk *map,
                                           OsmGpsMapGtkKey_t key, guint keyval);

G_END_DECLS

#endif /* _OSM_GPS_MAP_GTK_H_ */
