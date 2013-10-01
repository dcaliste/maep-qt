#include "osm-gps-map-qt.h"
#include "osm-gps-map.h"
#undef WITH_GTK
#include "misc.h"

#include <QWidget>
#include <QPainter>
#include <QImage>
#include <cairo.h>

#define GCONF_KEY_ZOOM       "zoom"
#define GCONF_KEY_SOURCE     "source"
#define GCONF_KEY_LATITUDE   "latitude"
#define GCONF_KEY_LONGITUDE  "longitude"
#define GCONF_KEY_DOUBLEPIX  "double-pixel"

#define MAP_SOURCE  OSM_GPS_MAP_SOURCE_OPENCYCLEMAP

static void osm_gps_map_qt_repaint(Maep::GpsMap *widget, OsmGpsMap *map);

Maep::GpsMap::GpsMap(QQuickItem *parent)
    : QQuickPaintedItem(parent)
{
  char *path;

  const char *p = getenv("HOME");
  gint source = gconf_get_int(GCONF_KEY_SOURCE, MAP_SOURCE);
  gint zoom = gconf_get_int(GCONF_KEY_ZOOM, 3);
  gfloat lat = gconf_get_float(GCONF_KEY_LATITUDE, 50.0);
  gfloat lon = gconf_get_float(GCONF_KEY_LONGITUDE, 21.0);
  gboolean dpix = gconf_get_bool(GCONF_KEY_DOUBLEPIX, FALSE);

  if(!p) p = "/tmp"; 
  path = g_strdup_printf("%s/.osm-gps-map", p);

    map = OSM_GPS_MAP(g_object_new(OSM_TYPE_GPS_MAP,
		 "map-source",               source,
		 "tile-cache",               OSM_GPS_MAP_CACHE_FRIENDLY,
		 "tile-cache-base",          path,
		 "auto-center",              FALSE,
		 "record-trip-history",      FALSE, 
		 "show-trip-history",        FALSE, 
		 "gps-track-point-radius",   10,
		 // proxy?"proxy-uri":NULL,     proxy,
		 "double-pixel",             dpix,
                                   NULL));
    osm_gps_map_set_mapcenter(map, lat, lon, zoom);

    g_signal_connect_swapped(G_OBJECT(map), "dirty",
                             G_CALLBACK(osm_gps_map_qt_repaint), this);

    g_free(path);

    drag_start_mouse_x = 0;
    drag_start_mouse_y = 0;
    drag_mouse_dx = 0;
    drag_mouse_dy = 0;

    forceActiveFocus();
    setAcceptedMouseButtons(Qt::LeftButton);
}
Maep::GpsMap::~GpsMap()
{
  gint zoom, source;
  gfloat lat, lon;
  gboolean dpix;

  /* get state information from map ... */
  g_object_get(map, 
	       "zoom", &zoom, 
	       "map-source", &source, 
	       "latitude", &lat, "longitude", &lon,
	       "double-pixel", &dpix,
	       NULL);

  /* ... and store it in gconf */
  gconf_set_int(GCONF_KEY_ZOOM, zoom);
  gconf_set_int(GCONF_KEY_SOURCE, source);
  gconf_set_float(GCONF_KEY_LATITUDE, lat);
  gconf_set_float(GCONF_KEY_LONGITUDE, lon);
  gconf_set_bool(GCONF_KEY_DOUBLEPIX, dpix);

  g_object_unref(map);
}

static void osm_gps_map_qt_repaint(Maep::GpsMap *widget, OsmGpsMap *map)
{
  widget->update();
}

void Maep::GpsMap::paint(QPainter *painter)
{
    QImage *img;
    cairo_surface_t *surf;

    g_message("repainting %fx%f!", width(), height());

    osm_gps_map_set_viewport(map, width(), height());
    surf = osm_gps_map_get_surface(map);
    img = new QImage(cairo_image_surface_get_data(surf),
                     cairo_image_surface_get_width(surf),
                     cairo_image_surface_get_height(surf),
                     QImage::Format_ARGB32);
    QRectF target(drag_mouse_dx - EXTRA_BORDER,
                  drag_mouse_dy - EXTRA_BORDER,
                  cairo_image_surface_get_width(surf),
                  cairo_image_surface_get_height(surf));
    QRectF source(0, 0, cairo_image_surface_get_width(surf),
                  cairo_image_surface_get_height(surf));
    painter->drawImage(target, *img, source);
    cairo_surface_destroy(surf);
}

#define OSM_GPS_MAP_SCROLL_STEP     (10)

void Maep::GpsMap::keyPressEvent(QKeyEvent * event)
{
  int step;

  step = width() / OSM_GPS_MAP_SCROLL_STEP;

  if (event->key() == Qt::Key_Up)
    osm_gps_map_scroll(map, 0, -step);
  else if (event->key() == Qt::Key_Down)
    osm_gps_map_scroll(map, 0, step);
  else if (event->key() == Qt::Key_Right)
    osm_gps_map_scroll(map, step, 0);
  else if (event->key() == Qt::Key_Left)
    osm_gps_map_scroll(map, -step, 0);
  else if (event->key() == Qt::Key_Plus)
    osm_gps_map_zoom_in(map);
  else if (event->key() == Qt::Key_Minus)
    osm_gps_map_zoom_out(map);
}

void Maep::GpsMap::mousePressEvent(QMouseEvent *event)
{
  drag_start_mouse_x = event->x();
  drag_start_mouse_y = event->y();
}
void Maep::GpsMap::mouseReleaseEvent(QMouseEvent *event)
{
  osm_gps_map_scroll(map, -drag_mouse_dx, -drag_mouse_dy);
  drag_mouse_dx = 0;
  drag_mouse_dy = 0;
}
void Maep::GpsMap::mouseMoveEvent(QMouseEvent *event)
{
  drag_mouse_dx = event->x() - drag_start_mouse_x;
  drag_mouse_dy = event->y() - drag_start_mouse_y;
  update();
}
