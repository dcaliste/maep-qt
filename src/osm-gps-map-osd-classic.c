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

#include "config.h" 

#include <stdlib.h>  // abs
#include <string.h>
#include <math.h>    // M_PI/cos()

/* parameters that can be overwritten from the config file: */
/* OSD_DIAMETER */
/* OSD_X, OSD_Y */

#include <cairo.h>

#include "osm-gps-map.h"
#include "converter.h"
#include "osm-gps-map-osd-classic.h"

static OsmGpsMapSource_t map_sources[] = {
    OSM_GPS_MAP_SOURCE_OPENSTREETMAP,
    OSM_GPS_MAP_SOURCE_OPENSTREETMAP_RENDERER,
    OSM_GPS_MAP_SOURCE_OPENCYCLEMAP,
    OSM_GPS_MAP_SOURCE_OSM_PUBLIC_TRANSPORT,
    OSM_GPS_MAP_SOURCE_GOOGLE_STREET,
    OSM_GPS_MAP_SOURCE_VIRTUAL_EARTH_STREET,
    OSM_GPS_MAP_SOURCE_VIRTUAL_EARTH_SATELLITE,
    OSM_GPS_MAP_SOURCE_VIRTUAL_EARTH_HYBRID
};
static int num_map_sources = sizeof(map_sources)/sizeof(map_sources[0]);

#ifdef OSD_DOUBLEPIXEL
#define OSD_DPIX_EXTRA 1
#define OSD_DPIX_SKIP  (OSD_FONT_SIZE/2)
#else
#define OSD_DPIX_EXTRA 0
#define OSD_DPIX_SKIP  0
#endif

//the osd controls
typedef struct {
    /* the offscreen representation of the OSD */
    struct {
        cairo_surface_t *surface;
        gboolean rendered;
#ifdef OSD_GPS_BUTTON
        gboolean gps_enabled;
#endif
    } controls;

#ifdef OSD_BALLOON
    //a balloon with additional info
    struct {
        cairo_surface_t *surface;
        int orientation, offset_x, offset_y;

        gboolean just_created;
        float lat, lon;
        OsmGpsMapRect_t rect;

        /* function called to have the user app draw the contents */
        OsmGpsMapBalloonCallback cb;
        gpointer data;
    } balloon;
#endif

#ifdef OSD_SCALE
    struct {
        cairo_surface_t *surface;
        int zoom;
    } scale;
#endif
        
#ifdef OSD_CROSSHAIR
    struct {
        cairo_surface_t *surface;
        gboolean rendered;
    } crosshair;
#endif

#ifdef OSD_NAV
    struct {
        cairo_surface_t *surface;
        float lat, lon;
        char *name;
        gboolean imperial, mode;    // display distance imperial/metric
        int click_sep;
    } nav;
#endif

#ifdef OSD_HEARTRATE
    struct {
        cairo_surface_t *surface;
        gint rate;
        gboolean ok;
    } hr;
#endif

#ifdef OSD_COORDINATES
    struct {
        cairo_surface_t *surface;
        float lat, lon;
    } coordinates;
#endif

#ifdef OSD_SOURCE_SEL
    struct {
        /* values to handle the "source" menu */
        cairo_surface_t *surface;
        gboolean expanded;
        gint shift, dir, count;
        gint handler_id;
        gint width, height;
        gboolean rendered;
        gint max_h;
    } source_sel;
#endif

} osd_priv_t;

#ifdef OSD_BALLOON
/* most visual effects are hardcoded by now, but may be made */
/* available via properties later */
#ifndef BALLOON_AREA_WIDTH
#define BALLOON_AREA_WIDTH           290
#endif
#ifndef BALLOON_AREA_HEIGHT
#define BALLOON_AREA_HEIGHT           75
#endif
#ifndef BALLOON_CORNER_RADIUS
#define BALLOON_CORNER_RADIUS         10
#endif

#define BALLOON_BORDER               (BALLOON_CORNER_RADIUS/2)
#define BALLOON_WIDTH                (priv->balloon.rect.w + 2 * BALLOON_BORDER)
#define BALLOON_HEIGHT               (priv->balloon.rect.h + 2 * BALLOON_BORDER)
#define BALLOON_TRANSPARENCY         0.8
#define POINTER_HEIGHT                20
#define POINTER_FOOT_WIDTH            20
#define POINTER_OFFSET               (BALLOON_CORNER_RADIUS*3/4)
#define BALLOON_SHADOW               (BALLOON_CORNER_RADIUS/2)
#define BALLOON_SHADOW_TRANSPARENCY  0.2

#define BALLOON_W  (BALLOON_WIDTH + BALLOON_SHADOW)
#define BALLOON_H  (BALLOON_HEIGHT + POINTER_HEIGHT + BALLOON_SHADOW)

/* draw the bubble shape. this is used twice, once for the shape and once */
/* for the shadow */
static void 
osm_gps_map_draw_balloon_shape (cairo_t *cr, int x0, int y0, int x1, int y1, 
       gboolean bottom, int px, int py, int px0, int px1) {

    cairo_move_to (cr, x0, y0 + BALLOON_CORNER_RADIUS);
    cairo_arc (cr, x0 + BALLOON_CORNER_RADIUS, y0 + BALLOON_CORNER_RADIUS,
               BALLOON_CORNER_RADIUS, -M_PI, -M_PI/2);
    if(!bottom) {
        /* insert top pointer */
        cairo_line_to (cr, px1, y0);
        cairo_line_to (cr, px, py);
        cairo_line_to (cr, px0, y0);
    }
        
    cairo_line_to (cr, x1 - BALLOON_CORNER_RADIUS, y0);
    cairo_arc (cr, x1 - BALLOON_CORNER_RADIUS, y0 + BALLOON_CORNER_RADIUS,
               BALLOON_CORNER_RADIUS, -M_PI/2, 0);
    cairo_line_to (cr, x1 , y1 - BALLOON_CORNER_RADIUS);
    cairo_arc (cr, x1 - BALLOON_CORNER_RADIUS, y1 - BALLOON_CORNER_RADIUS,
               BALLOON_CORNER_RADIUS, 0, M_PI/2);
    if(bottom) {
        /* insert bottom pointer */
        cairo_line_to (cr, px0, y1);
        cairo_line_to (cr, px, py);
        cairo_line_to (cr, px1, y1);
    }
        
    cairo_line_to (cr, x0 + BALLOON_CORNER_RADIUS, y1);
    cairo_arc (cr, x0 + BALLOON_CORNER_RADIUS, y1 - BALLOON_CORNER_RADIUS,
               BALLOON_CORNER_RADIUS, M_PI/2, M_PI);

    cairo_close_path (cr);
}

static void
osd_render_balloon(osm_gps_map_osd_t *osd) {
    osd_priv_t *priv = (osd_priv_t*)osd->priv; 

    if(!priv->balloon.surface)
        return;
    /* get zoom */
    gint zoom;
    g_object_get(OSM_GPS_MAP(osd->widget), "zoom", &zoom, NULL);

    /* ------- convert given coordinate into screen position --------- */
    gint xs, ys;
    osm_gps_map_geographic_to_screen (OSM_GPS_MAP(osd->widget),
                                      priv->balloon.lat, priv->balloon.lon, 
                                      &xs, &ys);

    gint x0 = 1, y0 = 1;

    /* check position of this relative to screen center to determine */
    /* pointer direction ... */
    int pointer_x, pointer_x0, pointer_x1;
    int pointer_y;

    /* ... and calculate position */
    int orientation = 0;
    if(xs > osd->widget->allocation.width/2) {
        priv->balloon.offset_x = -BALLOON_WIDTH + POINTER_OFFSET;
        pointer_x = x0 - priv->balloon.offset_x;
        pointer_x0 = pointer_x - (BALLOON_CORNER_RADIUS - POINTER_OFFSET);
        pointer_x1 = pointer_x0 - POINTER_FOOT_WIDTH;
        orientation |= 1;
    } else {
        priv->balloon.offset_x = -POINTER_OFFSET;
        pointer_x = x0 - priv->balloon.offset_x;
        pointer_x1 = pointer_x + (BALLOON_CORNER_RADIUS - POINTER_OFFSET);
        pointer_x0 = pointer_x1 + POINTER_FOOT_WIDTH;
    }
    
    gboolean bottom = FALSE;
    if(ys > osd->widget->allocation.height/2) {
        priv->balloon.offset_y = -BALLOON_HEIGHT - POINTER_HEIGHT;
        pointer_y = y0 - priv->balloon.offset_y;
        bottom = TRUE;
        orientation |= 2;
    } else {
        priv->balloon.offset_y = 0;
        pointer_y = y0 - priv->balloon.offset_y;
        y0 += POINTER_HEIGHT;
    }
    
    /* if required orientation equals current one, then don't render */
    /* anything */
    if(orientation == priv->balloon.orientation) 
        return;

    priv->balloon.orientation = orientation;

    /* calculate bottom/right of box */
    int x1 = x0 + priv->balloon.rect.w + 2*BALLOON_BORDER;
    int y1 = y0 + priv->balloon.rect.h + 2*BALLOON_BORDER;

    /* save balloon screen coordinates for later use */
    priv->balloon.rect.x = x0 + BALLOON_BORDER;
    priv->balloon.rect.y = y0 + BALLOON_BORDER;

    g_assert(priv->balloon.surface);
    cairo_t *cr = cairo_create(priv->balloon.surface);
    cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
    cairo_set_source_rgba(cr, 0.0, 0.0, 0.0, 0.0);
    cairo_paint(cr);
    cairo_set_operator(cr, CAIRO_OPERATOR_OVER);

    /* --------- draw shadow --------------- */
    osm_gps_map_draw_balloon_shape (cr, 
                 x0 + BALLOON_SHADOW, y0 + BALLOON_SHADOW, 
                 x1 + BALLOON_SHADOW, y1 + BALLOON_SHADOW,
                 bottom, pointer_x, pointer_y, 
                 pointer_x0 + BALLOON_SHADOW, pointer_x1 + BALLOON_SHADOW);

    cairo_set_source_rgba (cr, 0, 0, 0, BALLOON_SHADOW_TRANSPARENCY);
    cairo_fill_preserve (cr);
    cairo_set_source_rgba (cr, 1, 0, 0, 1.0);
    cairo_set_line_width (cr, 0);
    cairo_stroke (cr);
    
    /* --------- draw main shape ----------- */
    osm_gps_map_draw_balloon_shape (cr, x0, y0, x1, y1,
                 bottom, pointer_x, pointer_y, pointer_x0, pointer_x1);
        
    cairo_set_source_rgba (cr, 1, 1, 1, BALLOON_TRANSPARENCY);
    cairo_fill_preserve (cr);
    cairo_set_source_rgba (cr, 0, 0, 0, BALLOON_TRANSPARENCY);
    cairo_set_line_width (cr, 1);
    cairo_stroke (cr);
    
    if (priv->balloon.cb) {
        osm_gps_map_balloon_event_t event;

        /* clip in case application tries to draw in */
            /* exceed of the balloon */
        cairo_rectangle (cr, priv->balloon.rect.x, priv->balloon.rect.y, 
                         priv->balloon.rect.w, priv->balloon.rect.h);
        cairo_clip (cr);
        cairo_new_path (cr);  /* current path is not consumed by cairo_clip */

        /* request the application to draw the balloon contents */
        event.type = OSM_GPS_MAP_BALLOON_EVENT_TYPE_DRAW;
        event.data.draw.rect = &priv->balloon.rect;
        event.data.draw.cr = cr;
        
        priv->balloon.cb(&event, priv->balloon.data);
    }

    cairo_destroy(cr);
}

/* return true if balloon is being displayed and if */
/* the given coordinate is within this balloon */
static gboolean 
osd_balloon_check(osm_gps_map_osd_t *osd, gboolean click, gboolean down, gint x, gint y) 
{
    osd_priv_t *priv = (osd_priv_t*)osd->priv; 

    if(!priv->balloon.surface)
        return FALSE;

    gint xs, ys;
    osm_gps_map_geographic_to_screen (OSM_GPS_MAP(osd->widget),
                                      priv->balloon.lat, priv->balloon.lon, 
                                      &xs, &ys);

    xs += priv->balloon.rect.x + priv->balloon.offset_x;
    ys += priv->balloon.rect.y + priv->balloon.offset_y;

    gboolean is_in = 
        (x > xs) && (x < xs + priv->balloon.rect.w) &&
        (y > ys) && (y < ys + priv->balloon.rect.h);
    
    /* is this a real click or is the application just checking for something? */
    if(click) {

        /* handle the fact that the balloon may have been created by the */
        /* button down event */
        if(!is_in && !down && !priv->balloon.just_created) {
            /* the user actually clicked outside the balloon */
            
            /* close the balloon! */
            osm_gps_map_osd_clear_balloon (OSM_GPS_MAP(osd->widget));

            /* and inform application about this */
            if(priv->balloon.cb) {
                osm_gps_map_balloon_event_t event;
                event.type = OSM_GPS_MAP_BALLOON_EVENT_TYPE_REMOVED;
                priv->balloon.cb(&event, priv->balloon.data);
            }

        }
        
        if(is_in && priv->balloon.cb) {
            osm_gps_map_balloon_event_t event;
            
            /* notify application of click */
            event.type = OSM_GPS_MAP_BALLOON_EVENT_TYPE_CLICK;
            event.data.click.x = x - xs;
            event.data.click.y = y - ys;
            event.data.click.down = down;
            
            priv->balloon.cb(&event, priv->balloon.data);
        }
    }
    return is_in;
}

void osm_gps_map_osd_clear_balloon (OsmGpsMap *map) {
    g_return_if_fail (OSM_IS_GPS_MAP (map));

    osm_gps_map_osd_t *osd = osm_gps_map_osd_get(map);
    g_return_if_fail (osd);

    osd_priv_t *priv = (osd_priv_t*)osd->priv; 
    g_return_if_fail (priv);

    if(priv->balloon.surface) {
        cairo_surface_destroy(priv->balloon.surface);
        priv->balloon.surface = NULL;
        priv->balloon.lat = OSM_GPS_MAP_INVALID;
        priv->balloon.lon = OSM_GPS_MAP_INVALID;
    }
    osm_gps_map_redraw(map);
}

void 
osm_gps_map_osd_draw_balloon (OsmGpsMap *map, float latitude, float longitude, 
                              OsmGpsMapBalloonCallback cb, gpointer data) {
    g_return_if_fail (OSM_IS_GPS_MAP (map));

    osm_gps_map_osd_t *osd = osm_gps_map_osd_get(map);
    g_return_if_fail (osd);

    osd_priv_t *priv = (osd_priv_t*)osd->priv; 
    g_return_if_fail (priv);

    osm_gps_map_osd_clear_balloon (map);

    priv->balloon.lat = latitude;
    priv->balloon.lon = longitude;
    priv->balloon.cb = cb;
    priv->balloon.data = data;
    priv->balloon.just_created = TRUE;
    priv->balloon.orientation = -1;
    priv->balloon.rect.w = 0;
    priv->balloon.rect.h = 0;

    /* set default size and call app callback */
    if(!priv->balloon.rect.w || !priv->balloon.rect.h) {
        osm_gps_map_balloon_event_t event;
        
        priv->balloon.rect.w = BALLOON_AREA_WIDTH;
        priv->balloon.rect.h = BALLOON_AREA_HEIGHT;

        /* create some temporary surface, so the callback can */
        /* e.g. determine text extents etc */
        cairo_surface_t *surface = 
            cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 10,10);

        event.type = OSM_GPS_MAP_BALLOON_EVENT_TYPE_SIZE_REQUEST;
        event.data.draw.rect = &priv->balloon.rect;
        event.data.draw.cr = cairo_create(surface);
        priv->balloon.cb(&event, priv->balloon.data);

        cairo_destroy(event.data.draw.cr);
        cairo_surface_destroy(surface);
    }

    /* allocate balloon surface */
    priv->balloon.surface = 
        cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 
                                   BALLOON_W+2, BALLOON_H+2);

    osd_render_balloon(osd);

    osm_gps_map_redraw(map);
}

#endif // OSD_BALLOON

/* position and extent of bounding box */
#ifndef OSD_X
#define OSD_X      (10)
#endif

#ifndef OSD_Y
#define OSD_Y      (10)
#endif

/* parameters of the direction shape */
#ifndef OSD_DIAMETER
#define D_RAD  (30)         // diameter of dpad
#else
#define D_RAD  (OSD_DIAMETER)
#endif
#define D_TIP  (4*D_RAD/5)  // distance of arrow tip from dpad center
#define D_LEN  (D_RAD/4)    // length of arrow
#define D_WID  (D_LEN)      // width of arrow

/* parameters of the "zoom" pad */
#define Z_STEP   (D_RAD/4)  // distance between dpad and zoom
#define Z_RAD    (D_RAD/2)  // radius of "caps" of zoom bar

#ifdef OSD_SHADOW_ENABLE
/* shadow also depends on control size */
#define OSD_SHADOW (D_RAD/6)
#else
#define OSD_SHADOW (0)
#endif

/* normally the GPS button is in the center of the dpad. if there's */
/* no dpad it will go into the zoom area */
#if defined(OSD_GPS_BUTTON) && defined(OSD_NO_DPAD)
#define Z_GPS  1
#else
#define Z_GPS  0
#endif

/* total width and height of controls incl. shadow */
#define OSD_W    (2*D_RAD + OSD_SHADOW + Z_GPS * 2 * Z_RAD)
#if !Z_GPS
#define OSD_H    (2*D_RAD + Z_STEP + 2*Z_RAD + OSD_SHADOW)
#else
#define OSD_H    (2*Z_RAD + OSD_SHADOW)
#endif

#ifdef OSD_SHADOW_ENABLE
#define OSD_LBL_SHADOW (OSD_SHADOW/2)
#endif

#define Z_TOP    ((1-Z_GPS) * (2 * D_RAD + Z_STEP))

#define Z_MID    (Z_TOP + Z_RAD)
#define Z_BOT    (Z_MID + Z_RAD)
#define Z_LEFT   (Z_RAD)
#define Z_RIGHT  (2 * D_RAD - Z_RAD + Z_GPS * 2 * Z_RAD)
#define Z_CENTER ((Z_RIGHT + Z_LEFT)/2)

/* create the cairo shape used for the zoom buttons */
static void 
osd_zoom_shape(cairo_t *cr, gint x, gint y) 
{
    cairo_move_to (cr, x+Z_LEFT,    y+Z_TOP);
    cairo_line_to (cr, x+Z_RIGHT,   y+Z_TOP);
    cairo_arc     (cr, x+Z_RIGHT,   y+Z_MID, Z_RAD, -M_PI/2,  M_PI/2);
    cairo_line_to (cr, x+Z_LEFT,    y+Z_BOT);
    cairo_arc     (cr, x+Z_LEFT,    y+Z_MID, Z_RAD,  M_PI/2, -M_PI/2);
}

/* ------------------- color/shadow functions ----------------- */

#ifndef OSD_COLOR
/* if no color has been specified we just use the gdks default colors */
static void
osd_labels(cairo_t *cr, gint width, gboolean enabled,
                       GdkColor *fg, GdkColor *disabled) {
    if(enabled)  gdk_cairo_set_source_color(cr, fg);
    else         gdk_cairo_set_source_color(cr, disabled);
    cairo_set_line_width (cr, width);
}
#else
static void
osd_labels(cairo_t *cr, gint width, gboolean enabled) {
    if(enabled)  cairo_set_source_rgb (cr, OSD_COLOR);
    else         cairo_set_source_rgb (cr, OSD_COLOR_DISABLED);
    cairo_set_line_width (cr, width);
}
#endif

#ifdef OSD_SHADOW_ENABLE
static void 
osd_labels_shadow(cairo_t *cr, gint width, gboolean enabled) {
    cairo_set_source_rgba (cr, 0, 0, 0, enabled?0.3:0.15);
    cairo_set_line_width (cr, width);
}
#endif

#ifndef OSD_NO_DPAD
/* create the cairo shape used for the dpad */
static void 
osd_dpad_shape(cairo_t *cr, gint x, gint y) 
{
    cairo_arc (cr, x+D_RAD, y+D_RAD, D_RAD, 0, 2 * M_PI);
}
#endif

#ifdef OSD_SHADOW_ENABLE
static void 
osd_shape_shadow(cairo_t *cr) {
    cairo_set_source_rgba (cr, 0, 0, 0, 0.2);
    cairo_fill (cr);
    cairo_stroke (cr);
}
#endif

#ifndef OSD_COLOR
/* if no color has been specified we just use the gdks default colors */
static void 
osd_shape(cairo_t *cr, GdkColor *bg, GdkColor *fg) {
    gdk_cairo_set_source_color(cr, bg);
    cairo_fill_preserve (cr);
    gdk_cairo_set_source_color(cr, fg);
    cairo_set_line_width (cr, 1);
    cairo_stroke (cr);
}
#else
static void 
osd_shape(cairo_t *cr) {
    cairo_set_source_rgba (cr, OSD_COLOR_BG);
    cairo_fill_preserve (cr);
    cairo_set_source_rgb (cr, OSD_COLOR);
    cairo_set_line_width (cr, 1);
    cairo_stroke (cr);
}
#endif


static gboolean
osm_gps_map_in_circle(gint x, gint y, gint cx, gint cy, gint rad) 
{
    return( pow(cx - x, 2) + pow(cy - y, 2) < rad * rad);
}

#ifndef OSD_NO_DPAD
/* check whether x/y is within the dpad */
static osd_button_t
osd_check_dpad(gint x, gint y) 
{
    /* within entire dpad circle */
    if( osm_gps_map_in_circle(x, y, D_RAD, D_RAD, D_RAD)) 
    {
        /* convert into position relative to dpads centre */
        x -= D_RAD;
        y -= D_RAD;

#ifdef OSD_GPS_BUTTON
        /* check for dpad center goes here! */
        if( osm_gps_map_in_circle(x, y, 0, 0, D_RAD/3)) 
            return OSD_GPS;
#endif

        if( y < 0 && abs(x) < abs(y))
            return OSD_UP;

        if( y > 0 && abs(x) < abs(y))
            return OSD_DOWN;

        if( x < 0 && abs(y) < abs(x))
            return OSD_LEFT;

        if( x > 0 && abs(y) < abs(x))
            return OSD_RIGHT;

        return OSD_BG;
    }
    return OSD_NONE;
}
#endif

/* check whether x/y is within the zoom pads */
static osd_button_t
osd_check_zoom(gint x, gint y) {
    if( x > 0 && x < OSD_W && y > Z_TOP && y < Z_BOT) {

        /* within circle around (-) label */
        if( osm_gps_map_in_circle(x, y, Z_LEFT, Z_MID, Z_RAD)) 
            return OSD_OUT;

        /* within circle around (+) label */
        if( osm_gps_map_in_circle(x, y, Z_RIGHT, Z_MID, Z_RAD)) 
            return OSD_IN;

#if Z_GPS == 1
        /* within square around center */
        if( x > Z_CENTER - Z_RAD && x < Z_CENTER + Z_RAD)
            return OSD_GPS;
#endif

        /* between center of (-) button and center of entire zoom control area */
        if(x > OSD_LEFT && x < D_RAD) 
            return OSD_OUT;

        /* between center of (+) button and center of entire zoom control area */
        if(x < OSD_RIGHT && x > D_RAD) 
            return OSD_IN;
    }
 
    return OSD_NONE;
}

#ifdef OSD_SOURCE_SEL

/* place source selection at right border */
#define OSD_S_RAD (Z_RAD)
#define OSD_S_X   (-OSD_X)
#define OSD_S_Y   (OSD_Y)
#define OSD_S_PW  (2 * Z_RAD)
#define OSD_S_W   (OSD_S_PW)
#define OSD_S_PH  (2 * Z_RAD)
#define OSD_S_H   (OSD_S_PH + OSD_SHADOW)

/* size of usable area when expanded */
#define OSD_S_AREA_W (priv->source_sel.width)
#define OSD_S_AREA_H (priv->source_sel.height)
#define OSD_S_EXP_W  (OSD_S_PW + OSD_S_AREA_W + OSD_SHADOW)
#define OSD_S_EXP_H  (OSD_S_AREA_H + OSD_SHADOW)

/* internal value to draw the arrow on the "puller" */
#define OSD_S_D0  (OSD_S_RAD/2)
#ifndef OSD_FONT_SIZE
#define OSD_FONT_SIZE (16.0)
#endif
#define OSD_TEXT_BORDER   (OSD_FONT_SIZE/2)
#define OSD_TEXT_SKIP     (OSD_FONT_SIZE/8)

/* draw the shape of the source selection OSD, either only the puller (not expanded) */
/* or the entire menu incl. the puller (expanded) */
static void
osd_source_shape(osd_priv_t *priv, cairo_t *cr, gint x, gint y) {
    if(!priv->source_sel.expanded) {
        /* just draw the puller */
        cairo_move_to (cr, x + OSD_S_PW, y + OSD_S_PH);    
        cairo_arc (cr, x+OSD_S_RAD, y+OSD_S_RAD, OSD_S_RAD, M_PI/2, -M_PI/2);
        cairo_line_to (cr, x + OSD_S_PW, y);    
    } else {
        /* draw the puller and the area itself */
        cairo_move_to (cr, x + OSD_S_PW + OSD_S_AREA_W, y + OSD_S_AREA_H);    
        cairo_line_to (cr, x + OSD_S_PW, y + OSD_S_AREA_H);    
        if(OSD_S_Y > 0) {
            cairo_line_to (cr, x + OSD_S_PW, y + OSD_S_PH);    
            cairo_arc (cr, x+OSD_S_RAD, y+OSD_S_RAD, OSD_S_RAD, M_PI/2, -M_PI/2);
        } else {
            cairo_arc (cr, x+OSD_S_RAD, y+OSD_S_AREA_H-OSD_S_RAD, OSD_S_RAD, M_PI/2, -M_PI/2);
            cairo_line_to (cr, x + OSD_S_PW, y + OSD_S_AREA_H - OSD_S_PH);    
            cairo_line_to (cr, x + OSD_S_PW, y);    
        }
        cairo_line_to (cr, x + OSD_S_PW + OSD_S_AREA_W, y);    
        cairo_close_path (cr);
    }
}

static void
osd_source_content(osm_gps_map_osd_t *osd, cairo_t *cr, gint offset) {
    osd_priv_t *priv = (osd_priv_t*)osd->priv; 

    int py = offset + OSD_S_RAD - OSD_S_D0;

    if(!priv->source_sel.expanded) {
        /* draw the "puller" open (<) arrow */
        cairo_move_to (cr, offset + OSD_S_RAD + OSD_S_D0/2, py);
        cairo_rel_line_to (cr, -OSD_S_D0, +OSD_S_D0);
        cairo_rel_line_to (cr, +OSD_S_D0, +OSD_S_D0);
    } else {
        if(OSD_S_Y < 0) 
            py += OSD_S_AREA_H - OSD_S_PH;

        /* draw the "puller" close (>) arrow */
        cairo_move_to (cr, offset + OSD_S_RAD - OSD_S_D0/2, py);
        cairo_rel_line_to (cr, +OSD_S_D0, +OSD_S_D0);
        cairo_rel_line_to (cr, -OSD_S_D0, +OSD_S_D0);
        cairo_stroke(cr);

        /* don't draw a shadow for the text content */
        if(offset == 1) {
            gint source;
            g_object_get(osd->widget, "map-source", &source, NULL);

            cairo_select_font_face (cr, "Sans",
                                    CAIRO_FONT_SLANT_NORMAL,
                                    CAIRO_FONT_WEIGHT_BOLD);
            cairo_set_font_size (cr, OSD_FONT_SIZE);

            int i, step = (priv->source_sel.height - 2*OSD_TEXT_BORDER - OSD_DPIX_SKIP) / 
                (num_map_sources + OSD_DPIX_EXTRA);
            for(i=0;i<num_map_sources + OSD_DPIX_EXTRA;i++) {
                cairo_text_extents_t extents;
                const char *src = NULL;
#if OSD_DPIX_EXTRA > 0
                /* draw "double pixel" check button */
                const char *dpix = _("Double Pixel");
                if(i == num_map_sources) {
                    src = dpix;
                } else
#endif
                src = osm_gps_map_source_get_friendly_name( map_sources[i] );

                cairo_text_extents (cr, src, &extents);
                
                int x = offset + OSD_S_PW + OSD_TEXT_BORDER;
                int y = offset + step * i + OSD_TEXT_BORDER;

#if OSD_DPIX_EXTRA > 0
                if(i == num_map_sources) {
                    int skip3 = OSD_DPIX_SKIP/3;

                    gboolean dpix;
                    g_object_get(osd->widget, "double-pixel", &dpix, NULL);

                    cairo_set_line_width (cr, skip3);

                    /* draw seperator line */
                    cairo_move_to (cr, x, y + skip3);
                    cairo_rel_line_to (cr, OSD_S_AREA_W - 2*OSD_TEXT_BORDER, 0);
                    cairo_stroke(cr);

                    y += OSD_DPIX_SKIP;

                    /* draw check box */
                    cairo_move_to (cr, x, y);
                    cairo_rel_line_to (cr, OSD_FONT_SIZE, 0);
                    cairo_rel_line_to (cr, 0, OSD_FONT_SIZE);
                    cairo_rel_line_to (cr, -OSD_FONT_SIZE, 0);
                    cairo_rel_line_to (cr, 0, -OSD_FONT_SIZE);
                    cairo_stroke(cr);

                    /* draw check if needed */
                    if(dpix) {
                        int space = OSD_FONT_SIZE/4;
                        cairo_move_to (cr, x + space, y + space);
                        cairo_rel_line_to (cr, OSD_FONT_SIZE - 2*space, OSD_FONT_SIZE - 2*space);
                        cairo_stroke(cr);

                        cairo_move_to (cr, x + OSD_FONT_SIZE - space, y + space);
                        cairo_rel_line_to (cr, -(OSD_FONT_SIZE - 2*space), OSD_FONT_SIZE - 2*space);
                        cairo_stroke(cr);
                    }

                    x += 1.5 * OSD_FONT_SIZE;
                }
#endif

                /* draw filled rectangle if selected */
                if(source == map_sources[i]) {
                    cairo_rectangle(cr, x - OSD_TEXT_BORDER/2, 
                                    y - OSD_TEXT_SKIP, 
                                    priv->source_sel.width - OSD_TEXT_BORDER, 
                                    step + OSD_TEXT_SKIP);
                    cairo_fill(cr);

                    /* temprarily draw with background color */
#ifndef OSD_COLOR
                    GdkColor bg = osd->widget->style->bg[GTK_STATE_NORMAL];
                    gdk_cairo_set_source_color(cr, &bg);
#else
                    cairo_set_source_rgba (cr, OSD_COLOR_BG);
#endif
                }

                cairo_move_to (cr, x, y + OSD_TEXT_SKIP - extents.y_bearing);
                cairo_show_text (cr, src);

                /* restore color */
                if(source == map_sources[i]) {
#ifndef OSD_COLOR
                    GdkColor fg = osd->widget->style->fg[GTK_STATE_NORMAL];
                    gdk_cairo_set_source_color(cr, &fg);
#else
                    cairo_set_source_rgb (cr, OSD_COLOR);
#endif
                }
            }
        }
    }
}

static void
osd_render_source_sel(osm_gps_map_osd_t *osd, gboolean force_rerender) {
    osd_priv_t *priv = (osd_priv_t*)osd->priv; 

    if(!priv->source_sel.surface ||
       (priv->source_sel.rendered && !force_rerender))
        return;

    priv->source_sel.rendered = TRUE;

#ifndef OSD_COLOR
    GdkColor bg = GTK_WIDGET(osd->widget)->style->bg[GTK_STATE_NORMAL];
    GdkColor fg = GTK_WIDGET(osd->widget)->style->fg[GTK_STATE_NORMAL];
    GdkColor da = GTK_WIDGET(osd->widget)->style->fg[GTK_STATE_INSENSITIVE];
#endif

    /* draw source selector */
    g_assert(priv->source_sel.surface);
    cairo_t *cr = cairo_create(priv->source_sel.surface);
    cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
    cairo_set_source_rgba(cr, 1.0, 0.0, 0.0, 0.0);
    cairo_paint(cr);
    cairo_set_operator(cr, CAIRO_OPERATOR_OVER);

#ifdef OSD_SHADOW_ENABLE
    osd_source_shape(priv, cr, 1+OSD_SHADOW, 1+OSD_SHADOW);
    osd_shape_shadow(cr);
#endif

    osd_source_shape(priv, cr, 1, 1);
#ifndef OSD_COLOR
    osd_shape(cr, &bg, &fg);
#else
    osd_shape(cr);
#endif

#ifdef OSD_SHADOW_ENABLE
    osd_labels_shadow(cr, Z_RAD/3, TRUE);
    osd_source_content(osd, cr, 1+OSD_LBL_SHADOW);
    cairo_stroke (cr);
#endif
#ifndef OSD_COLOR
    osd_labels(cr, Z_RAD/3, TRUE, &fg, &da);
#else
    osd_labels(cr, Z_RAD/3, TRUE);
#endif
    osd_source_content(osd, cr, 1);
    cairo_stroke (cr);

    cairo_destroy(cr);
}

/* re-allocate the buffer used to draw the menu. This is used */
/* to collapse/expand the buffer */
static void
osd_source_reallocate(osm_gps_map_osd_t *osd) {
    osd_priv_t *priv = (osd_priv_t*)osd->priv; 

    /* re-allocate offscreen bitmap */
    g_assert (priv->source_sel.surface);

    int w = OSD_S_W, h = OSD_S_H;
    if(priv->source_sel.expanded) {
        cairo_text_extents_t extents;

        /* determine content size */
        g_assert(priv->source_sel.surface);
        cairo_t *cr = cairo_create(priv->source_sel.surface);
        cairo_select_font_face (cr, "Sans",
                                CAIRO_FONT_SLANT_NORMAL,
                                CAIRO_FONT_WEIGHT_BOLD);
        cairo_set_font_size (cr, OSD_FONT_SIZE);

        /* calculate menu size */
        int i,  max_w = 0;
        priv->source_sel.max_h = 0;
        for(i=0;i<num_map_sources;i++) {
            const char *src = osm_gps_map_source_get_friendly_name( map_sources[i] );
            cairo_text_extents (cr, src, &extents);

            if(extents.width > max_w) max_w = extents.width;
            if(extents.height > priv->source_sel.max_h) priv->source_sel.max_h = extents.height;
        }
        cairo_destroy(cr);
       
        priv->source_sel.width  = max_w + 2*OSD_TEXT_BORDER;
        priv->source_sel.height = (num_map_sources + OSD_DPIX_EXTRA) * 
            (priv->source_sel.max_h + 2*OSD_TEXT_SKIP) + 2*OSD_TEXT_BORDER + OSD_DPIX_SKIP;

        w = OSD_S_EXP_W;
        h = OSD_S_EXP_H;
    }

    cairo_surface_destroy(priv->source_sel.surface);
    priv->source_sel.surface = 
        cairo_image_surface_create(CAIRO_FORMAT_ARGB32, w+2, h+2);

    osd_render_source_sel(osd, TRUE);
}

#define OSD_HZ      15
#define OSD_TIME    500

static gboolean osd_source_animate(gpointer data) {
    osm_gps_map_osd_t *osd = (osm_gps_map_osd_t*)data;
    osd_priv_t *priv = (osd_priv_t*)osd->priv; 
    int diff = OSD_S_EXP_W - OSD_S_W - OSD_S_X;
    gboolean done = FALSE;
    priv->source_sel.count += priv->source_sel.dir;

    /* shifting in */
    if(priv->source_sel.dir < 0) {
        if(priv->source_sel.count <= 0) {
            priv->source_sel.count = 0;
            done = TRUE;
        }
    } else {
        if(priv->source_sel.count >= 1000) {
            priv->source_sel.expanded = FALSE;
            osd_source_reallocate(osd);

            priv->source_sel.count = 1000;
            done = TRUE;
        }
    }


    /* count runs linearly from 0 to 1000, map this nicely onto a position */

    /* nice sinoid mapping */
    float m = 0.5-cos(priv->source_sel.count * M_PI / 1000.0)/2;
    priv->source_sel.shift = 
        (osd->widget->allocation.width - OSD_S_EXP_W + OSD_S_X) + 
        m * diff;

    /* make sure the screen is updated */
    osm_gps_map_repaint(OSM_GPS_MAP(osd->widget));

    /* stop animation if done */
    if(done) 
        priv->source_sel.handler_id = 0;

    return !done;
}

/* switch between expand and collapse mode of source selection */
static void
osd_source_toggle(osm_gps_map_osd_t *osd)
{
    osd_priv_t *priv = (osd_priv_t*)osd->priv; 

    /* ignore clicks while animation is running */
    if(priv->source_sel.handler_id)
        return;

    /* expand immediately, collapse is handle at the end of the */
    /* collapse animation */
    if(!priv->source_sel.expanded) {
        priv->source_sel.expanded = TRUE;
        osd_source_reallocate(osd);

        priv->source_sel.count = 1000;
        priv->source_sel.shift = osd->widget->allocation.width - OSD_S_W;
        priv->source_sel.dir = -1000/OSD_HZ;
    } else {
        priv->source_sel.count =  0;
        priv->source_sel.shift = osd->widget->allocation.width - 
            OSD_S_EXP_W + OSD_S_X;
        priv->source_sel.dir = +1000/OSD_HZ;
    }

    /* start timer to handle animation */
    priv->source_sel.handler_id = gtk_timeout_add(OSD_TIME/OSD_HZ, 
                                                  osd_source_animate, osd);
}

/* check if the user clicked inside the source selection area */
static osd_button_t
osd_source_check(osm_gps_map_osd_t *osd, gboolean down, gint x, gint y) {
    osd_priv_t *priv = (osd_priv_t*)osd->priv; 

    if(!priv->source_sel.expanded)
        x -= osd->widget->allocation.width - OSD_S_W;
    else
        x -= osd->widget->allocation.width - OSD_S_EXP_W + OSD_S_X;

    if(OSD_S_Y > 0)
        y -= OSD_S_Y;
    else
        y -= osd->widget->allocation.height - OSD_S_PH + OSD_S_Y;
    
    /* within square around puller? */
    if(y > 0 && y < OSD_S_PH && x > 0 && x < OSD_S_PW) {
        /* really within puller shape? */
        if(x > Z_RAD || osm_gps_map_in_circle(x, y, Z_RAD, Z_RAD, Z_RAD)) {
            /* expand source selector */
            if(down)
                osd_source_toggle(osd);

            /* tell upper layers that user clicked some background element */
            /* of the OSD */
            return OSD_BG;
        }
    }

    /* check for clicks into data area */
    if(priv->source_sel.expanded && !priv->source_sel.handler_id) {
        /* re-adjust from puller top to content top */
        if(OSD_S_Y < 0)
            y += OSD_S_EXP_H - OSD_S_PH;

        if(x > OSD_S_PW && 
           x < OSD_S_PW + OSD_S_EXP_W &&
           y > 0 &&
           y < OSD_S_EXP_H) {
            
            int step = (priv->source_sel.height - 2*OSD_TEXT_BORDER - OSD_DPIX_SKIP) 
                / (num_map_sources + OSD_DPIX_EXTRA);

            y -= OSD_TEXT_BORDER - OSD_TEXT_SKIP;
            int py = y / step;

            if(down) {
                gint old = 0;
                g_object_get(osd->widget, "map-source", &old, NULL);

                if(py >= 0 &&
                   py < num_map_sources &&
                   old != map_sources[py]) {
                    g_object_set(osd->widget, "map-source", map_sources[py], NULL);
                    
                    osd_render_source_sel(osd, TRUE);
                    osm_gps_map_repaint(OSM_GPS_MAP(osd->widget));
                    osm_gps_map_redraw(OSM_GPS_MAP(osd->widget));
                }

#if OSD_DPIX_EXTRA > 0
                if(py >= num_map_sources) {
                    y -= num_map_sources * (priv->source_sel.max_h + 2*OSD_TEXT_SKIP ) +
                        OSD_TEXT_SKIP + OSD_DPIX_SKIP;

                    if(y >= 0) {
                        gboolean dpix = 0;
                        g_object_get(osd->widget, "double-pixel", &dpix, NULL);
                        g_object_set(osd->widget, "double-pixel", !dpix, NULL);
                    
                        osd_render_source_sel(osd, TRUE);
                        osm_gps_map_repaint(OSM_GPS_MAP(osd->widget));
                        osm_gps_map_redraw(OSM_GPS_MAP(osd->widget));
                    }
                }
#endif
            }
            
            /* return "clicked in OSD background" to prevent further */
            /* processing by application */
            return OSD_BG;
        }
    }

    return OSD_NONE;
}
#endif // OSD_SOURCE_SEL

#ifndef OSD_NO_DPAD
static void
osd_dpad_labels(cairo_t *cr, gint x, gint y) {
    /* move reference to dpad center */
    x += D_RAD;
    y += D_RAD;

    const static gint offset[][3][2] = {
        /* left arrow/triangle */
        { { -D_TIP+D_LEN, -D_WID }, { -D_LEN, D_WID }, { +D_LEN, D_WID } },
        /* right arrow/triangle */
        { { +D_TIP-D_LEN, -D_WID }, { +D_LEN, D_WID }, { -D_LEN, D_WID } },
        /* top arrow/triangle */
        { { -D_WID, -D_TIP+D_LEN }, { D_WID, -D_LEN }, { D_WID, +D_LEN } },
        /* bottom arrow/triangle */
        { { -D_WID, +D_TIP-D_LEN }, { D_WID, +D_LEN }, { D_WID, -D_LEN } }
    };

    int i;
    for(i=0;i<4;i++) {
        cairo_move_to (cr, x + offset[i][0][0], y + offset[i][0][1]);
        cairo_rel_line_to (cr, offset[i][1][0], offset[i][1][1]);
        cairo_rel_line_to (cr, offset[i][2][0], offset[i][2][1]);
    }
}
#endif

#ifdef OSD_GPS_BUTTON
/* draw the satellite dish icon in the center of the dpad */
#define GPS_V0  (D_RAD/7)
#define GPS_V1  (D_RAD/10)
#define GPS_V2  (D_RAD/5)

/* draw a satellite receiver dish */
/* this is either drawn in the center of the dpad (if present) */
/* or in the middle of the zoom area */
static void
osd_dpad_gps(cairo_t *cr, gint x, gint y) {
    /* move reference to dpad center */
    x += (1-Z_GPS) * D_RAD + Z_GPS * Z_RAD * 3;
    y += (1-Z_GPS) * D_RAD + Z_GPS * Z_RAD + GPS_V0;

    cairo_move_to (cr, x-GPS_V0, y+GPS_V0);
    cairo_rel_line_to (cr, +GPS_V0, -GPS_V0);
    cairo_rel_line_to (cr, +GPS_V0, +GPS_V0);
    cairo_close_path (cr);

    cairo_move_to (cr, x+GPS_V1-GPS_V2, y-2*GPS_V2);
    cairo_curve_to (cr, x-GPS_V2, y, x+GPS_V1, y+GPS_V1, x+GPS_V1+GPS_V2, y);
    cairo_close_path (cr);

    x += GPS_V1;
    cairo_move_to (cr, x, y-GPS_V2);
    cairo_rel_line_to (cr, +GPS_V1, -GPS_V1);
}
#endif

#define Z_LEN  (2*Z_RAD/3)

static void
osd_zoom_labels(cairo_t *cr, gint x, gint y) {
    cairo_move_to (cr, x + Z_LEFT  - Z_LEN, y + Z_MID);
    cairo_line_to (cr, x + Z_LEFT  + Z_LEN, y + Z_MID);

    cairo_move_to (cr, x + Z_RIGHT,         y + Z_MID - Z_LEN);
    cairo_line_to (cr, x + Z_RIGHT,         y + Z_MID + Z_LEN);
    cairo_move_to (cr, x + Z_RIGHT - Z_LEN, y + Z_MID);
    cairo_line_to (cr, x + Z_RIGHT + Z_LEN, y + Z_MID);
}

#ifdef OSD_COORDINATES

#ifndef OSD_COORDINATES_FONT_SIZE
#define OSD_COORDINATES_FONT_SIZE (12.0)
#endif

#define OSD_COORDINATES_OFFSET (OSD_COORDINATES_FONT_SIZE/6)

#define OSD_COORDINATES_W  (8*OSD_COORDINATES_FONT_SIZE+2*OSD_COORDINATES_OFFSET)
#define OSD_COORDINATES_H  (2*OSD_COORDINATES_FONT_SIZE+2*OSD_COORDINATES_OFFSET+OSD_COORDINATES_FONT_SIZE/4)

/* these can be overwritten with versions that support */
/* localization */
#ifndef OSD_COORDINATES_CHR_N
#define OSD_COORDINATES_CHR_N  "N"
#endif
#ifndef OSD_COORDINATES_CHR_S
#define OSD_COORDINATES_CHR_S  "S"
#endif
#ifndef OSD_COORDINATES_CHR_E
#define OSD_COORDINATES_CHR_E  "E"
#endif
#ifndef OSD_COORDINATES_CHR_W
#define OSD_COORDINATES_CHR_W  "W"
#endif



/* this is the classic geocaching notation */
static char 
*osd_latitude_str(float latitude) {
    char *c = OSD_COORDINATES_CHR_N;
    float integral, fractional;
    
    if(isnan(latitude)) 
        return NULL;
    
    if(latitude < 0) { 
        latitude = fabs(latitude); 
        c = OSD_COORDINATES_CHR_S; 
    }

    fractional = modff(latitude, &integral);
    
    return g_strdup_printf("%s %02d° %06.3f'", 
                           c, (int)integral, fractional*60.0);
}

static char 
*osd_longitude_str(float longitude) {
    char *c = OSD_COORDINATES_CHR_E;
    float integral, fractional;
    
    if(isnan(longitude)) 
        return NULL;
    
    if(longitude < 0) { 
        longitude = fabs(longitude); 
        c = OSD_COORDINATES_CHR_W; 
    }

    fractional = modff(longitude, &integral);
    
    return g_strdup_printf("%s %03d° %06.3f'", 
                           c, (int)integral, fractional*60.0);
}

/* render a string at the given screen position */
static int
osd_render_centered_text(cairo_t *cr, int y, int width, char *text) {
    if(!text) return y;

    char *p = g_malloc(strlen(text)+4);  // space for "...\n"
    strcpy(p, text);

    cairo_text_extents_t extents;
    memset(&extents, 0, sizeof(cairo_text_extents_t));
    cairo_text_extents (cr, p, &extents);
    g_assert(extents.width != 0.0);

    /* check if text needs to be truncated */
    int trunc_at = strlen(text);
    while(extents.width > width) {

        /* cut off all utf8 multibyte remains so the actual */
        /* truncation only deals with one byte */
        while((p[trunc_at-1] & 0xc0) == 0x80) {
            trunc_at--;
            g_assert(trunc_at > 0);
        }

        trunc_at--;
        g_assert(trunc_at > 0);

        strcpy(p+trunc_at, "...");
        cairo_text_extents (cr, p, &extents);
    }

    cairo_set_source_rgb(cr, 1.0, 1.0, 1.0);
    cairo_set_line_width (cr, OSD_COORDINATES_FONT_SIZE/6);
    cairo_move_to (cr, (width - extents.width)/2, y - extents.y_bearing);
    cairo_text_path (cr, p);
    cairo_stroke (cr);

    cairo_set_source_rgb(cr, 0.0, 0.0, 0.0);
    cairo_move_to (cr, (width - extents.width)/2, y - extents.y_bearing);
    cairo_show_text (cr, p);

    g_free(p);

    /* skip + 1/5 line */
    return y + 6*OSD_COORDINATES_FONT_SIZE/5;
}

static void
osd_render_coordinates(osm_gps_map_osd_t *osd) 
{
    osd_priv_t *priv = (osd_priv_t*)osd->priv; 

    if(!priv->coordinates.surface)
        return;

    /* get current map position */
    gfloat lat, lon;
    g_object_get(osd->widget, "latitude", &lat, "longitude", &lon, NULL);

    /* check if position has changed enough to require redraw */
    if(!isnan(priv->coordinates.lat) && !isnan(priv->coordinates.lon))
        /* 1/60000 == 1/1000 minute */
        if((fabsf(lat - priv->coordinates.lat) < 1/60000) &&
           (fabsf(lon - priv->coordinates.lon) < 1/60000))
            return;

    priv->coordinates.lat = lat;
    priv->coordinates.lon = lon;

    /* first fill with transparency */

    g_assert(priv->coordinates.surface);
    cairo_t *cr = cairo_create(priv->coordinates.surface);
    cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
    //    cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, 0.5);
    cairo_set_source_rgba(cr, 0.0, 0.0, 0.0, 0.0);
    cairo_paint(cr);
    cairo_set_operator(cr, CAIRO_OPERATOR_OVER);

    cairo_select_font_face (cr, "Sans",
                            CAIRO_FONT_SLANT_NORMAL,
                            CAIRO_FONT_WEIGHT_BOLD);
    cairo_set_font_size (cr, OSD_COORDINATES_FONT_SIZE);

    char *latitude = osd_latitude_str(lat);
    char *longitude = osd_longitude_str(lon);
    
    int y = OSD_COORDINATES_OFFSET;
    y = osd_render_centered_text(cr, y, OSD_COORDINATES_W, latitude);
    y = osd_render_centered_text(cr, y, OSD_COORDINATES_W, longitude);
    
    g_free(latitude);
    g_free(longitude);

    cairo_destroy(cr);
}
#endif  // OSD_COORDINATES

#ifdef OSD_NAV
#define OSD_NAV_W  (8*OSD_COORDINATES_FONT_SIZE+2*OSD_COORDINATES_OFFSET)
#define OSD_NAV_H  (11*OSD_COORDINATES_FONT_SIZE)

/* http://mathforum.org/library/drmath/view/55417.html */
static float get_bearing(float lat1, float lon1, float lat2, float lon2) {
  return atan2( sin(lon2 - lon1) * cos(lat2),
                cos(lat1) * sin(lat2) - 
                sin(lat1) * cos(lat2) * cos(lon2 - lon1));
}

/* http://mathforum.org/library/drmath/view/51722.html */
static float get_distance(float lat1, float lon1, float lat2, float lon2) {
  float aob = acos(cos(lat1) * cos(lat2) * cos(lon2 - lon1) +
		   sin(lat1) * sin(lat2));

  return(aob * 6371000.0);     /* great circle radius in meters */
}

static void
osd_render_nav(osm_gps_map_osd_t *osd) 
{
    osd_priv_t *priv = (osd_priv_t*)osd->priv; 

    if(!priv->nav.surface || isnan(priv->nav.lat) || isnan(priv->nav.lon))
        return;

    /* first fill with transparency */
    g_assert(priv->nav.surface);
    cairo_t *cr = cairo_create(priv->nav.surface);
    cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
    cairo_set_source_rgba(cr, 1.0, 0.0, 0.0, 0.0);
    cairo_paint(cr);
    cairo_set_operator(cr, CAIRO_OPERATOR_OVER);

    cairo_select_font_face (cr, "Sans",
                            CAIRO_FONT_SLANT_NORMAL,
                            CAIRO_FONT_WEIGHT_BOLD);
    cairo_set_font_size (cr, OSD_COORDINATES_FONT_SIZE);

    char *latitude = osd_latitude_str(priv->nav.lat);
    char *longitude = osd_longitude_str(priv->nav.lon);
    
    int y = OSD_COORDINATES_OFFSET;
    y = osd_render_centered_text(cr, y, OSD_NAV_W, priv->nav.name);
    y = osd_render_centered_text(cr, y, OSD_NAV_W, latitude);
    y = osd_render_centered_text(cr, y, OSD_NAV_W, longitude);

    g_free(latitude);
    g_free(longitude);

    /* everything below this is the compass. we need to know this */
    /* to differ between coordinate clicks and compass clicks */
    priv->nav.click_sep = y;
    
    /* draw the compass */
    int radius = (OSD_NAV_H - y - 5*OSD_COORDINATES_FONT_SIZE/4)/2;
    if(radius > OSD_NAV_W/2)
        radius = OSD_NAV_W/2;

    int x = OSD_NAV_W/2+1;
    y += radius;

    cairo_stroke (cr);

    /* draw background */
    cairo_arc(cr, x, y, radius, 0,  2*M_PI);
    cairo_set_source_rgba (cr, 1, 1, 1, 0.5);
    cairo_fill_preserve (cr);
    cairo_set_source_rgb (cr, 0, 0, 0);
    cairo_set_line_width (cr, 1);
    cairo_stroke (cr);

    /* draw pointer */
#define ARROW_WIDTH     0.3
#define ARROW_LENGTH    0.7

    coord_t mpos, *pos = osm_gps_map_get_gps (OSM_GPS_MAP(osd->widget));
    if(priv->nav.mode) {
        gfloat lat, lon;
        g_object_get(osd->widget, "latitude", &lat, "longitude", &lon, NULL);
        mpos.rlat = deg2rad(lat);
        mpos.rlon = deg2rad(lon);
        pos = &mpos;
    }

    if(pos) {
        float arot = get_bearing(pos->rlat, pos->rlon, 
                     deg2rad(priv->nav.lat), deg2rad(priv->nav.lon));

        cairo_move_to(cr, 
                      x + radius *  ARROW_LENGTH *  sin(arot),
                      y + radius *  ARROW_LENGTH * -cos(arot));
        
        cairo_line_to(cr,
                      x + radius * -ARROW_LENGTH *  sin(arot+ARROW_WIDTH),
                      y + radius * -ARROW_LENGTH * -cos(arot+ARROW_WIDTH));
        
        cairo_line_to(cr,
                      x + radius * -0.5 * ARROW_LENGTH *  sin(arot),
                      y + radius * -0.5 * ARROW_LENGTH * -cos(arot));
        
        cairo_line_to(cr,
                      x + radius * -ARROW_LENGTH *  sin(arot-ARROW_WIDTH),
                      y + radius * -ARROW_LENGTH * -cos(arot-ARROW_WIDTH));
        
        cairo_close_path(cr);
        if(priv->nav.mode) 
            cairo_set_source_rgb (cr, 0, 0, 0);
        else
            cairo_set_source_rgb (cr, 0, 0, 0.8);

        cairo_fill (cr);

        y += radius + OSD_COORDINATES_FONT_SIZE/4;

        float dist = get_distance(pos->rlat, pos->rlon, 
                        deg2rad(priv->nav.lat), deg2rad(priv->nav.lon));

        char *dist_str = NULL;
        if(!priv->nav.imperial) {
            /* metric is easy ... */
            if(dist<1000) 
                dist_str = g_strdup_printf("%u m", (int)dist);
            else          
                dist_str = g_strdup_printf("%.1f km", dist/1000);
        } else {
            /* and now the hard part: scale for useful imperial values :-( */
            /* try to convert to feet, 1ft == 0.3048 m */

            if(dist/(3*0.3048) >= 1760.0)      /* more than 1760 yard? */
                dist_str = g_strdup_printf("%.1f mi", dist/(0.3048*3*1760.0));
            else if(dist/0.3048 >= 100)        /* more than 100 feet? */
                dist_str = g_strdup_printf("%.1f yd", dist/(0.3048*3));
            else
                dist_str = g_strdup_printf("%.0f ft", dist/0.3048);
        }
        
        y = osd_render_centered_text(cr, y, OSD_NAV_W, dist_str);
        g_free(dist_str);
    }

    cairo_destroy(cr);
}

/* check if the user clicked inside the source selection area */
static osd_button_t
osd_nav_check(osm_gps_map_osd_t *osd, gboolean down, gint x, gint y) {
    osd_priv_t *priv = (osd_priv_t*)osd->priv; 
    
    if(!priv->nav.surface || down) 
        return OSD_NONE;

    x -= OSD_X;
    if(OSD_X < 0)
        x -= (osd->widget->allocation.width - OSD_NAV_W);
    
    y -= (osd->widget->allocation.height - OSD_NAV_H)/2;

    if(x >= 0 && y >= 0 && x <= OSD_NAV_W && y <= OSD_NAV_H) {
        if(y < priv->nav.click_sep)
            osm_gps_map_set_center(OSM_GPS_MAP(osd->widget), 
                                   priv->nav.lat, priv->nav.lon);
        else {
            priv->nav.mode = !priv->nav.mode;
            osm_gps_map_redraw(OSM_GPS_MAP(osd->widget));
        }
    }

    return OSD_NONE;
}

void osm_gps_map_osd_clear_nav (OsmGpsMap *map) {
    g_return_if_fail (OSM_IS_GPS_MAP (map));

    osm_gps_map_osd_t *osd = osm_gps_map_osd_get(map);
    g_return_if_fail (osd);

    osd_priv_t *priv = (osd_priv_t*)osd->priv; 
    g_return_if_fail (priv);

    if(priv->nav.surface) {
        cairo_surface_destroy(priv->nav.surface);
        priv->nav.surface = NULL;
        priv->nav.lat = OSM_GPS_MAP_INVALID;
        priv->nav.lon = OSM_GPS_MAP_INVALID;
        if(priv->nav.name) g_free(priv->nav.name);
    }
    osm_gps_map_redraw(map);
}

void 
osm_gps_map_osd_draw_nav (OsmGpsMap *map, gboolean imperial,
                          float latitude, float longitude, char *name) {
    g_return_if_fail (OSM_IS_GPS_MAP (map));

    osm_gps_map_osd_t *osd = osm_gps_map_osd_get(map);
    g_return_if_fail (osd);

    osd_priv_t *priv = (osd_priv_t*)osd->priv; 
    g_return_if_fail (priv);

    osm_gps_map_osd_clear_nav (map);

    /* allocate balloon surface */
    priv->nav.surface = 
        cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 
                                   OSD_NAV_W+2, OSD_NAV_H+2);

    priv->nav.lat = latitude;
    priv->nav.lon = longitude;
    priv->nav.name = g_strdup(name);
    priv->nav.imperial = imperial;

    osd_render_nav(osd);

    osm_gps_map_redraw(map);
}

#endif // OSD_NAV

#ifdef OSD_HEARTRATE
#ifndef OSD_HR_FONT_SIZE
#define OSD_HR_FONT_SIZE 60
#endif
#define OSD_HR_W  3*OSD_HR_FONT_SIZE
#define OSD_HR_H  OSD_HR_FONT_SIZE

#ifndef OSD_HR_Y
#define OSD_HR_Y OSD_Y
#endif

static void
osd_render_heart_shape(cairo_t *cr, gint x, gint y, gint s) {
    cairo_move_to (cr, x-s, y-s/4);
    cairo_curve_to (cr, x-s, y-s,     x,   y-s,     x,   y-s/4);
    cairo_curve_to (cr, x,   y-s,     x+s, y-s,     x+s, y-s/4);
    cairo_curve_to (cr, x+s, y+s/2,   x,   y+3*s/4, x,   y+s);
    cairo_curve_to (cr, x,   y+3*s/4, x-s, y+s/2,   x-s, y-s/4);
}

static void 
osd_render_heart(cairo_t *cr, gint x, gint y, gint s, gboolean ok) {
    /* xyz */

    cairo_set_line_join (cr, CAIRO_LINE_JOIN_ROUND);

    osd_render_heart_shape(cr, x, y, s);
    cairo_set_source_rgb (cr, 1, 1, 1);
    cairo_set_line_width (cr, s/2);
    cairo_stroke(cr);

    osd_render_heart_shape(cr, x, y, s);
    if(ok) cairo_set_source_rgb (cr, 1, 0, 0);
    else   cairo_set_source_rgb (cr, 0.8, 0.8, 0.8);
    cairo_fill_preserve (cr);
    cairo_set_line_width (cr, s/5);

    cairo_stroke(cr);
}

static void
osd_render_hr(osm_gps_map_osd_t *osd) 
{
    osd_priv_t *priv = (osd_priv_t*)osd->priv; 

    if(!priv->hr.surface)
        return;

    /* first fill with transparency */
    g_assert(priv->hr.surface);
    cairo_t *cr = cairo_create(priv->hr.surface);
    cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
    cairo_set_source_rgba(cr, 0.0, 0.0, 0.0, 0.0);
    cairo_paint(cr);
    cairo_set_operator(cr, CAIRO_OPERATOR_OVER);

    cairo_select_font_face (cr, "Sans",
                            CAIRO_FONT_SLANT_NORMAL,
                            CAIRO_FONT_WEIGHT_BOLD);
    cairo_set_font_size (cr, OSD_HR_FONT_SIZE);

    char str[5];
    if(priv->hr.rate > 0) 
        snprintf(str, 4, "%03u", priv->hr.rate);
    else if(priv->hr.rate == OSD_HR_INVALID) 
        strcpy(str, "?");
    else if(priv->hr.rate == OSD_HR_ERROR) 
        strcpy(str, "Err");
    else 
        g_assert(priv->hr.rate != OSD_HR_NONE);

    cairo_text_extents_t extents;
    cairo_text_extents (cr, str, &extents);

    cairo_set_source_rgb(cr, 1.0, 1.0, 1.0);
    cairo_set_line_width (cr, OSD_HR_FONT_SIZE/10);

    cairo_move_to (cr, OSD_HR_W/2 - extents.width/2 + OSD_HR_FONT_SIZE/4, 
                       OSD_HR_H/2 - extents.y_bearing/2);
    cairo_text_path (cr, str);
    cairo_stroke (cr);

    cairo_set_source_rgb(cr, 0.0, 0.0, 0.0);
    cairo_move_to (cr, OSD_HR_W/2 - extents.width/2 + OSD_HR_FONT_SIZE/4, 
                       OSD_HR_H/2 - extents.y_bearing/2);
    cairo_show_text (cr, str);

    osd_render_heart(cr, OSD_HR_W/2 - extents.width/2 - OSD_HR_FONT_SIZE/8, 
                     OSD_HR_H/2, OSD_HR_FONT_SIZE/5, priv->hr.ok);
    

    cairo_destroy(cr);
}

void 
osm_gps_map_osd_draw_hr (OsmGpsMap *map, gboolean ok, gint rate) {
    g_return_if_fail (OSM_IS_GPS_MAP (map));

    osm_gps_map_osd_t *osd = osm_gps_map_osd_get(map);
    g_return_if_fail (osd);

    osd_priv_t *priv = (osd_priv_t*)osd->priv; 
    g_return_if_fail (priv);

    /* allocate heart rate surface */
    if(rate != OSD_HR_NONE && !priv->hr.surface)
        priv->hr.surface = 
            cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 
                                       OSD_HR_W, OSD_HR_H);

    if(rate == OSD_HR_NONE && priv->hr.surface) {
        cairo_surface_destroy(priv->hr.surface);
        priv->hr.surface = NULL;
    }

    if(priv->hr.rate != rate || priv->hr.ok != ok) {
        priv->hr.rate = rate;
        priv->hr.ok = ok;
        osd_render_hr(osd);
    }
    
    osm_gps_map_redraw(map);
}
 
#endif // OSD_HEARTRATE
 
static osd_button_t
osd_check_int(osm_gps_map_osd_t *osd, gboolean click, gboolean down, gint x, gint y) {
    osd_button_t but = OSD_NONE;

#ifdef OSD_BALLOON
    if(down) {
        /* needed to handle balloons that are created at click */
        osd_priv_t *priv = (osd_priv_t*)osd->priv; 
        priv->balloon.just_created = FALSE;
    }
#endif

#ifdef OSD_SOURCE_SEL
    /* the source selection area is handles internally */
    but = osd_source_check(osd, down, x, y);
#endif

#ifdef OSD_NAV
    if(but == OSD_NONE) {
        /* the source selection area is handles internally */
        but = osd_nav_check(osd, down, x, y);
    }
#endif
        
    if(but == OSD_NONE) {
        gint mx = x - OSD_X;
        gint my = y - OSD_Y;
    
        if(OSD_X < 0)
            mx -= (osd->widget->allocation.width - OSD_W);
    
        if(OSD_Y < 0)
            my -= (osd->widget->allocation.height - OSD_H);
    
        /* first do a rough test for the OSD area. */
        /* this is just to avoid an unnecessary detailed test */
        if(mx > 0 && mx < OSD_W && my > 0 && my < OSD_H) {
#ifndef OSD_NO_DPAD
            but = osd_check_dpad(mx, my);
#endif
        }

        if(but == OSD_NONE) 
            but = osd_check_zoom(mx, my);
    }

#ifdef OSD_BALLOON
    if(but == OSD_NONE) {
        /* check if user clicked into balloon */
        if(osd_balloon_check(osd, click, down, x, y)) 
            but = OSD_BG;
    }
#endif
        
    return but;
}


#ifdef OSD_CROSSHAIR

#ifndef OSD_CROSSHAIR_RADIUS
#define OSD_CROSSHAIR_RADIUS 10
#endif

#define OSD_CROSSHAIR_TICK  (OSD_CROSSHAIR_RADIUS/2)
#define OSD_CROSSHAIR_BORDER (OSD_CROSSHAIR_TICK + OSD_CROSSHAIR_RADIUS/4)
#define OSD_CROSSHAIR_W  ((OSD_CROSSHAIR_RADIUS+OSD_CROSSHAIR_BORDER)*2)
#define OSD_CROSSHAIR_H  ((OSD_CROSSHAIR_RADIUS+OSD_CROSSHAIR_BORDER)*2)

static void
osd_render_crosshair_shape(cairo_t *cr) {
    cairo_arc (cr, OSD_CROSSHAIR_W/2, OSD_CROSSHAIR_H/2, 
               OSD_CROSSHAIR_RADIUS, 0,  2*M_PI);

    cairo_move_to (cr, OSD_CROSSHAIR_W/2 - OSD_CROSSHAIR_RADIUS, 
                   OSD_CROSSHAIR_H/2);
    cairo_rel_line_to (cr, -OSD_CROSSHAIR_TICK, 0);
    cairo_move_to (cr, OSD_CROSSHAIR_W/2 + OSD_CROSSHAIR_RADIUS, 
                   OSD_CROSSHAIR_H/2);
    cairo_rel_line_to (cr,  OSD_CROSSHAIR_TICK, 0);

    cairo_move_to (cr, OSD_CROSSHAIR_W/2,
                   OSD_CROSSHAIR_H/2 - OSD_CROSSHAIR_RADIUS);
    cairo_rel_line_to (cr, 0, -OSD_CROSSHAIR_TICK);
    cairo_move_to (cr, OSD_CROSSHAIR_W/2,
                   OSD_CROSSHAIR_H/2 + OSD_CROSSHAIR_RADIUS);
    cairo_rel_line_to (cr, 0, OSD_CROSSHAIR_TICK);

    cairo_stroke (cr);
}

static void
osd_render_crosshair(osm_gps_map_osd_t *osd) 
{
    osd_priv_t *priv = (osd_priv_t*)osd->priv; 

    if(!priv->crosshair.surface || priv->crosshair.rendered)
        return;

    priv->crosshair.rendered = TRUE;

    /* first fill with transparency */
    g_assert(priv->crosshair.surface);
    cairo_t *cr = cairo_create(priv->crosshair.surface);
    cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
    cairo_set_source_rgba(cr, 0.0, 0.0, 0.0, 0.0);
    cairo_paint(cr);
    cairo_set_operator(cr, CAIRO_OPERATOR_OVER);

    cairo_set_line_cap  (cr, CAIRO_LINE_CAP_ROUND);
   
    cairo_set_source_rgba (cr, 1.0, 1.0, 1.0, 0.5);
    cairo_set_line_width (cr, OSD_CROSSHAIR_RADIUS/2);
    osd_render_crosshair_shape(cr);

    cairo_set_source_rgba (cr, 0.0, 0.0, 0.0, 0.5);
    cairo_set_line_width (cr, OSD_CROSSHAIR_RADIUS/4);
    osd_render_crosshair_shape(cr);

    cairo_destroy(cr);
}
#endif

#ifdef OSD_SCALE

#ifndef OSD_SCALE_FONT_SIZE
#define OSD_SCALE_FONT_SIZE (12.0)
#endif
#define OSD_SCALE_W   (10*OSD_SCALE_FONT_SIZE)
#define OSD_SCALE_H   (5*OSD_SCALE_FONT_SIZE/2)

/* various parameters used to create the scale */
#define OSD_SCALE_H2   (OSD_SCALE_H/2)
#define OSD_SCALE_TICK (2*OSD_SCALE_FONT_SIZE/3)
#define OSD_SCALE_M    (OSD_SCALE_H2 - OSD_SCALE_TICK)
#define OSD_SCALE_I    (OSD_SCALE_H2 + OSD_SCALE_TICK)
#define OSD_SCALE_FD   (OSD_SCALE_FONT_SIZE/4)

static void
osd_render_scale(osm_gps_map_osd_t *osd) 
{
    osd_priv_t *priv = (osd_priv_t*)osd->priv; 

    if(!priv->scale.surface)
        return;

    /* this only needs to be rendered if the zoom has changed */
    gint zoom;
    g_object_get(OSM_GPS_MAP(osd->widget), "zoom", &zoom, NULL);
    if(zoom == priv->scale.zoom)
        return;

    priv->scale.zoom = zoom;

    float m_per_pix = osm_gps_map_get_scale(OSM_GPS_MAP(osd->widget));

    /* first fill with transparency */
    g_assert(priv->scale.surface);
    cairo_t *cr = cairo_create(priv->scale.surface);
    cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
    cairo_set_source_rgba(cr, 1.0, 0.0, 0.0, 0.0);
    // pink for testing:    cairo_set_source_rgba(cr, 1.0, 0.0, 0.0, 0.2);
    cairo_paint(cr);
    cairo_set_operator(cr, CAIRO_OPERATOR_OVER);

    /* determine the size of the scale width in meters */
    float width = (OSD_SCALE_W-OSD_SCALE_FONT_SIZE/6) * m_per_pix;
    
    /* scale this to useful values */
    int exp = logf(width)*M_LOG10E;
    int mant = width/pow(10,exp);
    int width_metric = mant * pow(10,exp);
    char *dist_str = NULL;
    if(width_metric<1000) 
        dist_str = g_strdup_printf("%u m", width_metric);
    else          
        dist_str = g_strdup_printf("%u km", width_metric/1000);
    width_metric /= m_per_pix;

    /* and now the hard part: scale for useful imperial values :-( */
    /* try to convert to feet, 1ft == 0.3048 m */
    width /= 0.3048;
    float imp_scale = 0.3048;
    char *dist_imp_unit = "ft";

    if(width >= 100) {
        /* 1yd == 3 feet */
        width /= 3.0;
        imp_scale *= 3.0;
        dist_imp_unit = "yd";

        if(width >= 1760.0) {
            /* 1mi == 1760 yd */
            width /= 1760.0;
            imp_scale *= 1760.0;
            dist_imp_unit = "mi";
        }
    }

    /* also convert this to full tens/hundreds */
    exp = logf(width)*M_LOG10E;
    mant = width/pow(10,exp);
    int width_imp = mant * pow(10,exp);
    char *dist_str_imp = g_strdup_printf("%u %s", width_imp, dist_imp_unit);

    /* convert back to pixels */
    width_imp *= imp_scale;
    width_imp /= m_per_pix;

    cairo_select_font_face (cr, "Sans",
                            CAIRO_FONT_SLANT_NORMAL,
                            CAIRO_FONT_WEIGHT_BOLD);
    cairo_set_font_size (cr, OSD_SCALE_FONT_SIZE);
    cairo_set_source_rgba(cr, 0.0, 0.0, 0.0, 1.0);

    cairo_text_extents_t extents;
    cairo_text_extents (cr, dist_str, &extents);

    cairo_set_source_rgb(cr, 1.0, 1.0, 1.0);
    cairo_set_line_width (cr, OSD_SCALE_FONT_SIZE/6);
    cairo_move_to (cr, 2*OSD_SCALE_FD, OSD_SCALE_H2-OSD_SCALE_FD);
    cairo_text_path (cr, dist_str);
    cairo_stroke (cr);
    cairo_move_to (cr, 2*OSD_SCALE_FD, 
                   OSD_SCALE_H2+OSD_SCALE_FD + extents.height);
    cairo_text_path (cr, dist_str_imp);
    cairo_stroke (cr);

    cairo_set_source_rgb(cr, 0.0, 0.0, 0.0);
    cairo_move_to (cr, 2*OSD_SCALE_FD, OSD_SCALE_H2-OSD_SCALE_FD);
    cairo_show_text (cr, dist_str);
    cairo_move_to (cr, 2*OSD_SCALE_FD, 
                   OSD_SCALE_H2+OSD_SCALE_FD + extents.height);
    cairo_show_text (cr, dist_str_imp);

    g_free(dist_str);
    g_free(dist_str_imp);

    /* draw white line */
    cairo_set_line_cap  (cr, CAIRO_LINE_CAP_ROUND);
    cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, 1.0);
    cairo_set_line_width (cr, OSD_SCALE_FONT_SIZE/3);
    cairo_move_to (cr, OSD_SCALE_FONT_SIZE/6, OSD_SCALE_M);
    cairo_rel_line_to (cr, 0,  OSD_SCALE_TICK);
    cairo_rel_line_to (cr, width_metric, 0);
    cairo_rel_line_to (cr, 0, -OSD_SCALE_TICK);
    cairo_stroke(cr);
    cairo_move_to (cr, OSD_SCALE_FONT_SIZE/6, OSD_SCALE_I);
    cairo_rel_line_to (cr, 0, -OSD_SCALE_TICK);
    cairo_rel_line_to (cr, width_imp, 0);
    cairo_rel_line_to (cr, 0, +OSD_SCALE_TICK);
    cairo_stroke(cr);

    /* draw black line */
    cairo_set_source_rgba(cr, 0.0, 0.0, 0.0, 1.0);
    cairo_set_line_width (cr, OSD_SCALE_FONT_SIZE/6);
    cairo_move_to (cr, OSD_SCALE_FONT_SIZE/6, OSD_SCALE_M);
    cairo_rel_line_to (cr, 0,  OSD_SCALE_TICK);
    cairo_rel_line_to (cr, width_metric, 0);
    cairo_rel_line_to (cr, 0, -OSD_SCALE_TICK);
    cairo_stroke(cr);
    cairo_move_to (cr, OSD_SCALE_FONT_SIZE/6, OSD_SCALE_I);
    cairo_rel_line_to (cr, 0, -OSD_SCALE_TICK);
    cairo_rel_line_to (cr, width_imp, 0);
    cairo_rel_line_to (cr, 0, +OSD_SCALE_TICK);
    cairo_stroke(cr);

    cairo_destroy(cr);
}
#endif

static void
osd_render_controls(osm_gps_map_osd_t *osd) 
{
    osd_priv_t *priv = (osd_priv_t*)osd->priv; 

    if(!priv->controls.surface)
        return;

    if(priv->controls.rendered 
#ifdef OSD_GPS_BUTTON
       && (priv->controls.gps_enabled == (osd->cb != NULL))
#endif
       )
        return;

#ifdef OSD_GPS_BUTTON
    priv->controls.gps_enabled = (osd->cb != NULL);
#endif
    priv->controls.rendered = TRUE;

#ifndef OSD_COLOR
    GdkColor bg = GTK_WIDGET(osd->widget)->style->bg[GTK_STATE_NORMAL];
    GdkColor fg = GTK_WIDGET(osd->widget)->style->fg[GTK_STATE_NORMAL];
    GdkColor da = GTK_WIDGET(osd->widget)->style->fg[GTK_STATE_INSENSITIVE];
#endif

    /* first fill with transparency */
    g_assert(priv->controls.surface);
    cairo_t *cr = cairo_create(priv->controls.surface);
    cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
    cairo_set_source_rgba(cr, 1.0, 0.0, 0.0, 0.0);
    cairo_paint(cr);
    cairo_set_operator(cr, CAIRO_OPERATOR_OVER);

    /* --------- draw zoom and dpad shape shadow ----------- */
#ifdef OSD_SHADOW_ENABLE
    osd_zoom_shape(cr, 1+OSD_SHADOW, 1+OSD_SHADOW);
    osd_shape_shadow(cr);
#ifndef OSD_NO_DPAD
    osd_dpad_shape(cr, 1+OSD_SHADOW, 1+OSD_SHADOW);
    osd_shape_shadow(cr);
#endif
#endif

    /* --------- draw zoom and dpad shape ----------- */

    osd_zoom_shape(cr, 1, 1);
#ifndef OSD_COLOR
    osd_shape(cr, &bg, &fg);
#else
    osd_shape(cr);
#endif
#ifndef OSD_NO_DPAD
    osd_dpad_shape(cr, 1, 1);
#ifndef OSD_COLOR
    osd_shape(cr, &bg, &fg);
#else
    osd_shape(cr);
#endif
#endif

    /* --------- draw zoom and dpad labels --------- */

#ifdef OSD_SHADOW_ENABLE
    osd_labels_shadow(cr, Z_RAD/3, TRUE);
    osd_zoom_labels(cr, 1+OSD_LBL_SHADOW, 1+OSD_LBL_SHADOW);
#ifndef OSD_NO_DPAD
    osd_dpad_labels(cr, 1+OSD_LBL_SHADOW, 1+OSD_LBL_SHADOW);
#endif
    cairo_stroke(cr);
#ifdef OSD_GPS_BUTTON
    osd_labels_shadow(cr, Z_RAD/6, osd->cb != NULL);
    osd_dpad_gps(cr, 1+OSD_LBL_SHADOW, 1+OSD_LBL_SHADOW); 
    cairo_stroke(cr);
#endif
#endif

#ifndef OSD_COLOR
    osd_labels(cr, Z_RAD/3, TRUE, &fg, &da);
#else
    osd_labels(cr, Z_RAD/3, TRUE);
#endif
    osd_zoom_labels(cr, 1, 1);
#ifndef OSD_NO_DPAD
    osd_dpad_labels(cr, 1, 1);
#endif
    cairo_stroke(cr);

#ifndef OSD_COLOR
    osd_labels(cr, Z_RAD/6, osd->cb != NULL, &fg, &da);
#else
    osd_labels(cr, Z_RAD/6, osd->cb != NULL);
#endif
#ifdef OSD_GPS_BUTTON
    osd_dpad_gps(cr, 1, 1);
#endif
    cairo_stroke(cr);
    
    cairo_destroy(cr);
}

static void
osd_render(osm_gps_map_osd_t *osd) 
{
    /* this function is actually called pretty often since the */
    /* OSD contents may have changed (due to a coordinate/zoom change). */
    /* The different OSD parts have to make sure that they don't */
    /* render unneccessarily often and thus waste CPU power */

    osd_render_controls(osd);

#ifdef OSD_SOURCE_SEL
    osd_render_source_sel(osd, FALSE);
#endif

#ifdef OSD_SCALE
    osd_render_scale(osd);
#endif

#ifdef OSD_CROSSHAIR
    osd_render_crosshair(osd);
#endif

#ifdef OSD_NAV
    osd_render_nav(osd);
#endif

#ifdef OSD_COORDINATES
    osd_render_coordinates(osd);
#endif
}

static void
osd_draw(osm_gps_map_osd_t *osd, GdkDrawable *drawable)
{
    osd_priv_t *priv = (osd_priv_t*)osd->priv; 

    /* OSD itself uses some off-screen rendering, so check if the */
    /* offscreen buffer is present and create it if not */
    if(!priv->controls.surface) {
        /* create overlay ... */
        priv->controls.surface = 
            cairo_image_surface_create(CAIRO_FORMAT_ARGB32, OSD_W+2, OSD_H+2);

        priv->controls.rendered = FALSE;
#ifdef OSD_GPS_BUTTON
        priv->controls.gps_enabled = FALSE;
#endif

#ifdef OSD_SOURCE_SEL
        /* the initial OSD state is alway not-expanded */
        priv->source_sel.surface = 
            cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 
                                           OSD_S_W+2, OSD_S_H+2);
        priv->source_sel.rendered = FALSE;
#endif

#ifdef OSD_SCALE
        priv->scale.surface = 
            cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 
                                       OSD_SCALE_W, OSD_SCALE_H);
        priv->scale.zoom = -1;
#endif

#ifdef OSD_CROSSHAIR
        priv->crosshair.surface = 
            cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 
                                       OSD_CROSSHAIR_W, OSD_CROSSHAIR_H);
        priv->crosshair.rendered = FALSE;         
#endif

#ifdef OSD_COORDINATES
        priv->coordinates.surface = 
            cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 
                                       OSD_COORDINATES_W, OSD_COORDINATES_H);

        priv->coordinates.lat = priv->coordinates.lon = OSM_GPS_MAP_INVALID;
#endif

        /* ... and render it */
        osd_render(osd);
    }

    // now draw this onto the original context 
    cairo_t *cr = gdk_cairo_create(drawable);

    gint x, y;

#ifdef OSD_SCALE
    x =  OSD_X;
    y = -OSD_Y;
    if(x < 0) x += osd->widget->allocation.width - OSD_SCALE_W;
    if(y < 0) y += osd->widget->allocation.height - OSD_SCALE_H;

    cairo_set_source_surface(cr, priv->scale.surface, x, y);
    cairo_paint(cr);
#endif

#ifdef OSD_CROSSHAIR
    x = (osd->widget->allocation.width - OSD_CROSSHAIR_W)/2;
    y = (osd->widget->allocation.height - OSD_CROSSHAIR_H)/2;

    cairo_set_source_surface(cr, priv->crosshair.surface, x, y);
    cairo_paint(cr);
#endif

#ifdef OSD_NAV
    if(priv->nav.surface) {
        x =  OSD_X;
        if(x < 0) x += osd->widget->allocation.width - OSD_NAV_W;
        y = (osd->widget->allocation.height - OSD_NAV_H)/2;
        
        cairo_set_source_surface(cr, priv->nav.surface, x, y);
        cairo_paint(cr);
    }
#endif

#ifdef OSD_COORDINATES
    x = -OSD_X;
    y = -OSD_Y;
    if(x < 0) x += osd->widget->allocation.width - OSD_COORDINATES_W;
    if(y < 0) y += osd->widget->allocation.height - OSD_COORDINATES_H;

    cairo_set_source_surface(cr, priv->coordinates.surface, x, y);
    cairo_paint(cr);
#endif

#ifdef OSD_HEARTRATE
    if(priv->hr.surface) {
        x = (osd->widget->allocation.width - OSD_HR_W)/2;
        y = OSD_HR_Y;
        if(y < 0) y += osd->widget->allocation.height - OSD_COORDINATES_H;
        
        cairo_set_source_surface(cr, priv->hr.surface, x, y);
        cairo_paint(cr);
    }
#endif

#ifdef OSD_BALLOON
    if(priv->balloon.surface) {
 
        /* convert given lat lon into screen coordinates */
        gint x, y;
        osm_gps_map_geographic_to_screen (OSM_GPS_MAP(osd->widget),
                                      priv->balloon.lat, priv->balloon.lon, 
                                      &x, &y);

        /* check if balloon needs to be rerendered */
        osd_render_balloon(osd);

        cairo_set_source_surface(cr, priv->balloon.surface, 
                                 x + priv->balloon.offset_x, 
                                 y + priv->balloon.offset_y);
        cairo_paint(cr);
    }
#endif

    x = OSD_X;
    if(x < 0)
        x += osd->widget->allocation.width - OSD_W;

    y = OSD_Y;
    if(y < 0)
        y += osd->widget->allocation.height - OSD_H;

    cairo_set_source_surface(cr, priv->controls.surface, x, y);
    cairo_paint(cr);

#ifdef OSD_SOURCE_SEL
    if(!priv->source_sel.handler_id) {
        /* the OSD source selection is not being animated */
        if(!priv->source_sel.expanded)
            x = osd->widget->allocation.width - OSD_S_W;
        else
            x = osd->widget->allocation.width - OSD_S_EXP_W + OSD_S_X;
    } else
        x = priv->source_sel.shift;

    y = OSD_S_Y;
    if(OSD_S_Y < 0) {
        if(!priv->source_sel.expanded)
            y = osd->widget->allocation.height - OSD_S_H + OSD_S_Y;
        else
            y = osd->widget->allocation.height - OSD_S_EXP_H + OSD_S_Y;
    }

    cairo_set_source_surface(cr, priv->source_sel.surface, x, y);
    cairo_paint(cr);
#endif

    cairo_destroy(cr);
}

static void
osd_free(osm_gps_map_osd_t *osd) 
{
    osd_priv_t *priv = (osd_priv_t *)(osd->priv);

    if (priv->controls.surface)
         cairo_surface_destroy(priv->controls.surface);

#ifdef OSD_SOURCE_SEL
    if(priv->source_sel.handler_id)
        gtk_timeout_remove(priv->source_sel.handler_id);

    if (priv->source_sel.surface)
         cairo_surface_destroy(priv->source_sel.surface);
#endif

#ifdef OSD_SCALE
    if (priv->scale.surface)
         cairo_surface_destroy(priv->scale.surface);
#endif

#ifdef OSD_CROSSHAIR
    if (priv->crosshair.surface)
         cairo_surface_destroy(priv->crosshair.surface);
#endif

#ifdef OSD_NAV
    if (priv->nav.surface)
         cairo_surface_destroy(priv->nav.surface);
#endif

#ifdef OSD_COORDINATES
    if (priv->coordinates.surface)
         cairo_surface_destroy(priv->coordinates.surface);
#endif

#ifdef OSD_BALLOON
    if (priv->balloon.surface) 
         cairo_surface_destroy(priv->balloon.surface);
#endif

#ifdef OSD_HEARTRATE
    if (priv->hr.surface) 
         cairo_surface_destroy(priv->hr.surface);
#endif

    g_free(priv);
    osd->priv = NULL;
}

static gboolean
osd_busy(osm_gps_map_osd_t *osd) 
{
#ifdef OSD_SOURCE_SEL
    osd_priv_t *priv = (osd_priv_t *)(osd->priv);
    return (priv->source_sel.handler_id != 0);
#else
    return FALSE;
#endif
}

static osd_button_t
osd_check(osm_gps_map_osd_t *osd, gboolean down, gint x, gint y) {
    return osd_check_int(osd, TRUE, down, x, y);
}

static osm_gps_map_osd_t osd_classic = {
    .widget     = NULL,

    .draw       = osd_draw,
    .check      = osd_check,
    .render     = osd_render,
    .free       = osd_free,
    .busy       = osd_busy,

    .cb         = NULL,
    .data       = NULL,

    .priv       = NULL
};

/* this is the only function that's externally visible */
void
osm_gps_map_osd_classic_init(OsmGpsMap *map) 
{
    osd_priv_t *priv = g_new0(osd_priv_t, 1);

    /* reset entries to default value */
    osd_classic.widget = NULL;
    osd_classic.cb     = NULL;
    osd_classic.data   = NULL;
    osd_classic.priv   = priv;

#ifdef OSD_BALLOON
    priv->balloon.lat = OSM_GPS_MAP_INVALID;
    priv->balloon.lon = OSM_GPS_MAP_INVALID;
#endif

#ifdef OSD_HEARTRATE
    priv->hr.rate = OSD_HR_NONE;
#endif

    osm_gps_map_register_osd(map, &osd_classic);
}

#ifdef OSD_GPS_BUTTON
/* below are osd specific functions which aren't used by osm-gps-map */
/* but instead are to be used by the main application */
void osm_gps_map_osd_enable_gps (OsmGpsMap *map, OsmGpsMapOsdCallback cb, 
                                 gpointer data) {
    osm_gps_map_osd_t *osd = osm_gps_map_osd_get(map);
    g_return_if_fail (osd);

    osd->cb = cb;
    osd->data = data;

    /* this may have changed the state of the gps button */
    /* we thus re-render the overlay */
    osd->render(osd);

    osm_gps_map_redraw(map);
}
#endif

osd_button_t
osm_gps_map_osd_check(OsmGpsMap *map, gint x, gint y) {
    osm_gps_map_osd_t *osd = osm_gps_map_osd_get(map);
    g_return_val_if_fail (osd, OSD_NONE);
    
    return osd_check_int(osd, FALSE, TRUE, x, y);
}
