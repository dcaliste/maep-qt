#include <string.h>
#include <math.h>

#include "misc.h"
#include "icon.h"
#include "converter.h"
#include "layer-wiki.h"
#include "osm-gps-map/osm-gps-map-types.h"

struct _MaepWikiContextPrivate
{
  gboolean dispose_has_run;

  OsmGpsMap *map;
  GSList *list;
  gulong timer_id, changed_handler_id;
  gboolean downloading;
  
  /* Just a pointer on a list entry. */
  MaepGeonamesEntry *balloon_src0;
  /* A deep copy of entry to represent a balloon. */
  MaepGeonamesEntry *balloon_src;
  //a balloon with additional info
  struct {
    cairo_surface_t *surface;
    int orientation, offset_x, offset_y;

    float lat, lon;
    OsmGpsMapRect_t rect;
  } balloon;
};

#ifdef MAEMO5
#include <hildon/hildon-pannable-area.h>
#define ICON_SIZE_STR "48"
#define ICON_SIZE      48
#else
#ifdef USE_MAEMO
#define ICON_SIZE_STR "32"
#define ICON_SIZE      32
#else
#ifdef SAILFISH
#define ICON_SIZE_STR "32"
#define ICON_SIZE      32
#else
#define ICON_SIZE_STR "24"
#define ICON_SIZE      24
#endif
#endif
#endif

enum {
  ENTRY_SELECTED_SIGNAL,
  LAST_SIGNAL
};
static guint _signals[LAST_SIGNAL] = { 0 };

static cairo_surface_t *surface_test_font;
static cairo_t *context_test_font;

static void maep_wiki_context_dispose(GObject* obj);
static void maep_wiki_context_finalize(GObject* obj);
static void osm_gps_map_layer_interface_init(OsmGpsMapLayerIface *iface);
static void maep_wiki_context_render(OsmGpsMapLayer *self, OsmGpsMap *map);
static void maep_wiki_context_draw(OsmGpsMapLayer *self, cairo_t *cr,
                                   int width, int height,
                                   int map_x0, int map_y0, int zoom);
static gboolean maep_wiki_context_busy(OsmGpsMapLayer *self);
static gboolean maep_wiki_context_button(OsmGpsMapLayer *self, int x, int y, gboolean press);
static void set_balloon (MaepWikiContextPrivate *priv,
                         float latitude, float longitude);
static void render_balloon(MaepWikiContextPrivate *priv,
                           int width, int height, int xs, int ys);
static void clear_balloon (MaepWikiContextPrivate *priv);

G_DEFINE_TYPE_WITH_CODE(MaepWikiContext, maep_wiki_context,
                        G_TYPE_OBJECT,
                        G_IMPLEMENT_INTERFACE(OSM_TYPE_GPS_MAP_LAYER,
                                              osm_gps_map_layer_interface_init))

static void maep_wiki_context_class_init(MaepWikiContextClass *klass)
{
  g_message("Class init wiki context.");
  /* Connect the overloading methods. */
  G_OBJECT_CLASS(klass)->dispose      = maep_wiki_context_dispose;
  G_OBJECT_CLASS(klass)->finalize     = maep_wiki_context_finalize;

  _signals[ENTRY_SELECTED_SIGNAL] = 
    g_signal_new ("entry-selected", G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST | G_SIGNAL_NO_RECURSE | G_SIGNAL_NO_HOOKS,
                  0 , NULL, NULL, g_cclosure_marshal_VOID__POINTER,
                  G_TYPE_NONE, 1, G_TYPE_POINTER);

  g_type_class_add_private(klass, sizeof(MaepWikiContextPrivate));

  surface_test_font = 
    cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 10,10);
  context_test_font = cairo_create(surface_test_font);
}
static void osm_gps_map_layer_interface_init(OsmGpsMapLayerIface *iface)
{
  g_message("setup layer interface for wiki context.");
  iface->render = maep_wiki_context_render;
  iface->draw = maep_wiki_context_draw;
  iface->busy = maep_wiki_context_busy;
  iface->button = maep_wiki_context_button;
}
static void maep_wiki_context_init(MaepWikiContext *obj)
{
  g_message("New wiki context %p.", (gpointer)obj);
  obj->priv = G_TYPE_INSTANCE_GET_PRIVATE(obj, MAEP_TYPE_WIKI_CONTEXT,
                                          MaepWikiContextPrivate);
  obj->priv->dispose_has_run = FALSE;
  obj->priv->map = NULL;
  obj->priv->timer_id = 0;
  obj->priv->downloading = FALSE;
  obj->priv->balloon_src0 = NULL;
  obj->priv->balloon_src  = NULL;
}
static void maep_wiki_context_dispose(GObject* obj)
{
  MaepWikiContextPrivate *priv = MAEP_WIKI_CONTEXT(obj)->priv;

  if (priv->dispose_has_run)
    return;
  priv->dispose_has_run = TRUE;

  if (priv->changed_handler_id)
    g_signal_handler_disconnect(priv->map, priv->changed_handler_id);

  /* Chain up to the parent class */
  G_OBJECT_CLASS(maep_wiki_context_parent_class)->dispose(obj);
}
static void maep_wiki_context_finalize(GObject* obj)
{
  MaepWikiContextPrivate *priv = MAEP_WIKI_CONTEXT(obj)->priv;

  maep_geonames_entry_list_free(priv->list);

  g_object_unref(priv->map);
  if (priv->timer_id)
    g_source_remove(priv->timer_id);
  if (priv->balloon_src)
    maep_geonames_entry_free(priv->balloon_src);
  clear_balloon(priv);

  G_OBJECT_CLASS(maep_wiki_context_parent_class)->finalize(obj);
}

/* ------------- begin of freeing ------------------ */

static void clear_balloon (MaepWikiContextPrivate *priv) {
    if(priv->balloon.surface) {
        cairo_surface_destroy(priv->balloon.surface);
        priv->balloon.surface = NULL;
        priv->balloon.lat = OSM_GPS_MAP_INVALID;
        priv->balloon.lon = OSM_GPS_MAP_INVALID;
    }
}

/* return distance between both points */
static float get_distance(coord_t *c0, coord_t *c1) {
  float aob = acos(cos(c0->rlat) * cos(c1->rlat) * cos(c1->rlon - c0->rlon) +
		   sin(c0->rlat) * sin(c1->rlat));

  return(aob * 6371000.0);     /* great circle radius in meters */
}

static int dist2pixel(OsmGpsMap *map, float m) {
  return m/osm_gps_map_get_scale(map);
}

/* ------------- end of freeing ------------------ */

static gboolean maep_wiki_context_button(OsmGpsMapLayer *self,
                                         int x, int y, gboolean press)
{
  MaepWikiContextPrivate *priv = MAEP_WIKI_CONTEXT(self)->priv;
  MaepGeonamesEntry *nearest;
  coord_t coord;
  float dst;
  gboolean is_in_balloon;

  coord = osm_gps_map_get_co_ordinates(priv->map, x, y);
  is_in_balloon = FALSE;

  /* Test the baloon, if any. */
  if (priv->balloon_src &&
      priv->balloon.rect.x != G_MAXINT && priv->balloon.rect.y != G_MAXINT)
    {
      is_in_balloon =
        (x > priv->balloon.rect.x - EXTRA_BORDER) &&
        (x < priv->balloon.rect.x - EXTRA_BORDER + priv->balloon.rect.w) &&
        (y > priv->balloon.rect.y - EXTRA_BORDER) &&
        (y < priv->balloon.rect.y - EXTRA_BORDER + priv->balloon.rect.h);
      g_message("rect (%d) %dx%d , %dx%d at %dx%d", is_in_balloon, priv->balloon.rect.x - EXTRA_BORDER, priv->balloon.rect.y - EXTRA_BORDER,
                priv->balloon.rect.w, priv->balloon.rect.h, x, y);
      if (is_in_balloon && !press)
        g_signal_emit(self, _signals[ENTRY_SELECTED_SIGNAL], 0,
                      priv->balloon_src, NULL);
      if (!press)
        {
          priv->balloon_src = NULL;
          osm_gps_map_layer_changed(priv->map, self);
        }
      if (is_in_balloon)
        return TRUE;
    }
  if (press) {
    /* the center of the click is in the middle of the icon which in turn */
    /* is ICON_SIZE/2+ICON_SIZE/4 (+4 because of the pin at the icon bottom) */
    /* above the actual object position. */

    /* find closest wikipoint */
    GSList *list = priv->list;
    float nearest_distance = 1000000000.0; 
    nearest = NULL;
    while(list) {
      MaepGeonamesEntry *entry = (MaepGeonamesEntry*)list->data;

      dst = get_distance(&entry->pos, &coord);
      if(dst < nearest_distance) {
	nearest = entry;
	nearest_distance = dst;
      }

      list = g_slist_next(list);
    }

    /* check if click was close enough */
    if(nearest && dist2pixel(priv->map, nearest_distance) >= ICON_SIZE/2)
      nearest = NULL;
    
    priv->balloon_src0 = nearest;
  } else {
    /* a "nearest" is still valid from the click */
    if (priv->balloon_src0)
      {
        dst = get_distance(&priv->balloon_src0->pos, &coord);
        if (dist2pixel(priv->map, dst) < ICON_SIZE/2) {
          priv->balloon_src = maep_geonames_entry_copy(priv->balloon_src0);
          set_balloon(priv, priv->balloon_src->pos.rlat,
                      priv->balloon_src->pos.rlon);
          osm_gps_map_layer_changed(priv->map, self);
        }
        else
          priv->balloon_src0 = NULL;
      }
  }
  return (priv->balloon_src0 != NULL);
}
static gboolean maep_wiki_context_busy(OsmGpsMapLayer *self)
{
  return FALSE;
}
static void maep_wiki_context_render(OsmGpsMapLayer *self, OsmGpsMap *map)
{
}
static void maep_wiki_context_draw(OsmGpsMapLayer *self, cairo_t *cr,
                                   int width, int height,
                                   int map_x0, int map_y0, int zoom)
{
  GSList *list;
  int w, h, x,y,pixel_x,pixel_y;
  MaepWikiContextPrivate *priv = MAEP_WIKI_CONTEXT(self)->priv;

  cairo_surface_t *cr_surf = icon_get_surface(G_OBJECT(self),
                                              "wikipedia_w." ICON_SIZE_STR);

  w = cairo_image_surface_get_width(cr_surf);
  h = cairo_image_surface_get_height(cr_surf);

  g_message("Draw a list of %d Wiki icons.", g_slist_length(priv->list));

  for(list = priv->list; list != NULL; list = list->next)
    {
      MaepGeonamesEntry *entry = (MaepGeonamesEntry*)list->data;

      // pixel_x,y, offsets
      pixel_x = lon2pixel(zoom, entry->pos.rlon);
      pixel_y = lat2pixel(zoom, entry->pos.rlat);

      x = pixel_x - map_x0;
      y = pixel_y - map_y0;

      g_message("Image %dx%d @: %f,%f (%d,%d,%d)",
              w, h,
              entry->pos.rlat, entry->pos.rlon,
                x, y, zoom);

      cairo_set_source_surface(cr, cr_surf, x - 0.5 * w, y - 0.5 * h);
      cairo_paint(cr);
    }

  if (priv->balloon_src)
    {
      g_message("Draw balloon.");

      pixel_x = lon2pixel(zoom, priv->balloon_src->pos.rlon);
      pixel_y = lat2pixel(zoom, priv->balloon_src->pos.rlat);

      x = pixel_x - map_x0;
      y = pixel_y - map_y0;

      render_balloon(priv, width, height, x - EXTRA_BORDER, y - EXTRA_BORDER);
      priv->balloon.rect.x += x + priv->balloon.offset_x;
      priv->balloon.rect.y += y + priv->balloon.offset_y;

      cairo_set_source_surface(cr, priv->balloon.surface, 
                               x + priv->balloon.offset_x, 
                               y + priv->balloon.offset_y);
      cairo_paint(cr);
    }
}

/* --------------------- end of OsmGpsMapLayer interface -------------------- */

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

#ifndef USE_MAEMO
#ifndef SAILFISH
#define BALLOON_FONT 12.0
#else
#define BALLOON_FONT 20.0
#endif
#else
#ifndef MAEMO5
#define BALLOON_FONT 20.0
#else
#define BALLOON_FONT 32.0
#endif
#endif

#define BORDER (BALLOON_FONT/4)

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
render_balloon(MaepWikiContextPrivate *priv, int width, int height, int xs, int ys) {
  cairo_text_extents_t extents;

  if(!priv->balloon.surface)
    return;

  gint x0 = 1, y0 = 1;

  /* check position of this relative to screen center to determine */
  /* pointer direction ... */
  int pointer_x, pointer_x0, pointer_x1;
  int pointer_y;

  /* ... and calculate position */
  int orientation = 0;
  if(xs > width/2) {
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
  if(ys > height/2) {
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
                                  pointer_x0 + BALLOON_SHADOW,
                                  pointer_x1 + BALLOON_SHADOW);

  cairo_set_source_rgba (cr, 0, 0, 0, BALLOON_SHADOW_TRANSPARENCY);
  cairo_fill_preserve (cr);
  cairo_set_source_rgba (cr, 1, 0, 0, 1.0);
  cairo_set_line_width (cr, 0);
  cairo_stroke (cr);
    
  /* --------- draw main shape ----------- */
  osm_gps_map_draw_balloon_shape (cr, x0, y0, x1, y1,
                                  bottom, pointer_x, pointer_y,
                                  pointer_x0, pointer_x1);
        
  cairo_set_source_rgba (cr, 1, 1, 1, BALLOON_TRANSPARENCY);
  cairo_fill_preserve (cr);
  cairo_set_source_rgba (cr, 0, 0, 0, BALLOON_TRANSPARENCY);
  cairo_set_line_width (cr, 1);
  cairo_stroke (cr);

  g_message("Draw balloon.");
    
  cairo_select_font_face (cr, "Sans",
                          CAIRO_FONT_SLANT_NORMAL,
                          CAIRO_FONT_WEIGHT_BOLD);
      
  cairo_set_font_size (cr, BALLOON_FONT);
  cairo_text_extents (cr, priv->balloon_src->title, &extents);

  cairo_move_to (cr, 
                 priv->balloon.rect.x + BORDER - extents.x_bearing, 
                 priv->balloon.rect.y + BORDER - extents.y_bearing);

  cairo_set_source_rgb (cr, 0, 0, 0);
  cairo_show_text (cr, priv->balloon_src->title);
  cairo_stroke (cr);

  cairo_destroy(cr);
}

static void set_balloon (MaepWikiContextPrivate *priv,
                         float latitude, float longitude) {
  cairo_text_extents_t extents;

  clear_balloon (priv);

  priv->balloon.lat = latitude;
  priv->balloon.lon = longitude;
  priv->balloon.orientation = -1;
  priv->balloon.rect.x = G_MAXINT;
  priv->balloon.rect.y = G_MAXINT;
  priv->balloon.rect.w = 0;
  priv->balloon.rect.h = 0;

  /* set default size and call app callback */
  /* we need to figure out the extents in order to determine */
  /* the surface size this in turn requires a valid surface ... */
  cairo_select_font_face (context_test_font, "Sans",
                          CAIRO_FONT_SLANT_NORMAL,
                          CAIRO_FONT_WEIGHT_BOLD);
      
  cairo_set_font_size (context_test_font, BALLOON_FONT);
  cairo_text_extents (context_test_font, priv->balloon_src->title, &extents);

  priv->balloon.rect.w = extents.width + 2*BORDER;
  priv->balloon.rect.h = extents.height + 2*BORDER;

  /* allocate balloon surface */
  priv->balloon.surface = 
    cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 
                               BALLOON_W+2, BALLOON_H+2);
}

static void geonames_wiki_cb(MaepWikiContext *context, GSList *list, GError *error) {
  if (!error)
    {
      /* remove any list that may already be preset */
      if(context->priv->list) {
        context->priv->balloon_src0 = NULL;	
	maep_geonames_entry_list_free(context->priv->list);
	context->priv->list = NULL;
      }
      
      /* render all icons */
      context->priv->list = list;
      osm_gps_map_layer_changed(context->priv->map, OSM_GPS_MAP_LAYER(context));
    }
  else
    {
      g_warning("%s", error->message);
    }

  context->priv->downloading = FALSE;
  g_object_unref(context);
}

/* request geotagged wikipedia entries for current map view */
static void geonames_wiki_request(MaepWikiContext *context) {
  /* don't have more than one pending request */
  if(context->priv->downloading)
    return;

  context->priv->downloading = TRUE;

  /* request area from map */
  coord_t pt1, pt2;
  osm_gps_map_get_bbox (OSM_GPS_MAP(context->priv->map), &pt1, &pt2);

  g_object_ref(context);
  maep_geonames_entry_request(&pt1, &pt2,
                              (MaepGeonamesRequestCallback)geonames_wiki_cb,
                              context);
}

static gboolean wiki_update(gpointer data) {
  MaepWikiContext *context = (MaepWikiContext *)data;

  geonames_wiki_request(context);

  context->priv->timer_id = 0;
  return FALSE;
}

#define WIKI_UPDATE_TIMEOUT (1)

/* the map has changed, the wiki timer needs to be reset to update the */
/* wiki data after 2 seconds of idle time. */
static void 
wiki_timer_trigger(MaepWikiContext *context) {
  g_assert(context);

  /* if the timer is already running, then reset it, otherwise */
  /* install a new one*/
  if(context->priv->timer_id) {
    g_source_remove(context->priv->timer_id);
    context->priv->timer_id = 0;
  }
  
  context->priv->timer_id =
    g_timeout_add_seconds(WIKI_UPDATE_TIMEOUT, wiki_update, context);
}

static void on_map_changed(OsmGpsMap *map, gpointer data ) {
  wiki_timer_trigger((MaepWikiContext*)data);
}

MaepWikiContext* maep_wiki_context_new(OsmGpsMap *map)
{
  MaepWikiContext *context;

  /* create and register a context */
  g_message("create wiki context");
  context = g_object_new(MAEP_TYPE_WIKI_CONTEXT, NULL);

  g_object_ref(G_OBJECT(map));
  context->priv->map = map;

  /* trigger wiki data update either by "changed" events ... */
  context->priv->changed_handler_id =
    g_signal_connect(G_OBJECT(map), "changed",
                     G_CALLBACK(on_map_changed), context);

  return context;
}

/* the wikimedia icons are automagically updated after one second */
/* of idle time */
void maep_wiki_context_enable(MaepWikiContext *context, gboolean enable)
{
  guint width, height;

  g_return_if_fail(MAEP_IS_WIKI_CONTEXT(context));

  if(enable) {
    osm_gps_map_add_layer(context->priv->map, OSM_GPS_MAP_LAYER(context));

    /* only do request if map has a reasonable size. otherwise map may */
    /* still be in setup phase */
    g_object_get(G_OBJECT(context->priv->map), "viewport-width", &width, "viewport-height", &height, NULL);
    if(width > 1 && height > 1)
      wiki_update(context);

  } else {
    osm_gps_map_remove_layer(context->priv->map, OSM_GPS_MAP_LAYER(context));
  }
}
