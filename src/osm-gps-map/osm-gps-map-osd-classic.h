/* -*- Mode: C; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4 -*- */
/* vim:set et sw=4 ts=4 cino=t0,(0: */
/*
 * Copyright (C) Till Harbaum 2009 <till@harbaum.org>
 *
 * osm-gps-map is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * osm-gps-map is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef _OSM_GPS_MAP_OSD_CLASSIC_H_
#define _OSM_GPS_MAP_OSD_CLASSIC_H_

#ifdef OSD_GPS_BUTTON
/* define custom gps button */
#define OSD_GPS   OSD_CUSTOM
/* more could be added like */
/* #define OSD_XYZ   OSD_CUSTOM+1  */ 
#endif

#include "osm-gps-map.h"

void osm_gps_map_osd_classic_init(OsmGpsMap *map);
osd_button_t osm_gps_map_osd_check(OsmGpsMap *map, gint x, gint y);
#ifdef OSD_GPS_BUTTON
void osm_gps_map_osd_enable_gps (OsmGpsMap *map, OsmGpsMapOsdCallback cb, gpointer data);
#endif

#ifdef OSD_BALLOON
void osm_gps_map_osd_draw_balloon (OsmGpsMap *map, 
                                   float latitude, float longitude, 
                                   OsmGpsMapBalloonCallback cb, gpointer data);
void osm_gps_map_osd_clear_balloon (OsmGpsMap *map);
#endif

#ifdef OSD_NAV
void osm_gps_map_osd_clear_nav (OsmGpsMap *map);
void osm_gps_map_osd_draw_nav (OsmGpsMap *map, gboolean imperial, float latitude, float longitude, char *name);
#endif

#ifdef OSD_HEARTRATE
void osm_gps_map_osd_draw_hr (OsmGpsMap *map, gboolean ok, gint rate);
#define OSD_HR_NONE    -2
#define OSD_HR_ERROR   -1
#define OSD_HR_INVALID  0

#endif

#endif
