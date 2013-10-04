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

G_BEGIN_DECLS

typedef enum {
    OSD_NONE = 0,
    OSD_BG,
    OSD_UP,
    OSD_DOWN,
    OSD_LEFT,
    OSD_RIGHT,
    OSD_IN,
    OSD_OUT,
    OSD_CUSTOM   // first custom buttom
} osd_button_t;

typedef void (*OsmGpsMapOsdCallback)(osd_button_t but, gpointer data);
#define	OSM_GPS_MAP_OSD_CALLBACK(f) ((OsmGpsMapOsdCallback) (f))

/* the osd structure mainly contains various callbacks */
/* required to draw and update the OSD */
typedef struct osm_gps_map_osd_s {
    /* To specify color. */
    double fg[3];
    double disabled[3];
    double bg[3];

    /* OSM renderer it is associated to. */
    OsmGpsMap *map;

    void(*render)(struct osm_gps_map_osd_s *);
    void(*draw)(struct osm_gps_map_osd_s *, cairo_t *);
    osd_button_t(*check)(struct osm_gps_map_osd_s *,gboolean,gint, gint);       /* check if x/y lies within OSD */
    gboolean(*busy)(struct osm_gps_map_osd_s *);
    void(*free)(struct osm_gps_map_osd_s *);

    OsmGpsMapOsdCallback cb;
    gpointer data;

    gpointer priv;
} osm_gps_map_osd_t;

typedef enum { 
    OSM_GPS_MAP_BALLOON_EVENT_TYPE_DRAW,
    OSM_GPS_MAP_BALLOON_EVENT_TYPE_CLICK,
    OSM_GPS_MAP_BALLOON_EVENT_TYPE_REMOVED,
    OSM_GPS_MAP_BALLOON_EVENT_TYPE_SIZE_REQUEST,
} osm_gps_map_balloon_event_type_t;

typedef struct {
    osm_gps_map_balloon_event_type_t type;
    union {
        struct { 
            OsmGpsMapRect_t *rect;
            cairo_t *cr;
        } draw;

        struct { 
            int x, y; 
            gboolean down; 
        } click;
    } data;
} osm_gps_map_balloon_event_t;

typedef void (*OsmGpsMapBalloonCallback)(osm_gps_map_balloon_event_t *event, gpointer data);
#define	OSM_GPS_MAP_BALLOON_CALLBACK(f) ((OsmGpsMapBalloonCallback) (f))

osm_gps_map_osd_t* osm_gps_map_osd_classic_init(OsmGpsMap *map);
void osm_gps_map_osd_classic_free(osm_gps_map_osd_t *osd);
osd_button_t osm_gps_map_osd_check(osm_gps_map_osd_t *osd, gint x, gint y);
#ifdef OSD_GPS_BUTTON
void osm_gps_map_osd_enable_gps (osm_gps_map_osd_t *osd,
                                 OsmGpsMapOsdCallback cb, gpointer data);
#endif

#ifdef OSD_BALLOON
void osm_gps_map_osd_draw_balloon (osm_gps_map_osd_t *osd, 
                                   float latitude, float longitude, 
                                   OsmGpsMapBalloonCallback cb, gpointer data);
void osm_gps_map_osd_clear_balloon (osm_gps_map_osd_t *osd);
#endif

#ifdef OSD_NAV
void osm_gps_map_osd_clear_nav (osm_gps_map_osd_t *osd);
void osm_gps_map_osd_draw_nav (osm_gps_map_osd_t *osd, gboolean imperial,
                               float latitude, float longitude, char *name);
#endif

#ifdef OSD_HEARTRATE
void osm_gps_map_osd_draw_hr (osm_gps_map_osd_t *osd, gboolean ok, gint rate);
#define OSD_HR_NONE    -2
#define OSD_HR_ERROR   -1
#define OSD_HR_INVALID  0

#endif

G_END_DECLS

#endif
