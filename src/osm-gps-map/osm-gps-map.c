/* -*- Mode: C; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4 -*- */
/* vim:set et sw=4 ts=4 cino=t0,(0: */
/*
 * osm-gps-map.c
 * Copyright (C) Marcus Bauer 2008 <marcus.bauer@gmail.com>
 * Copyright (C) John Stowers 2009 <john.stowers@gmail.com>
 * Copyright (C) Till Harbaum 2009 <till@harbaum.org>
 *
 * Contributions by
 * Everaldo Canuto 2009 <everaldo.canuto@gmail.com>
 *
 * osm-gps-map.c is free software: you can redistribute it and/or modify it
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

#include "config.h"

#include <fcntl.h>
#include <math.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <glib.h>
#include <glib/gstdio.h>
#include <glib/gprintf.h>
#include <libsoup/soup.h>
#include <cairo.h>

#include "converter.h"
#include "osm-gps-map-types.h"
#include "osm-gps-map.h"

#define ENABLE_DEBUG                (0)
#define USER_AGENT                  "Mozilla/5.0 (Windows; U; Windows NT 5.1; en-US; rv:1.8.1.11) Gecko/20071127 Firefox/2.0.0.11"

struct _OsmGpsMapPrivate
{
    GHashTable *tile_queue;
    GHashTable *missing_tiles;
    GHashTable *tile_cache;

    guint viewport_width;
    guint viewport_height;

    /* Dirty region. */
    cairo_region_t *dirty;

    int map_zoom;
    int max_zoom;
    int min_zoom;
    gboolean map_auto_center;
    gboolean map_auto_download;
    int map_x;
    int map_y;

    /* Latitude and longitude of the center of the map, in radians */
    gfloat center_rlat;
    gfloat center_rlon;

    guint max_tile_cache_size;
    /* Incremented at each redraw */
    guint redraw_cycle;
    /* ID of the idle redraw operation */
    gulong idle_map_redraw;

    //how we download tiles
    SoupSession *soup_session;
    char *proxy_uri;

    //where downloaded tiles are cached
    char *tile_dir;
    char *tile_base_dir;
    char *cache_dir;

    //contains flags indicating the various special characters
    //the uri string contains, that will be replaced when calculating
    //the uri to download.
    OsmGpsMapSource_t map_source;
    char *repo_uri;
    char *image_format;
    int uri_format;
    //flag indicating if the map source is located on the google
    gboolean the_google;

    //gps tracking state
    gboolean record_trip_history;
    gboolean show_trip_history;
    GSList *trip_history;
    coord_t *gps;
    float gps_heading;
    gboolean gps_valid;

    //additional images or tracks added to the map
    GSList *tracks;
    GSList *images;

    //Used for storing the joined tiles
    cairo_surface_t *cr_surf;
    cairo_t *cr;

    //The tile painted when one cannot be found
    cairo_surface_t *null_tile;

    //A list of OsmGpsMapLayer* layers, such as the OSD
    GSList *layers;

    //for customizing the redering of the gps track
    int ui_gps_track_width;
    int ui_gps_point_inner_radius;
    int ui_gps_point_outer_radius;

    guint fullscreen : 1;
    guint is_disposed : 1;
    guint double_pixel : 1;
};

#define OSM_GPS_MAP_PRIVATE(o)  (OSM_GPS_MAP (o)->priv)

typedef struct
{
    cairo_surface_t *cr_surf;
    /* We keep track of the number of the redraw cycle this tile was last used,
     * so that osm_gps_map_purge_cache() can remove the older ones */
    guint redraw_cycle;
} OsmCachedTile;

enum
{
    PROP_0,

    PROP_DOUBLE_PIXEL,
    PROP_AUTO_CENTER,
    PROP_RECORD_TRIP_HISTORY,
    PROP_SHOW_TRIP_HISTORY,
    PROP_AUTO_DOWNLOAD,
    PROP_REPO_URI,
    PROP_PROXY_URI,
    PROP_TILE_CACHE_DIR,
    PROP_TILE_CACHE_BASE_DIR,
    PROP_TILE_CACHE_DIR_IS_FULL_PATH,
    PROP_ZOOM,
    PROP_MAX_ZOOM,
    PROP_MIN_ZOOM,
    PROP_LATITUDE,
    PROP_LONGITUDE,
    PROP_MAP_X,
    PROP_MAP_Y,
    PROP_TILES_QUEUED,
    PROP_GPS_TRACK_WIDTH,
    PROP_GPS_POINT_R1,
    PROP_GPS_POINT_R2,
    PROP_MAP_SOURCE,
    PROP_IMAGE_FORMAT,
    PROP_VIEWPORT_WIDTH,
    PROP_VIEWPORT_HEIGHT,

    PROP_LAST
};

static GParamSpec *properties[PROP_LAST];

G_DEFINE_TYPE (OsmGpsMap, osm_gps_map, G_TYPE_OBJECT);

/*
 * Drawing function forward defintions
 */
static gchar    *replace_string(const gchar *src, const gchar *from, const gchar *to);
static void     inspect_map_uri(OsmGpsMap *map);
static gchar    *replace_map_uri(OsmGpsMap *map, const gchar *uri, int zoom, int x, int y);
static void     osm_gps_map_print_images (OsmGpsMap *map);
static void     osm_gps_map_draw_gps_point (OsmGpsMap *map);
#if USE_LIBSOUP22
static void     osm_gps_map_tile_download_complete (SoupMessage *msg, gpointer user_data);
#else
static void     osm_gps_map_tile_download_complete (SoupSession *session, SoupMessage *msg, gpointer user_data);
#endif
static void     osm_gps_map_download_tile (OsmGpsMap *map, int zoom, int x, int y, gboolean redraw);
static void     osm_gps_map_load_tile (OsmGpsMap *map, int zoom, int x, int y, int offset_x, int offset_y);
static void     osm_gps_map_fill_tiles_pixel (OsmGpsMap *map);
static gboolean osm_gps_map_idle_redraw(OsmGpsMap *map);

static void
cached_tile_free (OsmCachedTile *tile)
{
    g_message("destroying cached tile.");
    cairo_surface_destroy (tile->cr_surf);
    g_slice_free (OsmCachedTile, tile);
}

/*
 * Description:
 *   Find and replace text within a string.
 *
 * Parameters:
 *   src  (in) - pointer to source string
 *   from (in) - pointer to search text
 *   to   (in) - pointer to replacement text
 *
 * Returns:
 *   Returns a pointer to dynamically-allocated memory containing string
 *   with occurences of the text pointed to by 'from' replaced by with the
 *   text pointed to by 'to'.
 */
static gchar *
replace_string(const gchar *src, const gchar *from, const gchar *to)
{
    size_t size    = strlen(src) + 1;
    size_t fromlen = strlen(from);
    size_t tolen   = strlen(to);

    /* Allocate the first chunk with enough for the original string. */
    gchar *value = g_malloc(size);


    /* We need to return 'value', so let's make a copy to mess around with. */
    gchar *dst = value;

    if ( value != NULL )
    {
        for ( ;; )
        {
            /* Try to find the search text. */
            const gchar *match = g_strstr_len(src, size, from);
            if ( match != NULL )
            {
                gchar *temp;
                /* Find out how many characters to copy up to the 'match'. */
                size_t count = match - src;


                /* Calculate the total size the string will be after the
                 * replacement is performed. */
                size += tolen - fromlen;

                temp = g_realloc(value, size);
                if ( temp == NULL )
                {
                    g_free(value);
                    return NULL;
                }

                /* we'll want to return 'value' eventually, so let's point it
                 * to the memory that we are now working with.
                 * And let's not forget to point to the right location in
                 * the destination as well. */
                dst = temp + (dst - value);
                value = temp;

                /*
                 * Copy from the source to the point where we matched. Then
                 * move the source pointer ahead by the amount we copied. And
                 * move the destination pointer ahead by the same amount.
                 */
                g_memmove(dst, src, count);
                src += count;
                dst += count;

                /* Now copy in the replacement text 'to' at the position of
                 * the match. Adjust the source pointer by the text we replaced.
                 * Adjust the destination pointer by the amount of replacement
                 * text. */
                g_memmove(dst, to, tolen);
                src += fromlen;
                dst += tolen;
            }
            else
            {
                /*
                 * Copy any remaining part of the string. This includes the null
                 * termination character.
                 */
                strcpy(dst, src);
                break;
            }
        }
    }
    return value;
}

static void
map_convert_coords_to_quadtree_string(gint x, gint y, gint zoomlevel,
                                      gchar *buffer, const gchar initial,
                                      const gchar *const quadrant)
{
    gchar *ptr = buffer;
    gint n;

    if (initial)
        *ptr++ = initial;

    for(n = zoomlevel-1; n >= 0; n--)
    {
        gint xbit = (x >> n) & 1;
        gint ybit = (y >> n) & 1;
        *ptr++ = quadrant[xbit + 2 * ybit];
    }

    *ptr++ = '\0';
}


static void
inspect_map_uri(OsmGpsMap *map)
{
    OsmGpsMapPrivate *priv = map->priv;
    priv->uri_format = 0;
    priv->the_google = FALSE;

    if (g_strrstr(priv->repo_uri, URI_MARKER_X))
        priv->uri_format |= URI_HAS_X;

    if (g_strrstr(priv->repo_uri, URI_MARKER_Y))
        priv->uri_format |= URI_HAS_Y;

    if (g_strrstr(priv->repo_uri, URI_MARKER_Z))
        priv->uri_format |= URI_HAS_Z;

    if (g_strrstr(priv->repo_uri, URI_MARKER_S))
        priv->uri_format |= URI_HAS_S;

    if (g_strrstr(priv->repo_uri, URI_MARKER_Q))
        priv->uri_format |= URI_HAS_Q;

    if (g_strrstr(priv->repo_uri, URI_MARKER_Q0))
        priv->uri_format |= URI_HAS_Q0;

    if (g_strrstr(priv->repo_uri, URI_MARKER_YS))
        priv->uri_format |= URI_HAS_YS;

    if (g_strrstr(priv->repo_uri, URI_MARKER_R))
        priv->uri_format |= URI_HAS_R;

    if (g_strrstr(priv->repo_uri, "google.com"))
        priv->the_google = TRUE;

    g_debug("URI Format: 0x%X (google: %X)", priv->uri_format, priv->the_google);

}

static gchar *
replace_map_uri(OsmGpsMap *map, const gchar *uri, int zoom, int x, int y)
{
    OsmGpsMapPrivate *priv = map->priv;
    char *url;
    unsigned int i;
    char location[22];

    i = 1;
    url = g_strdup(uri);
    while (i < URI_FLAG_END)
    {
        char *s = NULL;
        char *old;

        old = url;
        switch(i & priv->uri_format)
        {
            case URI_HAS_X:
                s = g_strdup_printf("%d", x);
                url = replace_string(url, URI_MARKER_X, s);
                //g_debug("FOUND " URI_MARKER_X);
                break;
            case URI_HAS_Y:
                s = g_strdup_printf("%d", y);
                url = replace_string(url, URI_MARKER_Y, s);
                //g_debug("FOUND " URI_MARKER_Y);
                break;
            case URI_HAS_Z:
                s = g_strdup_printf("%d", zoom);
                url = replace_string(url, URI_MARKER_Z, s);
                //g_debug("FOUND " URI_MARKER_Z);
                break;
            case URI_HAS_S:
                s = g_strdup_printf("%d", priv->max_zoom-zoom);
                url = replace_string(url, URI_MARKER_S, s);
                //g_debug("FOUND " URI_MARKER_S);
                break;
            case URI_HAS_Q:
                map_convert_coords_to_quadtree_string(x,y,zoom,location,'t',"qrts");
                s = g_strdup_printf("%s", location);
                url = replace_string(url, URI_MARKER_Q, s);
                //g_debug("FOUND " URI_MARKER_Q);
                break;
            case URI_HAS_Q0:
                map_convert_coords_to_quadtree_string(x,y,zoom,location,'\0', "0123");
                s = g_strdup_printf("%s", location);
                url = replace_string(url, URI_MARKER_Q0, s);
                //g_debug("FOUND " URI_MARKER_Q0);
                break;
            case URI_HAS_YS:
                //              s = g_strdup_printf("%d", y);
                //              url = replace_string(url, URI_MARKER_YS, s);
                g_warning("FOUND " URI_MARKER_YS " NOT IMPLEMENTED");
                //            retval = g_strdup_printf(repo->url,
                //                    tilex,
                //                    (1 << (MAX_ZOOM - zoom)) - tiley - 1,
                //                    zoom - (MAX_ZOOM - 17));
                break;
            case URI_HAS_R:
                s = g_strdup_printf("%d", g_random_int_range(0,4));
                url = replace_string(url, URI_MARKER_R, s);
                //g_debug("FOUND " URI_MARKER_R);
                break;
            default:
                s = NULL;
                break;
        }

        if (s) {
            g_free(s);
            g_free(old);
        }

        i = (i << 1);

    }

    return url;
}

static void
my_log_handler (const gchar * log_domain, GLogLevelFlags log_level, const gchar * message, gpointer user_data)
{
    if (!(log_level & G_LOG_LEVEL_DEBUG) || ENABLE_DEBUG)
        g_log_default_handler (log_domain, log_level, message, user_data);
}

static float
osm_gps_map_get_scale_at_point(int zoom, float rlat, float rlon)
{
    /* world at zoom 1 == 512 pixels */
    return cos(rlat) * M_PI * OSM_EQ_RADIUS / (1<<(7+zoom));
}

/* clears the trip list and all resources */
static void
osm_gps_map_free_trip (OsmGpsMap *map)
{
    OsmGpsMapPrivate *priv = map->priv;
    if (priv->trip_history) {
        g_slist_foreach(priv->trip_history, (GFunc) g_free, NULL);
        g_slist_free(priv->trip_history);
        priv->trip_history = NULL;
    }
}

/* clears the tracks and all resources */
static void
osm_gps_map_free_tracks (OsmGpsMap *map)
{
    OsmGpsMapPrivate *priv = map->priv;
    if (priv->tracks)
    {
        GSList* tmp = priv->tracks;
        while (tmp != NULL)
        {
            g_slist_foreach(tmp->data, (GFunc) g_free, NULL);
            g_slist_free(tmp->data);
            tmp = g_slist_next(tmp);
        }
        g_slist_free(priv->tracks);
        priv->tracks = NULL;
    }
}

/* free the poi image lists */
static void
osm_gps_map_free_images (OsmGpsMap *map)
{
    OsmGpsMapPrivate *priv = map->priv;
    if (priv->images) {
        GSList *list;
        for(list = priv->images; list != NULL; list = list->next)
        {
            image_t *im = list->data;
            cairo_surface_destroy(im->image);
            g_free(im);
        }
        g_slist_free(priv->images);
        priv->images = NULL;
    }
}

static void
osm_gps_map_free_layers(OsmGpsMap *map)
{
    OsmGpsMapPrivate *priv = map->priv;
    if (priv->layers) {
        g_slist_foreach(priv->layers, (GFunc) g_object_unref, NULL);
        g_slist_free(priv->layers);
        priv->layers = NULL;
    }
}

static void
osm_gps_map_print_images (OsmGpsMap *map)
{
    GSList *list;
    int x,y,pixel_x,pixel_y;
    int min_x = 0,min_y = 0,max_x = 0,max_y = 0;
    int map_x0, map_y0;
    cairo_rectangle_int_t rect;
    OsmGpsMapPrivate *priv = map->priv;

    map_x0 = priv->map_x - EXTRA_BORDER;
    map_y0 = priv->map_y - EXTRA_BORDER;
    for(list = priv->images; list != NULL; list = list->next)
    {
        image_t *im = list->data;

        // pixel_x,y, offsets
        pixel_x = lon2pixel(priv->map_zoom, im->pt.rlon);
        pixel_y = lat2pixel(priv->map_zoom, im->pt.rlat);

        g_debug("Image %dx%d @: %f,%f (%d,%d)",
                im->w, im->h,
                im->pt.rlat, im->pt.rlon,
                pixel_x, pixel_y);

        x = pixel_x - map_x0;
        y = pixel_y - map_y0;

        cairo_set_source_surface(priv->cr, im->image, x-im->xoffset,y-im->yoffset);
        cairo_paint(priv->cr);

        max_x = MAX(x+im->w,max_x);
        min_x = MIN(x-im->w,min_x);
        max_y = MAX(y+im->h,max_y);
        min_y = MIN(y-im->h,min_y);
    }
    rect.x = min_x + EXTRA_BORDER;
    rect.y = min_y + EXTRA_BORDER;
    rect.width = max_x - min_x;
    rect.height = max_y - min_y;
    g_message("dirty is %p", priv->dirty);
    cairo_region_union_rectangle(priv->dirty, &rect);
}

static void
osm_gps_map_draw_gps_point (OsmGpsMap *map)
{
    OsmGpsMapPrivate *priv = map->priv;

    //incase we get called before we have got a gps point
    if (priv->gps_valid) {
        int map_x0, map_y0;
        int x, y;
        int r = priv->ui_gps_point_inner_radius;
        int r2 = priv->ui_gps_point_outer_radius;
        int mr = MAX(3*r,r2);
        cairo_rectangle_int_t rect;

        map_x0 = priv->map_x - EXTRA_BORDER;
        map_y0 = priv->map_y - EXTRA_BORDER;
        x = lon2pixel(priv->map_zoom, priv->gps->rlon) - map_x0;
        y = lat2pixel(priv->map_zoom, priv->gps->rlat) - map_y0;
        cairo_pattern_t *pat;

        // draw transparent area
        if (r2 > 0) {
            cairo_set_line_width (priv->cr, 1.5);
            cairo_set_source_rgba (priv->cr, 0.75, 0.75, 0.75, 0.4);
            cairo_arc (priv->cr, x, y, r2, 0, 2 * M_PI);
            cairo_fill (priv->cr);
            // draw transparent area border
            cairo_set_source_rgba (priv->cr, 0.55, 0.55, 0.55, 0.4);
            cairo_arc (priv->cr, x, y, r2, 0, 2 * M_PI);
            cairo_stroke(priv->cr);
        }

        // draw ball gradient
        if (r > 0) {
            // draw direction arrow
            if(!isnan(priv->gps_heading)) 
            {
                cairo_move_to (priv->cr, x-r*cos(priv->gps_heading), y-r*sin(priv->gps_heading));
                cairo_line_to (priv->cr, x+3*r*sin(priv->gps_heading), y-3*r*cos(priv->gps_heading));
                cairo_line_to (priv->cr, x+r*cos(priv->gps_heading), y+r*sin(priv->gps_heading));
                cairo_close_path (priv->cr);

                cairo_set_source_rgba (priv->cr, 0.3, 0.3, 1.0, 0.5);
                cairo_fill_preserve (priv->cr);

                cairo_set_line_width (priv->cr, 1.0);
                cairo_set_source_rgba (priv->cr, 0.0, 0.0, 0.0, 0.5);
                cairo_stroke(priv->cr);
            }

            pat = cairo_pattern_create_radial (x-(r/5), y-(r/5), (r/5), x,  y, r);
            cairo_pattern_add_color_stop_rgba (pat, 0, 1, 1, 1, 1.0);
            cairo_pattern_add_color_stop_rgba (pat, 1, 0, 0, 1, 1.0);
            cairo_set_source (priv->cr, pat);
            cairo_arc (priv->cr, x, y, r, 0, 2 * M_PI);
            cairo_fill (priv->cr);
            cairo_pattern_destroy (pat);
            // draw ball border
            cairo_set_line_width (priv->cr, 1.0);
            cairo_set_source_rgba (priv->cr, 0.0, 0.0, 0.0, 1.0);
            cairo_arc (priv->cr, x, y, r, 0, 2 * M_PI);
            cairo_stroke(priv->cr);
        }
        rect.x = x - mr;
        rect.y = y - mr;
        rect.width = mr * 2;
        rect.height = mr * 2;
        cairo_region_union_rectangle(priv->dirty, &rect);
    }
}

static void
osm_gps_map_blit_surface(OsmGpsMap *map, cairo_surface_t *cr_surf,
                         int offset_x, int offset_y,
                         int modulo, int area_x, int area_y)
{
    OsmGpsMapPrivate *priv = map->priv;

    g_debug("Queing redraw @ %d,%d (w:%d h:%d)", offset_x,offset_y, TILESIZE,TILESIZE);
    if (priv->double_pixel) {
        modulo *= 2;
        cairo_rectangle(priv->cr, offset_x, offset_y, TILESIZE * 2, TILESIZE * 2);
    }
    else
        cairo_rectangle(priv->cr, offset_x, offset_y, TILESIZE, TILESIZE);
    cairo_save(priv->cr);
    cairo_translate(priv->cr, offset_x, offset_y);
    cairo_scale(priv->cr, modulo, modulo);
    cairo_set_source_surface(priv->cr, cr_surf, -area_x, -area_y);
    cairo_fill_preserve(priv->cr);
    cairo_restore(priv->cr);
    cairo_set_source_rgb(priv->cr, 0., 0., 0.);
    cairo_stroke(priv->cr);
    g_message("Blit surface %p(%p) at %dx%d x%d %dx%d.",
              (gpointer)cr_surf, (gpointer)priv->null_tile, offset_x, offset_y,
              modulo, area_x, area_y);
}

/* libsoup-2.2 and libsoup-2.4 use different ways to store the body data */
#if USE_LIBSOUP22
#define  soup_message_headers_append(a,b,c) soup_message_add_header(a,b,c)
#define MSG_RESPONSE_BODY(a)    ((a)->response.body)
#define MSG_RESPONSE_LEN(a)     ((a)->response.length)
#define MSG_RESPONSE_LEN_FORMAT "%u"
#else
#define MSG_RESPONSE_BODY(a)    ((a)->response_body->data)
#define MSG_RESPONSE_LEN(a)     ((a)->response_body->length)
#define MSG_RESPONSE_LEN_FORMAT "%ld"
#endif

#if USE_LIBSOUP22
static void
osm_gps_map_tile_download_complete (SoupMessage *msg, gpointer user_data)
#else
static void
osm_gps_map_tile_download_complete (SoupSession *session, SoupMessage *msg, gpointer user_data)
#endif
{
    FILE *file;
    tile_download_t *dl = (tile_download_t *)user_data;
    OsmGpsMap *map = OSM_GPS_MAP(dl->map);
    OsmGpsMapPrivate *priv = map->priv;
    gboolean file_saved = FALSE;
    cairo_surface_t *cr_surf;

    if (SOUP_STATUS_IS_SUCCESSFUL (msg->status_code))
    {
        /* save tile into cachedir if one has been specified */
        if (priv->cache_dir)
        {
            if (g_mkdir_with_parents(dl->folder,0700) == 0)
            {
                file = g_fopen(dl->filename, "wb");
                if (file != NULL)
                {
                    fwrite (MSG_RESPONSE_BODY(msg), 1, MSG_RESPONSE_LEN(msg), file);
                    file_saved = TRUE;
                    g_debug("Wrote "MSG_RESPONSE_LEN_FORMAT" bytes to %s", MSG_RESPONSE_LEN(msg), dl->filename);
                    fclose (file);
                }
            }
            else
            {
                g_warning("Error creating tile download directory: %s", 
                          dl->folder);
                perror("perror:");
            }
        }

        if (dl->redraw)
        {
            if (priv->cache_dir && file_saved)
            {
                cr_surf = cairo_image_surface_create_from_png (dl->filename);
                if (cairo_surface_status(cr_surf) == CAIRO_STATUS_SUCCESS)
                    {
                        OsmCachedTile *tile = g_slice_new (OsmCachedTile);
                        tile->cr_surf = cr_surf;
                        tile->redraw_cycle = priv->redraw_cycle;
                        /* if the tile is already in the cache (it could be one
                         * rendered from another zoom level), it will be
                         * overwritten */
                        g_hash_table_insert (priv->tile_cache, dl->filename, tile);
                        /* NULL-ify dl->filename so that it won't be freed, as
                         * we are using it as a key in the hash table */
                        dl->filename = NULL;
                    }
            }
            else
                g_warning("Load from memory not implemented.");
            if (!priv->idle_map_redraw)
                priv->idle_map_redraw = g_idle_add((GSourceFunc)osm_gps_map_idle_redraw, map);
        }
        g_hash_table_remove(priv->tile_queue, dl->uri);

        g_free(dl->uri);
        g_free(dl->folder);
        g_free(dl->filename);
        g_free(dl);
    }
    else
    {
        g_message("Error downloading tile: %d - %s", msg->status_code, msg->reason_phrase);
        if (msg->status_code == SOUP_STATUS_NOT_FOUND)
        {
            g_hash_table_insert(priv->missing_tiles, dl->uri, NULL);
            g_hash_table_remove(priv->tile_queue, dl->uri);
        }
        else if (msg->status_code == SOUP_STATUS_CANCELLED)
        {
            ;//application exiting
        }
        else if (msg->status_code == SOUP_STATUS_CANT_RESOLVE)
        {
            ; /* No network... */
        }
        else
        {
#if USE_LIBSOUP22
            soup_session_requeue_message(dl->session, msg);
#else
            soup_session_requeue_message(session, msg);
#endif
            return;
        }
    }


}

static void
osm_gps_map_download_tile (OsmGpsMap *map, int zoom, int x, int y, gboolean redraw)
{
    SoupMessage *msg;
    OsmGpsMapPrivate *priv = map->priv;
    tile_download_t *dl = g_new0(tile_download_t,1);

    //calculate the uri to download
    dl->uri = replace_map_uri(map, priv->repo_uri, zoom, x, y);

#if USE_LIBSOUP22
    dl->session = priv->soup_session;
#endif

    //check the tile has not already been queued for download,
    //or has been attempted, and its missing
    if (g_hash_table_lookup_extended(priv->tile_queue, dl->uri, NULL, NULL) ||
        g_hash_table_lookup_extended(priv->missing_tiles, dl->uri, NULL, NULL) )
    {
        g_debug("Tile already downloading (or missing)");
        g_free(dl->uri);
        g_free(dl);
    } else {
        dl->folder = g_strdup_printf("%s%c%d%c%d%c",
                            priv->cache_dir, G_DIR_SEPARATOR,
                            zoom, G_DIR_SEPARATOR,
                            x, G_DIR_SEPARATOR);
        dl->filename = g_strdup_printf("%s%c%d%c%d%c%d.%s",
                            priv->cache_dir, G_DIR_SEPARATOR,
                            zoom, G_DIR_SEPARATOR,
                            x, G_DIR_SEPARATOR,
                            y,
                            priv->image_format);
        dl->map = map;
        dl->redraw = redraw;

        g_debug("Download tile: %d,%d z:%d\n\t%s --> %s", x, y, zoom, dl->uri, dl->filename);

        msg = soup_message_new (SOUP_METHOD_GET, dl->uri);
        if (msg) {
            if (priv->the_google) {
                //Set maps.google.com as the referrer
                g_debug("Setting Google Referrer");
                soup_message_headers_append(msg->request_headers, "Referer", "http://maps.google.com/");
                //For google satelite also set the appropriate cookie value
                if (priv->uri_format & URI_HAS_Q) {
                    const char *cookie = g_getenv("GOOGLE_COOKIE");
                    if (cookie) {
                        g_debug("Adding Google Cookie");
                        soup_message_headers_append(msg->request_headers, "Cookie", cookie);
                    }
                }
            }

#if USE_LIBSOUP22
            soup_message_headers_append(msg->request_headers, 
                                        "User-Agent", USER_AGENT);
#endif

            g_hash_table_insert (priv->tile_queue, dl->uri, msg);
            soup_session_queue_message (priv->soup_session, msg, osm_gps_map_tile_download_complete, dl);
        } else {
            g_warning("Could not create soup message");
            g_free(dl->uri);
            g_free(dl->folder);
            g_free(dl->filename);
            g_free(dl);
        }
    }
}

static OsmCachedTile *
osm_gps_map_load_cached_tile (OsmGpsMap *map, int zoom, int x, int y)
{
    OsmGpsMapPrivate *priv = map->priv;
    gchar *filename;
    OsmCachedTile *tile;
    cairo_surface_t *cr_surf;

    filename = g_strdup_printf("%s%c%d%c%d%c%d.%s",
                priv->cache_dir, G_DIR_SEPARATOR,
                zoom, G_DIR_SEPARATOR,
                x, G_DIR_SEPARATOR,
                y,
                priv->image_format);

    tile = g_hash_table_lookup (priv->tile_cache, filename);
    if (tile)
    {
        g_free (filename);
    }
    else
    {
        cr_surf = cairo_image_surface_create_from_png (filename);
        if (cairo_surface_status(cr_surf) == CAIRO_STATUS_SUCCESS)
        {
            tile = g_slice_new (OsmCachedTile);
            tile->cr_surf = cr_surf;
            g_hash_table_insert (priv->tile_cache, filename, tile);
            g_message("caching %s %p.", filename, (gpointer)cr_surf);
        }
        else
        {
            g_message("issue with %s.", filename);
            g_free (filename);
        }
    }

    /* set/update the redraw_cycle timestamp on the tile */
    if (tile)
    {
        tile->redraw_cycle = priv->redraw_cycle;
    }

    return tile;
}

static OsmCachedTile *
osm_gps_map_find_bigger_tile (OsmGpsMap *map, int zoom, int x, int y,
                              int *zoom_found)
{
    OsmCachedTile *tile;
    int next_zoom, next_x, next_y;

    if (zoom == 0) return NULL;
    next_zoom = zoom - 1;
    next_x = x / 2;
    next_y = y / 2;
    tile = osm_gps_map_load_cached_tile (map, next_zoom, next_x, next_y);
    if (tile)
        *zoom_found = next_zoom;
    else
        tile = osm_gps_map_find_bigger_tile (map, next_zoom, next_x, next_y,
                                               zoom_found);
    return tile;
}

static OsmCachedTile *
osm_gps_map_render_missing_tile_upscaled (OsmGpsMap *map, int zoom,
                                          int x, int y,
                                          int *modulo, int *area_x, int *area_y)
{
    OsmCachedTile *big;
    int zoom_big, zoom_diff, area_size;

    big = osm_gps_map_find_bigger_tile (map, zoom, x, y, &zoom_big);
    if (!big) return NULL;

    g_debug ("Found bigger tile (zoom = %d, wanted = %d)", zoom_big, zoom);

    /* get a Pixbuf for the area to magnify */
    zoom_diff = zoom - zoom_big;
    area_size = TILESIZE >> zoom_diff;
    *modulo = 1 << zoom_diff;
    *area_x = (x % (*modulo)) * area_size;
    *area_y = (y % (*modulo)) * area_size;

    return big;
}

static OsmCachedTile *
osm_gps_map_render_missing_tile (OsmGpsMap *map, int zoom, int x, int y,
                                 int *modulo, int *area_x, int *area_y)
{
    /* maybe TODO: render from downscaled tiles, if the following fails */
    g_message("look for upscaled at %dx%d.", x, y);
    return osm_gps_map_render_missing_tile_upscaled (map, zoom, x, y,
                                                     modulo, area_x, area_y);
}

/* default tile lifetime is one week */
#ifndef OSM_GPS_MAP_TILE_TTL 
#define  OSM_GPS_MAP_TILE_TTL  (60*60*24*7)   
#endif

static gboolean
osm_gps_map_tile_age_exceeded(char *filename) 
{
    struct stat buf;
    
    if(!g_stat(filename, &buf)) 
        return(time(NULL) - buf.st_mtime > OSM_GPS_MAP_TILE_TTL);

    return FALSE;
}

static void
osm_gps_map_load_tile (OsmGpsMap *map, int zoom, int x, int y, int offset_x, int offset_y)
{
    OsmGpsMapPrivate *priv = map->priv;
    gchar *filename;
    OsmCachedTile *tile = NULL;
    int modulo, area_x, area_y;

    g_debug("Load tile %d,%d (%d,%d) z:%d", x, y, offset_x, offset_y, zoom);

    if (priv->map_source == OSM_GPS_MAP_SOURCE_NULL) {
        osm_gps_map_blit_surface(map, priv->null_tile, offset_x,offset_y, 1, 0, 0);
        return;
    }

    filename = g_strdup_printf("%s%c%d%c%d%c%d.%s",
                priv->cache_dir, G_DIR_SEPARATOR,
                zoom, G_DIR_SEPARATOR,
                x, G_DIR_SEPARATOR,
                y,
                priv->image_format);

    gboolean needs_refresh = FALSE;
    if (g_file_test(filename, G_FILE_TEST_EXISTS)) {
        /* try to get file from internal cache first */
        tile = osm_gps_map_load_cached_tile(map, zoom, x, y);
        if (!tile) g_warning("cannot cache!!!");

        needs_refresh = osm_gps_map_tile_age_exceeded(filename);
    }

    if(tile)
    {
        g_debug("Found tile %s", filename);
        osm_gps_map_blit_surface(map, tile->cr_surf, offset_x,offset_y,
                                 1, 0, 0);
    }

    if(!tile || needs_refresh)
    {
        if (priv->map_auto_download)
            osm_gps_map_download_tile(map, zoom, x, y, TRUE);

        if(!needs_refresh) {
            /* try to render the tile by scaling cached tiles from other zoom
             * levels */
            tile = osm_gps_map_render_missing_tile (map, zoom, x, y,
                                                    &modulo, &area_x, &area_y);
            if (tile)
            {
                g_message("Tile not found, upscaling.");
                osm_gps_map_blit_surface (map, tile->cr_surf, offset_x,offset_y,
                                          modulo, area_x, area_y);
            }
            else
            {
                g_message("blank.");
                //prevent some artifacts when drawing not yet loaded areas.
                cairo_rectangle(priv->cr, offset_x, offset_y, TILESIZE, TILESIZE);
                cairo_set_source_rgb(priv->cr, 1., 1., 1.);
                cairo_fill(priv->cr);
            }
        }
    }
    g_free(filename);
}

static void
osm_gps_map_fill_tiles_pixel (OsmGpsMap *map)
{
    OsmGpsMapPrivate *priv = map->priv;
    int i,j, tile_x0, tile_y0, tiles_nx, tiles_ny;
    int offset_xn = 0;
    int offset_yn = 0;
    int offset_x;
    int offset_y;

    g_debug("Fill tiles: %d,%d z:%d", priv->map_x, priv->map_y, priv->map_zoom);

    if (priv->double_pixel)
    {
        offset_x = - priv->map_x % (TILESIZE*2);
        offset_y = - priv->map_y % (TILESIZE*2);
        if (offset_x > 0) offset_x -= (TILESIZE*2);
        if (offset_y > 0) offset_y -= (TILESIZE*2);

        offset_xn = offset_x + EXTRA_BORDER;
        offset_yn = offset_y + EXTRA_BORDER;

        tiles_nx = (priv->viewport_width  - offset_x) / (TILESIZE*2) + 1;
        tiles_ny = (priv->viewport_height - offset_y) / (TILESIZE*2) + 1;

        tile_x0 =  floor((float)priv->map_x / (float)(TILESIZE*2));
        tile_y0 =  floor((float)priv->map_y / (float)(TILESIZE*2));

        for (i=tile_x0; i<(tile_x0+tiles_nx);i++)
        {
            for (j=tile_y0;  j<(tile_y0+tiles_ny); j++)
            {
                if( j<0 || i<0 || i>=exp(priv->map_zoom * M_LN2) || j>=exp(priv->map_zoom * M_LN2))
                {
                    cairo_rectangle(priv->cr, offset_xn, offset_yn,
                                    TILESIZE*2, TILESIZE*2);
                    cairo_set_source_rgb(priv->cr, 1., 1., 1.);
                    cairo_fill(priv->cr);
                }
                else
                {
                    osm_gps_map_load_tile(map,
                                          priv->map_zoom - 1,
                                          i,j,
                                          offset_xn,offset_yn);
                }
                offset_yn += TILESIZE*2;
            }
            offset_xn += TILESIZE*2;
            offset_yn = offset_y + EXTRA_BORDER;
        }
    }
    else
    {
        offset_x = - priv->map_x % TILESIZE;
        offset_y = - priv->map_y % TILESIZE;
        if (offset_x > 0) offset_x -= TILESIZE;
        if (offset_y > 0) offset_y -= TILESIZE;

        offset_xn = offset_x + EXTRA_BORDER;
        offset_yn = offset_y + EXTRA_BORDER;

        tiles_nx = (priv->viewport_width  - offset_x) / TILESIZE + 1;
        tiles_ny = (priv->viewport_height - offset_y) / TILESIZE + 1;

        tile_x0 =  floor((float)priv->map_x / (float)TILESIZE);
        tile_y0 =  floor((float)priv->map_y / (float)TILESIZE);
        //TODO: implement wrap around
        for (i=tile_x0; i<(tile_x0+tiles_nx);i++)
        {
            for (j=tile_y0;  j<(tile_y0+tiles_ny); j++)
            {
                if( j<0 || i<0 || i>=exp(priv->map_zoom * M_LN2) || j>=exp(priv->map_zoom * M_LN2))
                {
                    cairo_rectangle(priv->cr, offset_xn, offset_yn,
                                    TILESIZE, TILESIZE);
                    cairo_set_source_rgb(priv->cr, 1., 1., 1.);
                    cairo_fill(priv->cr);
                }
                else
                {
                    osm_gps_map_load_tile(map,
                                          priv->map_zoom,
                                          i,j,
                                          offset_xn,offset_yn);
                }
                offset_yn += TILESIZE;
            }
            offset_xn += TILESIZE;
            offset_yn = offset_y + EXTRA_BORDER;
        }
    }
}

static void
osm_gps_map_print_track (OsmGpsMap *map, GSList *trackpoint_list)
{
    OsmGpsMapPrivate *priv = map->priv;

    GSList *list;
    int x,y;
    int min_x = 0,min_y = 0,max_x = 0,max_y = 0;
    int lw = priv->ui_gps_track_width;
    int map_x0, map_y0;
    cairo_rectangle_int_t rect;

    cairo_set_line_width (priv->cr, lw);
    cairo_set_source_rgba (priv->cr, 60000.0/65535.0, 0.0, 0.0, 0.6);
    cairo_set_line_cap (priv->cr, CAIRO_LINE_CAP_ROUND);
    cairo_set_line_join (priv->cr, CAIRO_LINE_JOIN_ROUND);

    map_x0 = priv->map_x - EXTRA_BORDER;
    map_y0 = priv->map_y - EXTRA_BORDER;
    for(list = trackpoint_list; list != NULL; list = list->next)
    {
        coord_t *tp = list->data;

        x = lon2pixel(priv->map_zoom, tp->rlon) - map_x0;
        y = lat2pixel(priv->map_zoom, tp->rlat) - map_y0;

        // first time through loop
        if (list == trackpoint_list) {
            cairo_move_to(priv->cr, x, y);
        }

        cairo_line_to(priv->cr, x, y);

        max_x = MAX(x,max_x);
        min_x = MIN(x,min_x);
        max_y = MAX(y,max_y);
        min_y = MIN(y,min_y);
    }
    cairo_stroke(priv->cr);
    rect.x = min_x - lw;
    rect.y = min_y - lw;
    rect.width = max_x - min_x + 2 * lw;
    rect.height = max_y - min_y + 2 * lw;
    cairo_region_union_rectangle(priv->dirty, &rect);
}

/* Prints the gps trip history, and any other tracks */
static void
osm_gps_map_print_tracks (OsmGpsMap *map)
{
    OsmGpsMapPrivate *priv = map->priv;

    if (priv->show_trip_history)
        osm_gps_map_print_track (map, priv->trip_history);

    if (priv->tracks)
    {
        GSList* tmp = priv->tracks;
        while (tmp != NULL)
        {
            osm_gps_map_print_track (map, tmp->data);
            tmp = g_slist_next(tmp);
        }
    }
}

static gboolean
osm_gps_map_purge_cache_check(gpointer key, gpointer value, gpointer user)
{
   return (((OsmCachedTile*)value)->redraw_cycle != ((OsmGpsMapPrivate*)user)->redraw_cycle);
}

static void
osm_gps_map_purge_cache (OsmGpsMap *map)
{
   OsmGpsMapPrivate *priv = map->priv;

   if (g_hash_table_size (priv->tile_cache) < priv->max_tile_cache_size)
       return;

   /* run through the cache, and remove the tiles which have not been used
    * during the last redraw operation */
   g_hash_table_foreach_remove(priv->tile_cache, osm_gps_map_purge_cache_check, priv);
}

static gboolean
osm_gps_map_redraw (OsmGpsMap *map)
{
    OsmGpsMapPrivate *priv = map->priv;

    /* on diablo the map comes up at 1x1 pixel size and */
    /* isn't really usable. we'll just ignore this ... */
    if((priv->viewport_width < 2) ||
       (priv->viewport_height < 2)) {
        printf("not a useful sized map yet ...\n");
        return FALSE;
    }

    /* don't redraw the entire map while the OSD is doing */
    /* some animation or the like. This is to keep the animation */
    /* fluid */
    if (priv->layers) {
        GSList *list;
        for(list = priv->layers; list != NULL; list = list->next) {
            OsmGpsMapLayer *layer = list->data;
            if (osm_gps_map_layer_busy(layer))
                return FALSE;
        }
    }
/* #ifdef ENABLE_OSD */
/*     if (priv->osd && priv->osd->busy(priv->osd)) */
/*         return FALSE; */
/* #endif */

    priv->redraw_cycle++;

    /* draw white background to initialise pixmap */
    cairo_set_source_rgb(priv->cr, 1., 1., 1.);
    cairo_paint(priv->cr);

    osm_gps_map_fill_tiles_pixel(map);

    g_message("dirty is %p.", (gpointer)priv->dirty);
    osm_gps_map_print_tracks(map);
    osm_gps_map_draw_gps_point(map);
    osm_gps_map_print_images(map);

    if (priv->layers) {
        GSList *list;
        for(list = priv->layers; list != NULL; list = list->next) {
            OsmGpsMapLayer *layer = list->data;
            osm_gps_map_layer_render (layer, map);
        }
    }

    osm_gps_map_purge_cache(map);

    g_signal_emit_by_name(G_OBJECT(map), "dirty");
    cairo_region_destroy(priv->dirty);
    priv->dirty = cairo_region_create();
    
    return TRUE;
}

static gboolean
osm_gps_map_idle_redraw(OsmGpsMap *map)
{
    OsmGpsMapPrivate *priv = map->priv;

    priv->idle_map_redraw = 0;
    osm_gps_map_redraw(map);
    return FALSE;
}

static void
center_coord_update(OsmGpsMap *map) {

    OsmGpsMapPrivate *priv = OSM_GPS_MAP_PRIVATE(map);

    // pixel_x,y, offsets
    gint pixel_x = priv->map_x + priv->viewport_width/2;
    gint pixel_y = priv->map_y + priv->viewport_height/2;

    priv->center_rlon = pixel2lon(priv->map_zoom, pixel_x);
    priv->center_rlat = pixel2lat(priv->map_zoom, pixel_y);

    g_signal_emit_by_name(map, "changed");
}

static void
osm_gps_map_init (OsmGpsMap *object)
{
    OsmGpsMapPrivate *priv;

    priv = G_TYPE_INSTANCE_GET_PRIVATE (object, OSM_TYPE_GPS_MAP, OsmGpsMapPrivate);
    object->priv = priv;

    priv->cr_surf = NULL;
    priv->cr = NULL;

    priv->trip_history = NULL;
    priv->gps = g_new0(coord_t, 1);
    priv->gps_valid = FALSE;
    priv->gps_heading = OSM_GPS_MAP_INVALID;

    priv->tracks = NULL;
    priv->images = NULL;
    priv->layers = NULL;

    priv->viewport_width = 0;
    priv->viewport_height = 0;
    priv->dirty = cairo_region_create();

    priv->uri_format = 0;
    priv->the_google = FALSE;

    priv->map_source = -1;

    priv->idle_map_redraw = 0;

#if USE_LIBSOUP22
    /* libsoup-2.2 has no special way to set the user agent, so we */
    /* set it seperately as an extra header field for each reuest */
    priv->soup_session = soup_session_async_new();
#else
    /* set the user agent */
    priv->soup_session =
        soup_session_async_new_with_options(SOUP_SESSION_USER_AGENT,
                                            USER_AGENT, NULL);

#endif
    //Hash table which maps tile d/l URIs to SoupMessage requests
    priv->tile_queue = g_hash_table_new (g_str_hash, g_str_equal);

    //Some mapping providers (Google) have varying degrees of tiles at multiple
    //zoom levels
    priv->missing_tiles = g_hash_table_new (g_str_hash, g_str_equal);

    /* memory cache for most recently used tiles */
    priv->tile_cache = g_hash_table_new_full (g_str_hash, g_str_equal,
                                              g_free, (GDestroyNotify)cached_tile_free);
    priv->max_tile_cache_size = 20;

    g_log_set_handler (G_LOG_DOMAIN, G_LOG_LEVEL_MASK, my_log_handler, NULL);
}

static char*
osm_gps_map_get_cache_dir(OsmGpsMapPrivate *priv)
{
    if (priv->tile_base_dir)
        return g_strdup(priv->tile_base_dir);
    return osm_gps_map_get_default_cache_directory();
}

/* strcmp0 was introduced with glib 2.16 */
#if ! GLIB_CHECK_VERSION (2, 16, 0)
int g_strcmp0(const char *str1, const char *str2)
{
    if( str1 == NULL && str2 == NULL ) return 0;
    if( str1 == NULL ) return -1;
    if( str2 == NULL ) return 1;
    return strcmp(str1, str2);
}
#endif

static void
osm_gps_map_setup(OsmGpsMapPrivate *priv)
{
    const char *uri;
    cairo_t *cr;

   //user can specify a map source ID, or a repo URI as the map source
    uri = osm_gps_map_source_get_repo_uri(OSM_GPS_MAP_SOURCE_NULL);
    if ( (priv->map_source == 0) || (strcmp(priv->repo_uri, uri) == 0) ) {
        g_debug("Using null source");
        priv->map_source = OSM_GPS_MAP_SOURCE_NULL;

        priv->null_tile = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, TILESIZE, TILESIZE);
        cr = cairo_create(priv->null_tile);
        cairo_set_source_rgb(cr, 1., 1., 1.);
        cairo_paint(cr);
        cairo_destroy(cr);
    }
    else if (priv->map_source >= 0) {
        //check if the source given is valid
        uri = osm_gps_map_source_get_repo_uri(priv->map_source);
        if (uri) {
            g_debug("Setting map source from ID");
            g_free(priv->repo_uri);

            priv->repo_uri = g_strdup(uri);
            priv->image_format = g_strdup(
                osm_gps_map_source_get_image_format(priv->map_source));
            priv->max_zoom = osm_gps_map_source_get_max_zoom(priv->map_source);
            priv->min_zoom = osm_gps_map_source_get_min_zoom(priv->map_source);
        }
    }

    if (priv->tile_dir == NULL)
        priv->tile_dir = g_strdup(OSM_GPS_MAP_CACHE_DISABLED);

    if ( g_strcmp0(priv->tile_dir, OSM_GPS_MAP_CACHE_DISABLED) == 0 ) {
        priv->cache_dir = NULL;
    } else if ( g_strcmp0(priv->tile_dir, OSM_GPS_MAP_CACHE_AUTO) == 0 ) {
        char *base = osm_gps_map_get_cache_dir(priv);
#if GLIB_CHECK_VERSION (2, 16, 0)
        char *md5 = g_compute_checksum_for_string (G_CHECKSUM_MD5, priv->repo_uri, -1);
#else
        char *md5 = g_strdup(osm_gps_map_source_get_friendly_name(priv->map_source));
#endif
        priv->cache_dir = g_strdup_printf("%s%c%s", base, G_DIR_SEPARATOR, md5);
        g_free(base);
        g_free(md5);
    } else if ( g_strcmp0(priv->tile_dir, OSM_GPS_MAP_CACHE_FRIENDLY) == 0 ) {
        char *base = osm_gps_map_get_cache_dir(priv);
        const char *fname = osm_gps_map_source_get_friendly_name(priv->map_source);
        priv->cache_dir = g_strdup_printf("%s%c%s", base, G_DIR_SEPARATOR, fname);
        g_free(base);
    } else {
        priv->cache_dir = g_strdup(priv->tile_dir);
    }
    g_debug("Cache dir: %s", priv->cache_dir);
}

static GObject *
osm_gps_map_constructor (GType gtype, guint n_properties, GObjectConstructParam *properties)
{
    //Always chain up to the parent constructor
    GObject *object = 
        G_OBJECT_CLASS(osm_gps_map_parent_class)->constructor(gtype, n_properties, properties);

    osm_gps_map_setup(OSM_GPS_MAP_PRIVATE(object));

    inspect_map_uri(OSM_GPS_MAP(object));

    return object;
}

static void
osm_gps_map_dispose (GObject *object)
{
    OsmGpsMap *map = OSM_GPS_MAP(object);
    OsmGpsMapPrivate *priv = map->priv;

    if (priv->is_disposed)
        return;

    g_message("disposing.");
    priv->is_disposed = TRUE;

    soup_session_abort(priv->soup_session);
    g_object_unref(priv->soup_session);

    g_hash_table_destroy(priv->tile_queue);
    g_hash_table_destroy(priv->missing_tiles);
    g_hash_table_destroy(priv->tile_cache);

    /* images and layers contain GObjects which need unreffing, so free here */
    osm_gps_map_free_images(map);
    osm_gps_map_free_layers(map);

    cairo_region_destroy(priv->dirty);

    if (priv->cr)
        cairo_destroy (priv->cr);
    if (priv->cr_surf)
        cairo_surface_destroy (priv->cr_surf);
    
    if (priv->null_tile)
        g_object_unref (priv->null_tile);

    if (priv->idle_map_redraw != 0)
        g_source_remove (priv->idle_map_redraw);

    g_free(priv->gps);

    G_OBJECT_CLASS (osm_gps_map_parent_class)->dispose (object);
}

static void
osm_gps_map_finalize (GObject *object)
{
    OsmGpsMap *map = OSM_GPS_MAP(object);
    OsmGpsMapPrivate *priv = map->priv;

    if (priv->tile_dir)
        g_free(priv->tile_dir);

    if (priv->cache_dir)
        g_free(priv->cache_dir);

    g_free(priv->repo_uri);
    g_free(priv->image_format);

    /* trip and tracks contain simple non GObject types, so free them here */
    osm_gps_map_free_trip(map);
    osm_gps_map_free_tracks(map);

    G_OBJECT_CLASS (osm_gps_map_parent_class)->finalize (object);
}

static void
osm_gps_map_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
    g_return_if_fail (OSM_IS_GPS_MAP (object));
    OsmGpsMap *map = OSM_GPS_MAP(object);
    OsmGpsMapPrivate *priv = map->priv;

    switch (prop_id)
    {
        case PROP_AUTO_CENTER:
            priv->map_auto_center = g_value_get_boolean (value);
            break;
        case PROP_DOUBLE_PIXEL:
            priv->double_pixel = g_value_get_boolean (value);
            if (!priv->idle_map_redraw)
                priv->idle_map_redraw = g_idle_add((GSourceFunc)osm_gps_map_idle_redraw, (gpointer)map);
            break;
        case PROP_RECORD_TRIP_HISTORY:
            priv->record_trip_history = g_value_get_boolean (value);
            break;
        case PROP_SHOW_TRIP_HISTORY:
            priv->show_trip_history = g_value_get_boolean (value);
            break;
        case PROP_AUTO_DOWNLOAD:
            priv->map_auto_download = g_value_get_boolean (value);
            break;
        case PROP_REPO_URI:
            priv->repo_uri = g_value_dup_string (value);
            break;
        case PROP_PROXY_URI:
            if ( g_value_get_string(value) ) {
                priv->proxy_uri = g_value_dup_string (value);
                g_debug("Setting proxy server: %s", priv->proxy_uri);

#if USE_LIBSOUP22
                SoupUri* uri = soup_uri_new(priv->proxy_uri);
                g_object_set(G_OBJECT(priv->soup_session), SOUP_SESSION_PROXY_URI, uri, NULL);
#else
                GValue val = {0};
                SoupURI* uri = soup_uri_new(priv->proxy_uri);
                g_value_init(&val, SOUP_TYPE_URI);
                g_value_take_boxed(&val, uri);
                g_object_set_property(G_OBJECT(priv->soup_session),SOUP_SESSION_PROXY_URI,&val);
#endif
            } else
                priv->proxy_uri = NULL;

            break;
        case PROP_TILE_CACHE_DIR:
            priv->tile_dir = g_value_dup_string (value);
            break;
        case PROP_TILE_CACHE_BASE_DIR:
            priv->tile_base_dir = g_value_dup_string (value);
            break;
        case PROP_TILE_CACHE_DIR_IS_FULL_PATH:
             g_warning("GObject property tile-cache-is-full-path depreciated");
             break;
        case PROP_ZOOM:
            priv->map_zoom = g_value_get_int (value);
            break;
        case PROP_MAX_ZOOM:
            priv->max_zoom = g_value_get_int (value);
            break;
        case PROP_MIN_ZOOM:
            priv->min_zoom = g_value_get_int (value);
            break;
        case PROP_MAP_X:
            priv->map_x = g_value_get_int (value);
            g_message("set map_x at %d.", priv->map_x);
            center_coord_update(map);
            break;
        case PROP_MAP_Y:
            priv->map_y = g_value_get_int (value);
            g_message("set map_y at %d.", priv->map_y);
            center_coord_update(map);
            break;
        case PROP_GPS_TRACK_WIDTH:
            priv->ui_gps_track_width = g_value_get_int (value);
            break;
        case PROP_GPS_POINT_R1:
            priv->ui_gps_point_inner_radius = g_value_get_int (value);
            break;
        case PROP_GPS_POINT_R2:
            priv->ui_gps_point_outer_radius = g_value_get_int (value);
            break;
        case PROP_MAP_SOURCE: {
            gint old = priv->map_source;
            priv->map_source = g_value_get_int (value);
            if(old >= OSM_GPS_MAP_SOURCE_NULL && 
               priv->map_source != old &&
               priv->map_source >= OSM_GPS_MAP_SOURCE_NULL &&
               priv->map_source <= OSM_GPS_MAP_SOURCE_LAST) {

                /* we now have to switch the entire map */

                /* flush the ram cache */
                g_hash_table_remove_all(priv->tile_cache);

                osm_gps_map_setup(priv);

                inspect_map_uri(map);

                if (!priv->idle_map_redraw)
                    priv->idle_map_redraw = g_idle_add((GSourceFunc)osm_gps_map_idle_redraw, map);

                /* adjust zoom if necessary */
                if(priv->map_zoom > priv->max_zoom) 
                    osm_gps_map_set_zoom(map, priv->max_zoom);

                if(priv->map_zoom < priv->min_zoom)
                    osm_gps_map_set_zoom(map, priv->min_zoom);

            } } break;
        case PROP_IMAGE_FORMAT:
            priv->image_format = g_value_dup_string (value);
            break;
        case PROP_VIEWPORT_WIDTH:
            osm_gps_map_set_viewport(map, g_value_get_uint (value), priv->viewport_height);
            break;
        case PROP_VIEWPORT_HEIGHT:
            osm_gps_map_set_viewport(map, priv->viewport_width, g_value_get_uint (value));
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
            break;
    }
}

static void
osm_gps_map_get_property (GObject *object, guint prop_id, GValue *value, GParamSpec *pspec)
{
    g_return_if_fail (OSM_IS_GPS_MAP (object));
    OsmGpsMap *map = OSM_GPS_MAP(object);
    OsmGpsMapPrivate *priv = map->priv;

    switch (prop_id)
    {
        case PROP_DOUBLE_PIXEL:
            g_value_set_boolean(value, priv->double_pixel);
            break;
        case PROP_AUTO_CENTER:
            g_value_set_boolean(value, priv->map_auto_center);
            break;
        case PROP_RECORD_TRIP_HISTORY:
            g_value_set_boolean(value, priv->record_trip_history);
            break;
        case PROP_SHOW_TRIP_HISTORY:
            g_value_set_boolean(value, priv->show_trip_history);
            break;
        case PROP_AUTO_DOWNLOAD:
            g_value_set_boolean(value, priv->map_auto_download);
            break;
        case PROP_REPO_URI:
            g_value_set_string(value, priv->repo_uri);
            break;
        case PROP_PROXY_URI:
            g_value_set_string(value, priv->proxy_uri);
            break;
        case PROP_TILE_CACHE_DIR:
            g_value_set_string(value, priv->cache_dir);
            break;
        case PROP_TILE_CACHE_BASE_DIR:
            g_value_set_string(value, priv->tile_base_dir);
            break;
        case PROP_TILE_CACHE_DIR_IS_FULL_PATH:
            g_value_set_boolean(value, FALSE);
            break;
        case PROP_ZOOM:
            g_value_set_int(value, priv->map_zoom);
            break;
        case PROP_MAX_ZOOM:
            g_value_set_int(value, priv->max_zoom);
            break;
        case PROP_MIN_ZOOM:
            g_value_set_int(value, priv->min_zoom);
            break;
        case PROP_LATITUDE:
            g_value_set_float(value, rad2deg(priv->center_rlat));
            break;
        case PROP_LONGITUDE:
            g_value_set_float(value, rad2deg(priv->center_rlon));
            break;
        case PROP_MAP_X:
            g_value_set_int(value, priv->map_x);
            break;
        case PROP_MAP_Y:
            g_value_set_int(value, priv->map_y);
            break;
        case PROP_TILES_QUEUED:
            g_value_set_int(value, g_hash_table_size(priv->tile_queue));
            break;
        case PROP_GPS_TRACK_WIDTH:
            g_value_set_int(value, priv->ui_gps_track_width);
            break;
        case PROP_GPS_POINT_R1:
            g_value_set_int(value, priv->ui_gps_point_inner_radius);
            break;
        case PROP_GPS_POINT_R2:
            g_value_set_int(value, priv->ui_gps_point_outer_radius);
            break;
        case PROP_MAP_SOURCE:
            g_value_set_int(value, priv->map_source);
            break;
        case PROP_IMAGE_FORMAT:
            g_value_set_string(value, priv->image_format);
            break;
        case PROP_VIEWPORT_WIDTH:
            g_value_set_uint(value, priv->viewport_width);
            g_message("get width %d.", priv->viewport_width);
            break;
        case PROP_VIEWPORT_HEIGHT:
            g_value_set_uint(value, priv->viewport_height);
            g_message("get height %d.", priv->viewport_height);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
            break;
    }
}

void
osm_gps_map_set_viewport (OsmGpsMap *map, guint width, guint height)
{
    OsmGpsMapPrivate *priv;

    g_return_if_fail(OSM_IS_GPS_MAP(map));
    priv = map->priv;

    if (priv->viewport_width == width &&
        priv->viewport_height == height)
        return;

    /* Set viewport. */
    g_message("Set view port to %dx%d.", width, height);
    priv->viewport_width = width;
    priv->viewport_height = height;

    if (priv->cr_surf)
        cairo_surface_destroy(priv->cr_surf);
    priv->cr_surf = cairo_image_surface_create
        (CAIRO_FORMAT_ARGB32,
         priv->viewport_width + EXTRA_BORDER * 2,
         priv->viewport_height + EXTRA_BORDER * 2);
    if (priv->cr)
        cairo_destroy (priv->cr);
    priv->cr = cairo_create (priv->cr_surf);

    // pixel_x,y, offsets
    gint pixel_x = lon2pixel(priv->map_zoom, priv->center_rlon);
    gint pixel_y = lat2pixel(priv->map_zoom, priv->center_rlat);

    priv->map_x = pixel_x - priv->viewport_width/2;
    priv->map_y = pixel_y - priv->viewport_height/2;

    osm_gps_map_redraw(map);

    g_signal_emit_by_name(map, "changed");
}


static void
osm_gps_map_class_init (OsmGpsMapClass *klass)
{
    GObjectClass* object_class = G_OBJECT_CLASS (klass);

    g_type_class_add_private (klass, sizeof (OsmGpsMapPrivate));

    object_class->dispose = osm_gps_map_dispose;
    object_class->finalize = osm_gps_map_finalize;
    object_class->constructor = osm_gps_map_constructor;
    object_class->set_property = osm_gps_map_set_property;
    object_class->get_property = osm_gps_map_get_property;

    properties[PROP_DOUBLE_PIXEL] = g_param_spec_boolean ("double-pixel",
                                                          "double pixel",
                                                          "double map pixels for better readability",
                                                          FALSE,
                                                          G_PARAM_READABLE | G_PARAM_WRITABLE | G_PARAM_CONSTRUCT);
    g_object_class_install_property (object_class,
                                     PROP_DOUBLE_PIXEL,
                                     properties[PROP_DOUBLE_PIXEL]);


    g_object_class_install_property (object_class,
                                     PROP_AUTO_CENTER,
                                     g_param_spec_boolean ("auto-center",
                                                           "auto center",
                                                           "map auto center",
                                                           TRUE,
                                                           G_PARAM_READABLE | G_PARAM_WRITABLE | G_PARAM_CONSTRUCT));

    g_object_class_install_property (object_class,
                                     PROP_RECORD_TRIP_HISTORY,
                                     g_param_spec_boolean ("record-trip-history",
                                                           "record trip history",
                                                           "should all gps points be recorded in a trip history",
                                                           TRUE,
                                                           G_PARAM_READABLE | G_PARAM_WRITABLE | G_PARAM_CONSTRUCT));

    g_object_class_install_property (object_class,
                                     PROP_SHOW_TRIP_HISTORY,
                                     g_param_spec_boolean ("show-trip-history",
                                                           "show trip history",
                                                           "should the recorded trip history be shown on the map",
                                                           TRUE,
                                                           G_PARAM_READABLE | G_PARAM_WRITABLE | G_PARAM_CONSTRUCT));

    g_object_class_install_property (object_class,
                                     PROP_AUTO_DOWNLOAD,
                                     g_param_spec_boolean ("auto-download",
                                                           "auto download",
                                                           "map auto download",
                                                           TRUE,
                                                           G_PARAM_READABLE | G_PARAM_WRITABLE | G_PARAM_CONSTRUCT));

    g_object_class_install_property (object_class,
                                     PROP_REPO_URI,
                                     g_param_spec_string ("repo-uri",
                                                          "repo uri",
                                                          "map source tile repository uri",
                                                          OSM_REPO_URI,
                                                          G_PARAM_READABLE | G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));

     g_object_class_install_property (object_class,
                                     PROP_PROXY_URI,
                                     g_param_spec_string ("proxy-uri",
                                                          "proxy uri",
                                                          "http proxy uri on NULL",
                                                          NULL,
                                                          G_PARAM_READABLE | G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));

    g_object_class_install_property (object_class,
                                     PROP_TILE_CACHE_DIR,
                                     g_param_spec_string ("tile-cache",
                                                          "tile cache",
                                                          "osm local tile cache dir",
                                                          OSM_GPS_MAP_CACHE_AUTO,
                                                          G_PARAM_READABLE | G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));

    g_object_class_install_property (object_class,
                                     PROP_TILE_CACHE_BASE_DIR,
                                     g_param_spec_string ("tile-cache-base",
                                                          "tile cache-base",
                                                          "base directory to which friendly and auto paths are appended",
                                                          NULL,
                                                          G_PARAM_READABLE | G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));

     g_object_class_install_property (object_class,
                                      PROP_TILE_CACHE_DIR_IS_FULL_PATH,
                                      g_param_spec_boolean ("tile-cache-is-full-path",
                                                            "tile cache is full path",
                                                            "DEPRECIATED",
                                                            FALSE,
                                                            G_PARAM_READABLE | G_PARAM_WRITABLE));

     properties[PROP_ZOOM] = g_param_spec_int ("zoom",
                                               "zoom",
                                               "initial zoom level",
                                               MIN_ZOOM, /* minimum property value */
                                               MAX_ZOOM, /* maximum property value */
                                               3,
                                               G_PARAM_READABLE | G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY);
    g_object_class_install_property (object_class,
                                     PROP_ZOOM,
                                     properties[PROP_ZOOM]);

    g_object_class_install_property (object_class,
                                     PROP_MAX_ZOOM,
                                     g_param_spec_int ("max-zoom",
                                                       "max zoom",
                                                       "maximum zoom level",
                                                       MIN_ZOOM, /* minimum property value */
                                                       MAX_ZOOM, /* maximum property value */
                                                       OSM_MAX_ZOOM,
                                                       G_PARAM_READABLE | G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));

    g_object_class_install_property (object_class,
                                     PROP_MIN_ZOOM,
                                     g_param_spec_int ("min-zoom",
                                                       "min zoom",
                                                       "minimum zoom level",
                                                       MIN_ZOOM, /* minimum property value */
                                                       MAX_ZOOM, /* maximum property value */
                                                       OSM_MIN_ZOOM,
                                                       G_PARAM_READABLE | G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));

    g_object_class_install_property (object_class,
                                     PROP_LATITUDE,
                                     g_param_spec_float ("latitude",
                                                         "latitude",
                                                         "latitude in degrees",
                                                         -90.0, /* minimum property value */
                                                         90.0, /* maximum property value */
                                                         0,
                                                         G_PARAM_READABLE));

    g_object_class_install_property (object_class,
                                     PROP_LONGITUDE,
                                     g_param_spec_float ("longitude",
                                                         "longitude",
                                                         "longitude in degrees",
                                                         -180.0, /* minimum property value */
                                                         180.0, /* maximum property value */
                                                         0,
                                                         G_PARAM_READABLE));

    properties[PROP_MAP_X] = g_param_spec_int ("map-x",
                                               "map-x",
                                               "initial map x location",
                                               G_MININT, /* minimum property value */
                                               G_MAXINT, /* maximum property value */
                                               890,
                                               G_PARAM_READABLE | G_PARAM_WRITABLE);
    g_object_class_install_property (object_class,
                                     PROP_MAP_X,
                                     properties[PROP_MAP_X]);

    properties[PROP_MAP_Y] = g_param_spec_int ("map-y",
                                               "map-y",
                                               "initial map y location",
                                               G_MININT, /* minimum property value */
                                               G_MAXINT, /* maximum property value */
                                               515,
                                               G_PARAM_READABLE | G_PARAM_WRITABLE);
    g_object_class_install_property (object_class,
                                     PROP_MAP_Y,
                                     properties[PROP_MAP_Y]);

    g_object_class_install_property (object_class,
                                     PROP_TILES_QUEUED,
                                     g_param_spec_int ("tiles-queued",
                                                       "tiles-queued",
                                                       "number of tiles currently waiting to download",
                                                       G_MININT, /* minimum property value */
                                                       G_MAXINT, /* maximum property value */
                                                       0,
                                                       G_PARAM_READABLE));

    g_object_class_install_property (object_class,
                                     PROP_GPS_TRACK_WIDTH,
                                     g_param_spec_int ("gps-track-width",
                                                       "gps-track-width",
                                                       "width of the lines drawn for the gps track",
                                                       1,           /* minimum property value */
                                                       G_MAXINT,    /* maximum property value */
                                                       4,
                                                       G_PARAM_READABLE | G_PARAM_WRITABLE | G_PARAM_CONSTRUCT));

    g_object_class_install_property (object_class,
                                     PROP_GPS_POINT_R1,
                                     g_param_spec_int ("gps-track-point-radius",
                                                       "gps-track-point-radius",
                                                       "radius of the gps point inner circle",
                                                       0,           /* minimum property value */
                                                       G_MAXINT,    /* maximum property value */
                                                       5,
                                                       G_PARAM_READABLE | G_PARAM_WRITABLE | G_PARAM_CONSTRUCT));

    g_object_class_install_property (object_class,
                                     PROP_GPS_POINT_R2,
                                     g_param_spec_int ("gps-track-highlight-radius",
                                                       "gps-track-highlight-radius",
                                                       "radius of the gps point highlight circle",
                                                       0,           /* minimum property value */
                                                       G_MAXINT,    /* maximum property value */
                                                       20,
                                                       G_PARAM_READABLE | G_PARAM_WRITABLE | G_PARAM_CONSTRUCT));

    g_object_class_install_property (object_class,
                                     PROP_MAP_SOURCE,
                                     g_param_spec_int ("map-source",
                                                       "map source",
                                                       "map source ID",
                                                       -1,          /* minimum property value */
                                                       G_MAXINT,    /* maximum property value */
                                                       -1,
                                                       G_PARAM_READABLE | G_PARAM_WRITABLE | G_PARAM_CONSTRUCT));

    g_object_class_install_property (object_class,
                                     PROP_IMAGE_FORMAT,
                                     g_param_spec_string ("image-format",
                                                          "image format",
                                                          "map source tile repository image format (jpg, png)",
                                                          OSM_IMAGE_FORMAT,
                                                          G_PARAM_READABLE | G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));

    g_object_class_install_property (object_class,
                                     PROP_VIEWPORT_WIDTH,
                                     g_param_spec_uint ("viewport-width",
                                                       "Viewport width",
                                                       "width of the viewable area",
                                                       1, /* minimum property value */
                                                       2048, /* maximum property value */
                                                       1,
                                                       G_PARAM_READABLE | G_PARAM_WRITABLE));

    g_object_class_install_property (object_class,
                                     PROP_VIEWPORT_HEIGHT,
                                     g_param_spec_uint ("viewport-height",
                                                       "Viewport height",
                                                       "height of the viewable area",
                                                       1, /* minimum property value */
                                                       2048, /* maximum property value */
                                                       1,
                                                       G_PARAM_READABLE | G_PARAM_WRITABLE));

    g_signal_new ("changed", OSM_TYPE_GPS_MAP,
                  G_SIGNAL_RUN_FIRST, 0, NULL, NULL,
                  g_cclosure_marshal_VOID__VOID, G_TYPE_NONE, 0);

    g_signal_new ("dirty", OSM_TYPE_GPS_MAP,
                  G_SIGNAL_RUN_FIRST, 0, NULL, NULL,
                  g_cclosure_marshal_VOID__VOID, G_TYPE_NONE, 0);
}

const char* 
osm_gps_map_source_get_friendly_name(OsmGpsMapSource_t source)
{
    switch(source)
    {
        case OSM_GPS_MAP_SOURCE_NULL:
            return "None";
        case OSM_GPS_MAP_SOURCE_OPENSTREETMAP:
            return "OpenStreetMap I";
        case OSM_GPS_MAP_SOURCE_OPENSTREETMAP_RENDERER:
            return "OpenStreetMap II";
        case OSM_GPS_MAP_SOURCE_OPENAERIALMAP:
            return "OpenAerialMap";
        case OSM_GPS_MAP_SOURCE_OPENCYCLEMAP:
            return "OpenCycleMap";
        case OSM_GPS_MAP_SOURCE_OSM_PUBLIC_TRANSPORT:
            return "Public Transport";
        case OSM_GPS_MAP_SOURCE_OSMC_TRAILS:
            return "OSMC Trails";
        case OSM_GPS_MAP_SOURCE_MAPS_FOR_FREE:
            return "Maps-For-Free";
        case OSM_GPS_MAP_SOURCE_GOOGLE_STREET:
            return "Google Maps";
        case OSM_GPS_MAP_SOURCE_GOOGLE_SATELLITE:
            return "Google Satellite";
        case OSM_GPS_MAP_SOURCE_GOOGLE_HYBRID:
            return "Google Hybrid";
        case OSM_GPS_MAP_SOURCE_VIRTUAL_EARTH_STREET:
            return "Virtual Earth";
        case OSM_GPS_MAP_SOURCE_VIRTUAL_EARTH_SATELLITE:
            return "Virtual Earth Satellite";
        case OSM_GPS_MAP_SOURCE_VIRTUAL_EARTH_HYBRID:
            return "Virtual Earth Hybrid";
        case OSM_GPS_MAP_SOURCE_YAHOO_STREET:
            return "Yahoo Maps";
        case OSM_GPS_MAP_SOURCE_YAHOO_SATELLITE:
            return "Yahoo Satellite";
        case OSM_GPS_MAP_SOURCE_YAHOO_HYBRID:
            return "Yahoo Hybrid";
        case OSM_GPS_MAP_SOURCE_LAST:
        default:
            return NULL;
    }
    return NULL;
}

//http://www.internettablettalk.com/forums/showthread.php?t=5209
//https://garage.maemo.org/plugins/scmsvn/viewcvs.php/trunk/src/maps.c?root=maemo-mapper&view=markup
//http://www.ponies.me.uk/maps/GoogleTileUtils.java
//http://www.mgmaps.com/cache/MapTileCacher.perl
const char* 
osm_gps_map_source_get_repo_uri(OsmGpsMapSource_t source)
{
    switch(source)
    {
        case OSM_GPS_MAP_SOURCE_NULL:
            return "none://";
        case OSM_GPS_MAP_SOURCE_OPENSTREETMAP:
            return OSM_REPO_URI;
        case OSM_GPS_MAP_SOURCE_OPENAERIALMAP:
            /* OpenAerialMap is down, offline till furthur notice
               http://openaerialmap.org/pipermail/talk_openaerialmap.org/2008-December/000055.html */
            return NULL;
        case OSM_GPS_MAP_SOURCE_OPENSTREETMAP_RENDERER:
            return "http://tah.openstreetmap.org/Tiles/tile/#Z/#X/#Y.png";
        case OSM_GPS_MAP_SOURCE_OPENCYCLEMAP:
            return "http://c.andy.sandbox.cloudmade.com/tiles/cycle/#Z/#X/#Y.png";
        case OSM_GPS_MAP_SOURCE_OSM_PUBLIC_TRANSPORT:
            return "http://tile.xn--pnvkarte-m4a.de/tilegen/#Z/#X/#Y.png";
        case OSM_GPS_MAP_SOURCE_OSMC_TRAILS:
            return "http://topo.geofabrik.de/trails/#Z/#X/#Y.png";
        case OSM_GPS_MAP_SOURCE_MAPS_FOR_FREE:
            return "http://maps-for-free.com/layer/relief/z#Z/row#Y/#Z_#X-#Y.jpg";
        case OSM_GPS_MAP_SOURCE_GOOGLE_STREET:
            return "http://mt#R.google.com/vt/v=w2.97&x=#X&y=#Y&z=#Z";
        case OSM_GPS_MAP_SOURCE_GOOGLE_HYBRID:
            /* No longer working
               "http://mt#R.google.com/mt?n=404&v=w2t.99&x=#X&y=#Y&zoom=#S" */
            return NULL;
        case OSM_GPS_MAP_SOURCE_GOOGLE_SATELLITE:
            return "http://khm#R.google.com/kh/v=51&x=#X&y=#Y&z=#Z";
        case OSM_GPS_MAP_SOURCE_VIRTUAL_EARTH_STREET:
            return "http://a#R.ortho.tiles.virtualearth.net/tiles/r#W.jpeg?g=50";
        case OSM_GPS_MAP_SOURCE_VIRTUAL_EARTH_SATELLITE:
            return "http://a#R.ortho.tiles.virtualearth.net/tiles/a#W.jpeg?g=50";
        case OSM_GPS_MAP_SOURCE_VIRTUAL_EARTH_HYBRID:
            return "http://a#R.ortho.tiles.virtualearth.net/tiles/h#W.jpeg?g=50";
        case OSM_GPS_MAP_SOURCE_YAHOO_STREET:
        case OSM_GPS_MAP_SOURCE_YAHOO_SATELLITE:
        case OSM_GPS_MAP_SOURCE_YAHOO_HYBRID:
            /* TODO: Implement signed Y, aka U
             * http://us.maps3.yimg.com/aerial.maps.yimg.com/ximg?v=1.7&t=a&s=256&x=%d&y=%-d&z=%d 
             *  x = tilex,
             *  y = (1 << (MAX_ZOOM - zoom)) - tiley - 1,
             *  z = zoom - (MAX_ZOOM - 17));
             */
            return NULL;
        case OSM_GPS_MAP_SOURCE_LAST:
        default:
            return NULL;
    }
    return NULL;
}

const char *
osm_gps_map_source_get_image_format(OsmGpsMapSource_t source)
{
    switch(source) {
        case OSM_GPS_MAP_SOURCE_NULL:
        case OSM_GPS_MAP_SOURCE_OPENSTREETMAP:
        case OSM_GPS_MAP_SOURCE_OPENSTREETMAP_RENDERER:
        case OSM_GPS_MAP_SOURCE_OPENCYCLEMAP:
        case OSM_GPS_MAP_SOURCE_OSM_PUBLIC_TRANSPORT:
        case OSM_GPS_MAP_SOURCE_OSMC_TRAILS:
            return "png";
        case OSM_GPS_MAP_SOURCE_OPENAERIALMAP:
        case OSM_GPS_MAP_SOURCE_GOOGLE_STREET:
        case OSM_GPS_MAP_SOURCE_GOOGLE_HYBRID:
        case OSM_GPS_MAP_SOURCE_VIRTUAL_EARTH_STREET:
        case OSM_GPS_MAP_SOURCE_VIRTUAL_EARTH_SATELLITE:
        case OSM_GPS_MAP_SOURCE_VIRTUAL_EARTH_HYBRID:
        case OSM_GPS_MAP_SOURCE_YAHOO_STREET:
        case OSM_GPS_MAP_SOURCE_YAHOO_SATELLITE:
        case OSM_GPS_MAP_SOURCE_YAHOO_HYBRID:
        case OSM_GPS_MAP_SOURCE_MAPS_FOR_FREE:
        case OSM_GPS_MAP_SOURCE_GOOGLE_SATELLITE:
            return "jpg";
        case OSM_GPS_MAP_SOURCE_LAST:
        default:
            return "bin";
    }
    return "bin";
}


int 
osm_gps_map_source_get_min_zoom(OsmGpsMapSource_t source)
{
    return 1;
}

int 
osm_gps_map_source_get_max_zoom(OsmGpsMapSource_t source)
{
    switch(source) {
        case OSM_GPS_MAP_SOURCE_NULL:
            return 18;
        case OSM_GPS_MAP_SOURCE_OPENSTREETMAP:
        case OSM_GPS_MAP_SOURCE_OPENCYCLEMAP:
        case OSM_GPS_MAP_SOURCE_OSM_PUBLIC_TRANSPORT:
            return OSM_MAX_ZOOM;
        case OSM_GPS_MAP_SOURCE_OPENSTREETMAP_RENDERER:
        case OSM_GPS_MAP_SOURCE_OPENAERIALMAP:
        case OSM_GPS_MAP_SOURCE_GOOGLE_STREET:
        case OSM_GPS_MAP_SOURCE_GOOGLE_HYBRID:
        case OSM_GPS_MAP_SOURCE_VIRTUAL_EARTH_STREET:
        case OSM_GPS_MAP_SOURCE_VIRTUAL_EARTH_SATELLITE:
        case OSM_GPS_MAP_SOURCE_VIRTUAL_EARTH_HYBRID:
        case OSM_GPS_MAP_SOURCE_YAHOO_STREET:
        case OSM_GPS_MAP_SOURCE_YAHOO_SATELLITE:
        case OSM_GPS_MAP_SOURCE_YAHOO_HYBRID:
            return 17;
        case OSM_GPS_MAP_SOURCE_OSMC_TRAILS:
            return 15;
        case OSM_GPS_MAP_SOURCE_MAPS_FOR_FREE:
            return 11;
        case OSM_GPS_MAP_SOURCE_GOOGLE_SATELLITE:
            return 18;
        case OSM_GPS_MAP_SOURCE_LAST:
        default:
            return 17;
    }
    return 17;
}

gboolean
osm_gps_map_source_is_valid(OsmGpsMapSource_t source)
{
    return osm_gps_map_source_get_repo_uri(source) != NULL;
}

void
osm_gps_map_download_maps (OsmGpsMap *map, coord_t *pt1, coord_t *pt2, int zoom_start, int zoom_end)
{
    int i,j,zoom,num_tiles;
    OsmGpsMapPrivate *priv = map->priv;

    if (pt1 && pt2)
    {
        gchar *filename;
        num_tiles = 0;
        zoom_end = CLAMP(zoom_end, priv->min_zoom, priv->max_zoom);
        g_debug("Download maps: z:%d->%d",zoom_start, zoom_end);

        for(zoom=zoom_start; zoom<=zoom_end; zoom++)
        {
            int x1,y1,x2,y2;

            x1 = (int)floor((float)lon2pixel(zoom, pt1->rlon) / (float)TILESIZE);
            y1 = (int)floor((float)lat2pixel(zoom, pt1->rlat) / (float)TILESIZE);

            x2 = (int)floor((float)lon2pixel(zoom, pt2->rlon) / (float)TILESIZE);
            y2 = (int)floor((float)lat2pixel(zoom, pt2->rlat) / (float)TILESIZE);

            // loop x1-x2
            for(i=x1; i<=x2; i++)
            {
                // loop y1 - y2
                for(j=y1; j<=y2; j++)
                {
                    // x = i, y = j
                    filename = g_strdup_printf("%s%c%d%c%d%c%d.%s",
                                    priv->cache_dir, G_DIR_SEPARATOR,
                                    zoom, G_DIR_SEPARATOR,
                                    i, G_DIR_SEPARATOR,
                                    j,
                                    priv->image_format);

                    if ((!g_file_test(filename, G_FILE_TEST_EXISTS)) ||
                        osm_gps_map_tile_age_exceeded(filename))
                    {
                        osm_gps_map_download_tile(map, zoom, i, j, FALSE);
                        num_tiles++;
                    } 

                    g_free(filename);
                }
            }
            g_debug("DL @Z:%d = %d tiles",zoom,num_tiles);
        }
    }
}

void
osm_gps_map_get_bbox (OsmGpsMap *map, coord_t *pt1, coord_t *pt2)
{
    OsmGpsMapPrivate *priv = map->priv;

    if (pt1 && pt2) {
        pt1->rlat = pixel2lat(priv->map_zoom, priv->map_y);
        pt1->rlon = pixel2lon(priv->map_zoom, priv->map_x);
        pt2->rlat = pixel2lat(priv->map_zoom, priv->map_y + priv->viewport_height);
        pt2->rlon = pixel2lon(priv->map_zoom, priv->map_x + priv->viewport_width);

        g_debug("BBOX: %f %f %f %f", pt1->rlat, pt1->rlon, pt2->rlat, pt2->rlon);
    }
}

void
osm_gps_map_set_mapcenter (OsmGpsMap *map, float latitude, float longitude, int zoom)
{
    osm_gps_map_set_center (map, latitude, longitude);
    osm_gps_map_set_zoom (map, zoom);
}

void
osm_gps_map_set_center (OsmGpsMap *map, float latitude, float longitude)
{
    int pixel_x, pixel_y;
    OsmGpsMapPrivate *priv;

    g_return_if_fail (OSM_IS_GPS_MAP (map));
    priv = map->priv;

    g_object_set(G_OBJECT(map), "auto-center", FALSE, NULL);

    priv->center_rlat = deg2rad(latitude);
    priv->center_rlon = deg2rad(longitude);

    // pixel_x,y, offsets
    pixel_x = lon2pixel(priv->map_zoom, priv->center_rlon);
    pixel_y = lat2pixel(priv->map_zoom, priv->center_rlat);

    priv->map_x = pixel_x - priv->viewport_width/2;
    priv->map_y = pixel_y - priv->viewport_height/2;

    if (!priv->idle_map_redraw)
        priv->idle_map_redraw = g_idle_add((GSourceFunc)osm_gps_map_idle_redraw, map);

    g_signal_emit_by_name(map, "changed");
}

int 
osm_gps_map_set_zoom (OsmGpsMap *map, int zoom)
{
    int zoom_old;
    double factor = 0.0;
    int width_center, height_center;
    OsmGpsMapPrivate *priv;

    g_return_val_if_fail (OSM_IS_GPS_MAP (map), 0);
    priv = map->priv;

    if (zoom != priv->map_zoom)
    {
        width_center  = priv->viewport_width / 2;
        height_center = priv->viewport_height / 2;

        zoom_old = priv->map_zoom;
        //constrain zoom min_zoom -> max_zoom
        priv->map_zoom = CLAMP(zoom, priv->min_zoom, priv->max_zoom);

        priv->map_x = lon2pixel(priv->map_zoom, priv->center_rlon) - width_center;
        priv->map_y = lat2pixel(priv->map_zoom, priv->center_rlat) - height_center;

        factor = pow(2, priv->map_zoom-zoom_old);
        g_debug("Zoom changed from %d to %d factor:%f x:%d",
                zoom_old, priv->map_zoom, factor, priv->map_x);

        /* adjust gps precision indicator */
        priv->ui_gps_point_outer_radius *= factor;

        if (!priv->idle_map_redraw)
            priv->idle_map_redraw = g_idle_add((GSourceFunc)osm_gps_map_idle_redraw, map);

        g_object_notify_by_pspec(G_OBJECT(map), properties[PROP_ZOOM]);
        g_signal_emit_by_name(map, "changed");
    }
    return priv->map_zoom;
}

int
osm_gps_map_zoom_in (OsmGpsMap *map)
{
    g_return_val_if_fail (OSM_IS_GPS_MAP (map), 0);
    return osm_gps_map_set_zoom(map, map->priv->map_zoom+1);
}

int
osm_gps_map_zoom_out (OsmGpsMap *map)
{
    g_return_val_if_fail (OSM_IS_GPS_MAP (map), 0);
    return osm_gps_map_set_zoom(map, map->priv->map_zoom-1);
}

void
osm_gps_map_add_track (OsmGpsMap *map, GSList *track)
{
    OsmGpsMapPrivate *priv;

    g_return_if_fail (OSM_IS_GPS_MAP (map));
    priv = map->priv;

    if (track) {
        priv->tracks = g_slist_append(priv->tracks, track);
        if (!priv->idle_map_redraw)
            priv->idle_map_redraw = g_idle_add((GSourceFunc)osm_gps_map_idle_redraw, map);
    }
}

void
osm_gps_map_replace_track (OsmGpsMap *map, GSList *old_track, GSList *new_track)
{
    OsmGpsMapPrivate *priv;

    if(!old_track) {
        osm_gps_map_add_track (map, new_track);
        return;
    }

    g_return_if_fail (OSM_IS_GPS_MAP (map));
    priv = map->priv;

    GSList *old = g_slist_find(priv->tracks, old_track);
    if(!old) {
        g_warning("track to be replaced not found");
        return;
    }

    old->data = new_track;
    if (!priv->idle_map_redraw)
        priv->idle_map_redraw = g_idle_add((GSourceFunc)osm_gps_map_idle_redraw, map);
}

void
osm_gps_map_clear_tracks (OsmGpsMap *map)
{
    g_return_if_fail (OSM_IS_GPS_MAP (map));

    osm_gps_map_free_tracks(map);
    if (!map->priv->idle_map_redraw)
        map->priv->idle_map_redraw = g_idle_add((GSourceFunc)osm_gps_map_idle_redraw, map);
}

void
osm_gps_map_add_image_with_alignment (OsmGpsMap *map, float latitude, float longitude, cairo_surface_t *image, float xalign, float yalign)
{
    g_return_if_fail (OSM_IS_GPS_MAP (map));

    if (image) {
        OsmGpsMapPrivate *priv = map->priv;
        image_t *im;

        //cache w/h for speed, and add image to list
        im = g_new0(image_t,1);
        im->w = cairo_image_surface_get_width(image);
        im->h = cairo_image_surface_get_height(image);
        im->pt.rlat = deg2rad(latitude);
        im->pt.rlon = deg2rad(longitude);

        //handle alignment
        im->xoffset = xalign * im->w;
        im->yoffset = yalign * im->h;

        cairo_surface_reference(image);
        im->image = image;

        priv->images = g_slist_append(priv->images, im);

        if (!priv->idle_map_redraw)
            priv->idle_map_redraw = g_idle_add((GSourceFunc)osm_gps_map_idle_redraw, map);
    }
}

void
osm_gps_map_add_image (OsmGpsMap *map, float latitude, float longitude, cairo_surface_t *image)
{
    osm_gps_map_add_image_with_alignment (map, latitude, longitude, image, 0.5, 0.5);
}

gboolean
osm_gps_map_remove_image (OsmGpsMap *map, cairo_surface_t *image)
{
    OsmGpsMapPrivate *priv = map->priv;
    if (priv->images) {
        GSList *list;
        for(list = priv->images; list != NULL; list = list->next)
        {
            image_t *im = list->data;
	        if (im->image == image)
	        {
		        priv->images = g_slist_remove_link(priv->images, list);
                cairo_surface_destroy(im->image);
		        g_free(im);
                if (!priv->idle_map_redraw)
                    priv->idle_map_redraw = g_idle_add((GSourceFunc)osm_gps_map_idle_redraw, map);
		        return TRUE;
	        }
        }
    }
    return FALSE;
}

void
osm_gps_map_clear_images (OsmGpsMap *map)
{
    g_return_if_fail (OSM_IS_GPS_MAP (map));

    osm_gps_map_free_images(map);
    if (!map->priv->idle_map_redraw)
        map->priv->idle_map_redraw = g_idle_add((GSourceFunc)osm_gps_map_idle_redraw, map);
}

void
osm_gps_map_draw_gps (OsmGpsMap *map, float latitude, float longitude, float heading)
{
    int pixel_x, pixel_y;
    OsmGpsMapPrivate *priv;

    g_return_if_fail (OSM_IS_GPS_MAP (map));
    priv = map->priv;

    priv->gps->rlat = deg2rad(latitude);
    priv->gps->rlon = deg2rad(longitude);
    priv->gps_valid = TRUE;
    priv->gps_heading = deg2rad(heading);

    // pixel_x,y, offsets
    pixel_x = lon2pixel(priv->map_zoom, priv->gps->rlon);
    pixel_y = lat2pixel(priv->map_zoom, priv->gps->rlat);

    //If trip marker add to list of gps points.
    if (priv->record_trip_history) {
        coord_t *tp = g_new0(coord_t,1);
        tp->rlat = priv->gps->rlat;
        tp->rlon = priv->gps->rlon;
        priv->trip_history = g_slist_append(priv->trip_history, tp);
    }

    // dont draw anything if we are dragging
    g_error("implement here.");
    /* if (priv->dragging) { */
    /*     g_debug("Dragging"); */
    /*     return; */
    /* } */

    //Automatically center the map if the track approaches the edge
    if(priv->map_auto_center)   {
        int x = pixel_x - priv->map_x;
        int y = pixel_y - priv->map_y;
        int width = priv->viewport_width;
        int height = priv->viewport_height;
        if( x < (width/2 - width/8)     || x > (width/2 + width/8)  ||
            y < (height/2 - height/8)   || y > (height/2 + height/8)) {

            priv->map_x = pixel_x - width/2;
            priv->map_y = pixel_y - height/2;
            center_coord_update(map);
        }
    }

    // this redraws the map (including the gps track, and adjusts the
    // map center if it was changed
    if (!priv->idle_map_redraw)
        priv->idle_map_redraw = g_idle_add((GSourceFunc)osm_gps_map_idle_redraw, map);
}

void
osm_gps_map_clear_gps (OsmGpsMap *map)
{
    g_return_if_fail(OSM_IS_GPS_MAP(map));

    osm_gps_map_free_trip(map);
    if (!map->priv->idle_map_redraw)
        map->priv->idle_map_redraw = g_idle_add((GSourceFunc)osm_gps_map_idle_redraw, map);
}

coord_t
osm_gps_map_get_co_ordinates (OsmGpsMap *map, int pixel_x, int pixel_y)
{
    coord_t coord;
    OsmGpsMapPrivate *priv = map->priv;

    coord.rlat = pixel2lat(priv->map_zoom, priv->map_y + pixel_y);
    coord.rlon = pixel2lon(priv->map_zoom, priv->map_x + pixel_x);
    return coord;
}

OsmGpsMap *
osm_gps_map_new (void)
{
    return g_object_new (OSM_TYPE_GPS_MAP, NULL);
}

void
osm_gps_map_screen_to_geographic (OsmGpsMap *map, gint pixel_x, gint pixel_y,
                                  gfloat *latitude, gfloat *longitude)
{
    OsmGpsMapPrivate *priv;

    g_return_if_fail (OSM_IS_GPS_MAP (map));
    priv = map->priv;

    if (latitude)
        *latitude = rad2deg(pixel2lat(priv->map_zoom, priv->map_y + pixel_y));
    if (longitude)
        *longitude = rad2deg(pixel2lon(priv->map_zoom, priv->map_x + pixel_x));
}

void
osm_gps_map_geographic_to_screen (OsmGpsMap *map,
                                  gfloat latitude, gfloat longitude,
                                  gint *pixel_x, gint *pixel_y)
{
    OsmGpsMapPrivate *priv;

    g_return_if_fail (OSM_IS_GPS_MAP (map));
    priv = map->priv;

    if (pixel_x)
        *pixel_x = lon2pixel(priv->map_zoom, deg2rad(longitude)) - priv->map_x;
    if (pixel_y)
        *pixel_y = lat2pixel(priv->map_zoom, deg2rad(latitude)) - priv->map_y;
}

void
osm_gps_map_scroll (OsmGpsMap *map, gint dx, gint dy)
{
    OsmGpsMapPrivate *priv;

    g_return_if_fail (OSM_IS_GPS_MAP (map));
    priv = map->priv;

    g_message("scroll of %dx%d.", dx, dy);
    priv->map_x += dx;
    priv->map_y += dy;
    center_coord_update(map);

    if (!priv->idle_map_redraw)
        priv->idle_map_redraw = g_idle_add((GSourceFunc)osm_gps_map_idle_redraw, map);

    g_object_notify_by_pspec(G_OBJECT(map), properties[PROP_MAP_X]);
    g_object_notify_by_pspec(G_OBJECT(map), properties[PROP_MAP_Y]);
}

float
osm_gps_map_get_scale(OsmGpsMap *map)
{
    OsmGpsMapPrivate *priv;

    g_return_val_if_fail (OSM_IS_GPS_MAP (map), OSM_GPS_MAP_INVALID);
    priv = map->priv;

    return osm_gps_map_get_scale_at_point(priv->map_zoom, priv->center_rlat, priv->center_rlon);
}

cairo_surface_t*
osm_gps_map_get_surface(OsmGpsMap *map)
{
    OsmGpsMapPrivate *priv;

    g_return_val_if_fail (OSM_IS_GPS_MAP (map), (cairo_surface_t*)0);
    priv = map->priv;

    cairo_surface_reference(priv->cr_surf);
    cairo_surface_flush(priv->cr_surf);
    return priv->cr_surf;
}

char * osm_gps_map_get_default_cache_directory(void)
{
    return g_build_filename(
                        g_get_user_cache_dir(),
                        "osmgpsmap",
                        NULL);
}

#ifdef ENABLE_OSD
coord_t *
osm_gps_map_get_gps (OsmGpsMap *map) 
{
    g_return_val_if_fail (OSM_IS_GPS_MAP (map), NULL);

    if(!map->priv->gps_valid)
        return NULL;
    
    return map->priv->gps;
}

#endif
