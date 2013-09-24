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

#ifndef _OSM_GPS_MAP_GTKWIDGET_H_
#define _OSM_GPS_MAP_GTKWIDGET_H_

#include "config.h"

#include <gtk/gtk.h>

#include "osm-gps-map.h"

G_BEGIN_DECLS

#define OSM_TYPE_GPS_MAP_GTKWIDGET             (osm_gps_map_gtkwidget_get_type ())
#define OSM_GPS_MAP_GTKWIDGET(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), OSM_TYPE_GPS_MAP_GTKWIDGET, OsmGpsMapGtkwidget))
#define OSM_GPS_MAP_GTKWIDGET_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), OSM_TYPE_GPS_MAP_GTKWIDGET, OsmGpsMapGtkwidgetClass))
#define OSM_IS_GPS_MAP_GTKWIDGET(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), OSM_TYPE_GPS_MAP_GTKWIDGET))
#define OSM_IS_GPS_MAP_GTKWIDGET_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), OSM_TYPE_GPS_MAP_GTKWIDGET))
#define OSM_GPS_MAP_GTKWIDGET_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), OSM_TYPE_GPS_MAP_GTKWIDGET, OsmGpsMapGtkwidgetClass))

typedef struct _OsmGpsMapGtkwidgetClass OsmGpsMapGtkwidgetClass;
typedef struct _OsmGpsMapGtkwidget OsmGpsMapGtkwidget;
typedef struct _OsmGpsMapGtkwidgetPrivate OsmGpsMapGtkwidgetPrivate;

struct _OsmGpsMapGtkwidgetClass
{
    GtkDrawingAreaClass parent_class;
};

struct _OsmGpsMapGtkwidget
{
    GtkDrawingArea parent_instance;
    OsmGpsMapGtkwidgetPrivate *priv;
};

typedef enum {
    OSM_GPS_MAP_GTKWIDGET_KEY_FULLSCREEN,
    OSM_GPS_MAP_GTKWIDGET_KEY_ZOOMIN,
    OSM_GPS_MAP_GTKWIDGET_KEY_ZOOMOUT,
    OSM_GPS_MAP_GTKWIDGET_KEY_UP,
    OSM_GPS_MAP_GTKWIDGET_KEY_DOWN,
    OSM_GPS_MAP_GTKWIDGET_KEY_LEFT,
    OSM_GPS_MAP_GTKWIDGET_KEY_RIGHT,
    OSM_GPS_MAP_GTKWIDGET_KEY_MAX
} OsmGpsMapGtkwidgetKey_t;

GType       osm_gps_map_gtkwidget_get_type (void) G_GNUC_CONST;

GtkWidget*  osm_gps_map_gtkwidget_new      (OsmGpsMap *map);

void osm_gps_map_gtkwidget_set_keyboard_shortcut(OsmGpsMapGtkwidget *map,
                                                 OsmGpsMapGtkwidgetKey_t key, guint keyval);

G_END_DECLS

#endif /* _OSM_GPS_MAP_GTKWIDGET_H_ */
