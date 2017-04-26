/*
 * osm-gps-map-qt.cpp
 * Copyright (C) Damien Caliste 2013-2017 <dcaliste@free.fr>
 *
 * osm-gps-map-qt.cpp is free software: you can redistribute it and/or modify it
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

#include <glib/gstdio.h>

#include "osm-gps-map-qt.h"
#include "osm-gps-map.h"
#include "osm-gps-map-layer.h"
#undef WITH_GTK
#include "../net_io.h"

#include <QPainter>
#include <QPainterPath>
#include <QDebug>
#include <cmath>
#include <algorithm>

#define MAEP_CONF_KEY_ZOOM       "zoom"
#define MAEP_CONF_KEY_SOURCE     "source"
#define MAEP_CONF_KEY_OVERLAY_SOURCE "overlay-source"
#define MAEP_CONF_KEY_LATITUDE   "latitude"
#define MAEP_CONF_KEY_LONGITUDE  "longitude"
#define MAEP_CONF_KEY_DOUBLEPIX  "double-pixel"
#define MAEP_CONF_KEY_WIKIPEDIA  "wikipedia"
#define MAEP_CONF_KEY_TRACK_CAPTURE "track_capture_enabled"
#define MAEP_CONF_KEY_TRACK_PATH "track_path"
#define MAEP_CONF_KEY_TRACK_WIDTH "track_width"
#define MAEP_CONF_KEY_TRACK_COLOR "track_color"
#define MAEP_CONF_KEY_SCREEN_ROTATE "screen-rotate"
#define MAEP_CONF_KEY_GPS_REFRESH_RATE "gps-refresh-rate"
#define MAEP_CONF_KEY_COMPASS_ENABLED "compass-enabled"
#define MAEP_CONF_KEY_COMPASS_MODE "compass-mode"

QString Maep::GeonamesPlace::coordinateToString(QGeoCoordinate::CoordinateFormat format) const
{
  return m_coordinate.toString(format);
}
QString Maep::GeonamesEntry::coordinateToString(QGeoCoordinate::CoordinateFormat format) const
{
  return m_coordinate.toString(format);
}

void Maep::Track::set(MaepGeodata *t)
{
  if (!t)
    return;

  g_object_unref(G_OBJECT(track));
  g_object_ref(G_OBJECT(t));
  track = t;
}
bool Maep::Track::set(const QString &filename)
{
  MaepGeodata *t;
  GError *error;

  error = NULL;
  t = maep_geodata_new_from_file(filename.toLocal8Bit().data(), &error);
  if (t)
    {
      set(t);
      source = filename;
      maep_geodata_set_autosave_path(track, source.toLocal8Bit().data());
      emit pathChanged();
      return true;
    }
  if (error)
    {
      emit fileError(QString(error->message));
      g_error_free(error);
    }
  return false;
}
bool Maep::Track::setFromBackup(void)
{
  char *path;
  bool res;

  path = maep_geodata_get_default_autosave_path();
  res = set(path);
  g_free(path);
  source.clear();
  emit pathChanged();

  return res;
}
bool Maep::Track::toFile(const QString &filename)
{
  GError *error;
  bool res;

  error = NULL;
  res = maep_geodata_to_file(track, filename.toLocal8Bit().data(), &error);
  if (error)
    {
      emit fileError(QString(error->message));
      g_error_free(error);
    }
  else
    {
      source = filename;
      maep_geodata_set_autosave_path(track, source.toLocal8Bit().data());
      emit pathChanged();
    }
  return res;
}
void Maep::Track::addPoint(QGeoPositionInfo &info)
{
  QGeoCoordinate coord = info.coordinate();
  qreal speed, h_acc;

#ifndef NAN
#define NAN (0.0/0.0)
#endif

  speed = NAN;
  if (info.hasAttribute(QGeoPositionInfo::GroundSpeed))
    speed = info.attribute(QGeoPositionInfo::GroundSpeed);
  h_acc = G_MAXFLOAT;
  if (info.hasAttribute(QGeoPositionInfo::HorizontalAccuracy))
    h_acc = info.attribute(QGeoPositionInfo::HorizontalAccuracy);

  maep_geodata_add_trackpoint(track, coord.latitude(), coord.longitude(), h_acc,
                              coord.altitude(), speed, NAN, NAN);

  emit characteristicsChanged((qreal)maep_geodata_track_get_metric_length(track),
                              (unsigned int)maep_geodata_track_get_duration(track));
}
void Maep::Track::finalizeSegment()
{
  if (track)
    maep_geodata_track_finalize_segment(track);
}
bool Maep::Track::setAutosavePeriod(unsigned int value)
{
  bool ret;

  autosavePeriod = value;
  ret = maep_geodata_set_autosave_period(track, (guint)value);
  if (ret)
    emit autosavePeriodChanged(value);

  return ret;
}
bool Maep::Track::setMetricAccuracy(qreal value)
{
  bool ret;

  ret = maep_geodata_track_set_metric_accuracy(track, (value <= 0)?G_MAXFLOAT:(gfloat)value);
  if (ret)
    {
      emit metricAccuracyChanged(value);
      emit characteristicsChanged((qreal)maep_geodata_track_get_metric_length(track),
                                  (unsigned int)maep_geodata_track_get_duration(track));
    }

  return ret;
}
void Maep::Track::addWayPoint(const QGeoCoordinate &coord, const QString &name,
                              const QString &comment, const QString &description)
{
  maep_geodata_add_waypoint(track, coord.latitude(), coord.longitude(),
                            name.toLocal8Bit().data(),
                            comment.toLocal8Bit().data(),
                            description.toLocal8Bit().data());
}
void Maep::Track::highlightWayPoint(int iwpt)
{
  maep_geodata_waypoint_set_highlight(track, iwpt);
}

namespace Maep {
  struct GpsMapCClosures
  {
    static void repaint_from_map(Maep::GpsMap *widget)
    {
      widget->drag_map_dx = 0;
      widget->drag_map_dy = 0;
      widget->mapUpdate();
      widget->update();
    }
    static void repaint(Maep::GpsMap *widget)
    {
      widget->mapUpdate();
      widget->update();
    }
  };
};
static void osm_gps_map_qt_coordinate(Maep::GpsMap *widget, GParamSpec *pspec, OsmGpsMap *map);
static void osm_gps_map_qt_double_pixel(Maep::GpsMap *widget, GParamSpec *pspec, OsmGpsMap *map);
static void osm_gps_map_qt_auto_center(Maep::GpsMap *widget, GParamSpec *pspec, OsmGpsMap *map);
static void osm_gps_map_qt_source(Maep::GpsMap *widget);
static void osm_gps_map_qt_overlay_source(Maep::GpsMap *widget);
static void osm_gps_map_qt_wiki(Maep::GpsMap *widget, MaepGeonamesEntry *entry, MaepWikiContext *wiki);
static void osm_gps_map_qt_places(Maep::GpsMap *widget, MaepSearchContextSource source,
                                  GSList *places, MaepSearchContext *wiki);
static void osm_gps_map_qt_places_failure(Maep::GpsMap *widget,
                                          MaepSearchContextSource source,
                                          GError *error, MaepSearchContext *wiki);
static void osm_gps_map_qt_track_width(Maep::GpsMap *widget, GParamSpec *pspec, OsmGpsMap *map);
static void osm_gps_map_qt_track_color(Maep::GpsMap *widget, GParamSpec *pspec, OsmGpsMap *map);

Maep::GpsMap::GpsMap(QQuickItem *parent)
    : QQuickPaintedItem(parent)
    , compass(parent)
    , compassMode_(COMPASS_MODE_OFF)
{
  gchar *path, *oldPath, *cache_dir;
  gdouble color[4];
  gdouble defaultColor[4] = {0.0, 0.0, 0.0, 0.0};
  OsmColor_t trackColor;
  const MaepSource *source;

  gint sourceId = maep_conf_get_int(MAEP_CONF_KEY_SOURCE, MAEP_SOURCE_OPENSTREETMAP);
  gint overlaySourceId = maep_conf_get_int(MAEP_CONF_KEY_OVERLAY_SOURCE, MAEP_SOURCE_NULL);
  gint zoom = maep_conf_get_int(MAEP_CONF_KEY_ZOOM, 3);
  gfloat lat = maep_conf_get_float(MAEP_CONF_KEY_LATITUDE, 50.0);
  gfloat lon = maep_conf_get_float(MAEP_CONF_KEY_LONGITUDE, 21.0);
  gboolean dpix = maep_conf_get_bool(MAEP_CONF_KEY_DOUBLEPIX, FALSE);
  bool wikipedia = maep_conf_get_bool(MAEP_CONF_KEY_WIKIPEDIA, FALSE);
  bool track = maep_conf_get_bool(MAEP_CONF_KEY_TRACK_CAPTURE, FALSE);
  gint width = maep_conf_get_int(MAEP_CONF_KEY_TRACK_WIDTH, -1);
  bool orientation = maep_conf_get_bool(MAEP_CONF_KEY_SCREEN_ROTATE, TRUE);
  int gpsRefresh = maep_conf_get_int(MAEP_CONF_KEY_GPS_REFRESH_RATE, 1000);
  bool compassEnabled = maep_conf_get_bool(MAEP_CONF_KEY_COMPASS_ENABLED, FALSE);
  gint compassMode = maep_conf_get_int(MAEP_CONF_KEY_COMPASS_MODE,
                                   // backward compatibility with old boolean setting
                                   compassEnabled ? COMPASS_MODE_NORTH : COMPASS_MODE_OFF);

  compassMode = std::min(static_cast<gint>(COMPASS_N_MODES) - 1,
                         std::max(static_cast<gint>(COMPASS_MODE_OFF), compassMode));

  maep_conf_get_color(MAEP_CONF_KEY_TRACK_COLOR, color, defaultColor);
  path = g_build_filename(g_get_user_cache_dir(), APP, NULL);
  cache_dir = g_strdup_printf("%s%s", MAEP_SOURCE_MANAGER_CACHE_FRIENDLY, path);

  /* Backward compatibility, move old path. */
  oldPath = g_build_filename(g_get_user_data_dir(), "maep", NULL);
  if (g_file_test(oldPath, G_FILE_TEST_IS_DIR)
      && !g_file_test(path, G_FILE_TEST_IS_DIR))
    g_rename(oldPath, path);
  g_free(oldPath);
  g_free(path);

  sources = maep_source_manager_get_instance();
  maep_source_manager_set_cache_dir(sources, cache_dir);
  g_free(cache_dir);
  source = maep_source_manager_getById(sources, sourceId);

  screenRotation = orientation;
  map = OSM_GPS_MAP(g_object_new(OSM_TYPE_GPS_MAP,
                                 "map-source",               source,
                                 "auto-center",              FALSE,
                                 "record-trip-history",      FALSE, 
                                 "show-trip-history",        FALSE, 
                                 "gps-track-point-radius",   10,
                                 // proxy?"proxy-uri":NULL,     proxy,
                                 "double-pixel",             dpix,
                                 NULL));

  if (color[3] > 0.) {
      trackColor.red   = color[0];
      trackColor.green = color[1];
      trackColor.blue  = color[2];
      trackColor.alpha = color[3];
      g_object_set(G_OBJECT(map), "gps-track-color", &trackColor, NULL);
  }
  if (width > 0) {
      g_object_set(G_OBJECT(map), "gps-track-width", width, NULL);
  }

  osm_gps_map_set_mapcenter(map, lat, lon, zoom);
  coordinate = QGeoCoordinate(lat, lon);

  g_signal_connect_swapped(G_OBJECT(map), "dirty",
                           G_CALLBACK(Maep::GpsMapCClosures::repaint_from_map), this);
  g_signal_connect_swapped(G_OBJECT(map), "notify::latitude",
                           G_CALLBACK(osm_gps_map_qt_coordinate), this);
  g_signal_connect_swapped(G_OBJECT(map), "notify::auto-center",
                           G_CALLBACK(osm_gps_map_qt_auto_center), this);
  g_signal_connect_swapped(G_OBJECT(map), "notify::double-pixel",
                           G_CALLBACK(osm_gps_map_qt_double_pixel), this);
  g_signal_connect_swapped(G_OBJECT(map), "notify::map-source",
                           G_CALLBACK(osm_gps_map_qt_source), this);
  g_signal_connect_swapped(G_OBJECT(map), "notify::gps-track-width",
                           G_CALLBACK(osm_gps_map_qt_track_width), this);
  g_signal_connect_swapped(G_OBJECT(map), "notify::gps-track-color",
                           G_CALLBACK(osm_gps_map_qt_track_color), this);

  overlay = NULL;
  if (overlaySourceId != MAEP_SOURCE_NULL)
      ensureOverlay(maep_source_manager_getById(sources, overlaySourceId));

  net_io_init();
  osd = osm_gps_map_osd_classic_init(map);
  wiki = maep_wiki_context_new();
  wiki_enabled = wikipedia;
  if (wiki_enabled)
    maep_wiki_context_enable(wiki, map);
  wiki_entry = NULL;
  g_signal_connect_swapped(G_OBJECT(wiki), "entry-selected",
                           G_CALLBACK(osm_gps_map_qt_wiki), this);
  g_signal_connect_swapped(G_OBJECT(wiki), "dirty",
                           G_CALLBACK(Maep::GpsMapCClosures::repaint), this);
  search = maep_search_context_new();
  g_signal_connect_swapped(G_OBJECT(search), "places-available",
                           G_CALLBACK(osm_gps_map_qt_places), this);
  g_signal_connect_swapped(G_OBJECT(search), "download-error",
                           G_CALLBACK(osm_gps_map_qt_places_failure), this);

  drag_mouse_dx = 0;
  drag_mouse_dy = 0;
  drag_map_dx = 0;
  drag_map_dy = 0;

  surf = NULL;
  cr = NULL;
  // pat = NULL;
  white = new QColor(255, 255, 255);
  img = NULL;

  forceActiveFocus();
  setAcceptedMouseButtons(Qt::LeftButton);

  gpsRefreshRate_ = gpsRefresh;
  gps = QGeoPositionInfoSource::createDefaultSource(this);
  if (gps)
    {
      connect(gps, SIGNAL(positionUpdated(QGeoPositionInfo)),
              this, SLOT(positionUpdate(QGeoPositionInfo)));
      connect(gps, SIGNAL(updateTimeout()),
              this, SLOT(positionLost()));
      g_message("Start gps with rate at %d", gpsRefreshRate_);
      if (gpsRefreshRate_ > 0)
        {
          gps->setUpdateInterval(gpsRefreshRate_);
          gps->startUpdates();
        }
    }
  else
    g_message("no gps source...");
  lastGps = QGeoPositionInfo();
  lgps = maep_layer_gps_new();
  g_signal_connect_swapped(G_OBJECT(lgps), "dirty",
                           G_CALLBACK(Maep::GpsMapCClosures::repaint), this);

  maep_layer_gps_set_azimuth(lgps, NAN);
  connect(&compass, SIGNAL(readingChanged()), this, SLOT(compassReadingChanged()));
  setCompassMode(static_cast<CompassMode>(compassMode));

  track_capture = track;
  track_current = NULL;
}
Maep::GpsMap::~GpsMap()
{
  gint zoom, sourceId, overlaySourceId, width;
  gfloat lat, lon;
  gboolean dpix;
  OsmColor_t *color;
  gdouble track_color[4];
  MaepSource *source;

  /* get state information from map ... */
  overlaySourceId = MAEP_SOURCE_NULL;
  if (overlay)
    {
        /* Retrieve it to store it in dconf later. */
        g_object_get(overlay, "map-source", &source, NULL);
        g_object_unref(overlay);
        overlaySourceId = source ? maep_source_get_id(source) : int(MAEP_SOURCE_NULL);
        g_boxed_free(MAEP_TYPE_SOURCE, source);
    }
  overlay = NULL;

  compass.stop();

  g_object_get(map, 
	       "zoom", &zoom, 
	       "map-source", &source, 
	       "latitude", &lat, "longitude", &lon,
	       "double-pixel", &dpix,
               "gps-track-color", &color, "gps-track-width", &width,
	       NULL);
  sourceId = source ? maep_source_get_id(source) : int(MAEP_SOURCE_NULL);
  g_boxed_free(MAEP_TYPE_SOURCE, source);
  track_color[0] = color->red;
  track_color[1] = color->green;
  track_color[2] = color->blue;
  track_color[3] = color->alpha;
  g_boxed_free(OSM_TYPE_COLOR, color);
  osm_gps_map_osd_classic_free(osd);
  osd = NULL;
  maep_wiki_context_enable(wiki, NULL);
  g_object_unref(wiki);
  if (wiki_entry)
    delete(wiki_entry);
  g_object_unref(search);
  net_io_finalize();

  if (surf)
    cairo_surface_destroy(surf);
  if (cr)
    cairo_destroy(cr);
  if (img)
    delete(img);
  delete(white);
  if (gps)
    delete(gps);
  if (track_current && track_current->parent() == this)
    delete(track_current);
  g_object_unref(lgps);

  g_object_unref(map);

  /* ... and store it in dconf */
  g_message("Storing configuration.");
  maep_conf_set_int(MAEP_CONF_KEY_ZOOM, zoom);
  maep_conf_set_int(MAEP_CONF_KEY_SOURCE, sourceId);
  maep_conf_set_int(MAEP_CONF_KEY_OVERLAY_SOURCE, overlaySourceId);
  maep_conf_set_float(MAEP_CONF_KEY_LATITUDE, lat);
  maep_conf_set_float(MAEP_CONF_KEY_LONGITUDE, lon);
  maep_conf_set_bool(MAEP_CONF_KEY_DOUBLEPIX, dpix);

  maep_conf_set_bool(MAEP_CONF_KEY_WIKIPEDIA, wiki_enabled);

  maep_conf_set_bool(MAEP_CONF_KEY_TRACK_CAPTURE, track_capture);
  maep_conf_set_int(MAEP_CONF_KEY_TRACK_WIDTH, width);
  maep_conf_set_color(MAEP_CONF_KEY_TRACK_COLOR, track_color);

  maep_conf_set_bool(MAEP_CONF_KEY_SCREEN_ROTATE, screenRotation);

  maep_conf_set_int(MAEP_CONF_KEY_GPS_REFRESH_RATE, gpsRefreshRate_);

  maep_conf_unset_key(MAEP_CONF_KEY_COMPASS_ENABLED);
  maep_conf_set_int(MAEP_CONF_KEY_COMPASS_MODE, compassMode_);
  g_message("Storing configuration done.");
}
static void onLatLon(GObject *map, GParamSpec *pspec, OsmGpsMap *overlay)
{
  Q_UNUSED(pspec);
  /* get current map position */
  gfloat lat, lon;
  g_object_get(map, "latitude", &lat, "longitude", &lon, NULL);
  
  osm_gps_map_set_center(overlay, lat, lon);
}
void Maep::GpsMap::ensureOverlay(const MaepSource *source)
{
  if (overlay)
    return;

  g_message("Creating overlay %s", maep_source_get_friendly_name(source));
  overlay = OSM_GPS_MAP(g_object_new(OSM_TYPE_GPS_MAP,
                                     "map-source",               source,
                                     "auto-center",              FALSE,
                                     "record-trip-history",      FALSE, 
                                     "show-trip-history",        FALSE, 
                                     // proxy?"proxy-uri":NULL,     proxy,
                                     NULL));

  g_object_bind_property(G_OBJECT(map), "zoom", G_OBJECT(overlay), "zoom",
                         (GBindingFlags)(G_BINDING_DEFAULT | G_BINDING_SYNC_CREATE));
  g_object_bind_property(G_OBJECT(map), "factor", G_OBJECT(overlay), "factor",
                         (GBindingFlags)(G_BINDING_DEFAULT | G_BINDING_SYNC_CREATE));
  g_object_bind_property(G_OBJECT(map), "double-pixel",
                         G_OBJECT(overlay), "double-pixel",
                         (GBindingFlags)(G_BINDING_DEFAULT | G_BINDING_SYNC_CREATE));
  g_object_bind_property(G_OBJECT(map), "viewport-width",
                         G_OBJECT(overlay), "viewport-width",
                         (GBindingFlags)(G_BINDING_DEFAULT | G_BINDING_SYNC_CREATE));
  g_object_bind_property(G_OBJECT(map), "viewport-height",
                         G_OBJECT(overlay), "viewport-height",
                         (GBindingFlags)(G_BINDING_DEFAULT | G_BINDING_SYNC_CREATE));
  /* Workaround to bind lat and lon together. */
  g_signal_connect_object(G_OBJECT(map), "notify::latitude",
                          G_CALLBACK(onLatLon), (gpointer)overlay, (GConnectFlags)0);
  onLatLon(G_OBJECT(map), NULL, overlay);

  g_signal_connect_swapped(G_OBJECT(overlay), "dirty",
                           G_CALLBACK(Maep::GpsMapCClosures::repaint), this);
  g_signal_connect_swapped(G_OBJECT(overlay), "notify::map-source",
                           G_CALLBACK(osm_gps_map_qt_overlay_source), this);
}

bool Maep::GpsMap::mapSized()
{
  if (width() < 1 || height() < 1)
    return false;

  if (surf && (cairo_image_surface_get_width(surf) != width() ||
               cairo_image_surface_get_height(surf) < height()))
    {
      cairo_surface_destroy(surf);
      cairo_destroy(cr);
      // cairo_pattern_destroy(pat);
      delete(img);
      surf = NULL;
    }

  if (!surf)
    {
      surf = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, width(), height());
      cr = cairo_create(surf);
      // pat = cairo_pattern_create_linear (0.0, height(),
      //                                    0.0, height() - 100);
      // cairo_pattern_add_color_stop_rgba (pat, 1, 0, 0, 0, 1);
      // cairo_pattern_add_color_stop_rgba (pat, 0, 0, 0, 0, 0.3);
      img = new QImage(cairo_image_surface_get_data(surf),
                       cairo_image_surface_get_width(surf),
                       cairo_image_surface_get_height(surf),
                       QImage::Format_ARGB32);
      osm_gps_map_set_viewport(map, width(), height());
      return true;
    }
  return false;
}

void Maep::GpsMap::mapUpdate()
{
  if (!cr)
    return;

  cairo_set_operator (cr, CAIRO_OPERATOR_CLEAR);
  cairo_paint(cr);

  cairo_save(cr);
  cairo_translate(cr, drag_map_dx, drag_map_dy);
  // g_message("update at drag %dx%d %g", drag_mouse_dx, drag_mouse_dy, 1.f / factor);
  osm_gps_map_blit(map, cr, CAIRO_OPERATOR_SOURCE);
  if (overlay && overlaySource() != Maep::GpsMap::SOURCE_NULL)
    osm_gps_map_blit(overlay, cr, CAIRO_OPERATOR_OVER);
  cairo_restore(cr);

  cairo_save(cr);
  cairo_translate(cr, drag_mouse_dx, drag_mouse_dy);
  osm_gps_map_layer_draw(OSM_GPS_MAP_LAYER(lgps), cr, map);
  if (wiki_enabled)
    osm_gps_map_layer_draw(OSM_GPS_MAP_LAYER(wiki), cr, map);
  cairo_restore(cr);

#ifdef ENABLE_OSD
  if (osd)
    osd->draw(osd, cr);
#endif

  // w = cairo_image_surface_get_width(surf);
  //h = cairo_image_surface_get_height(surf);

  /* Remove the bottom part for a Sailfish toolbar. */
  // cairo_save(cr);
  // cairo_rectangle (cr, 0., h - 100., w, h);
  // cairo_clip(cr);
  // cairo_set_operator (cr, CAIRO_OPERATOR_DEST_IN);
  // cairo_set_source (cr, pat);
  // cairo_paint(cr);
  // cairo_restore(cr);
  /* Make top rounded corners. */
  // cairo_set_operator (cr, CAIRO_OPERATOR_CLEAR);
  // cairo_move_to(cr, 0., 0.);
  // cairo_line_to(cr, 20., 0.);
  // cairo_arc_negative(cr, 20., 20., 20., 1.5 * G_PI, G_PI);
  // cairo_line_to(cr, 0., 0.);
  // cairo_set_source_rgb (cr, 1., 1., 1.);
  // cairo_fill(cr);

  // cairo_move_to(cr, w, 0.);
  // cairo_line_to(cr, w - 20., 0.);
  // cairo_arc(cr, w - 20., 20., 20., 1.5 * G_PI, 2. * G_PI);
  // cairo_line_to(cr, w, 0.);
  // cairo_set_source_rgb (cr, 1., 1., 1.);
  // cairo_fill(cr);

  emit mapChanged();
}

void Maep::GpsMap::paintTo(QPainter *painter, int width, int height)
{
  int w, h;

  if (!img || !surf)
    return;

  w = cairo_image_surface_get_width(surf);
  h = cairo_image_surface_get_height(surf);
  // g_message("Paint to %dx%d from %fx%f - %fx%f.", width, height,
  //           (w - width) * 0.5, (h - height) * 0.5,
  //           (w + width) * 0.5, (h + height) * 0.5);
  QRectF target(0, 0, width, height);
  QRectF source((w - width) * 0.5, (h - height) * 0.5,
                width, height);
  painter->drawImage(target, *img, source);
}

void Maep::GpsMap::paint(QPainter *painter)
{
  int w;
  QPainterPath path;

  if (mapSized())
    mapUpdate();
  paintTo(painter, width(), height());
  /* Make top rounded corners. */
  painter->setCompositionMode(QPainter::CompositionMode_Clear);
  w = width();

  path = QPainterPath();
  path.moveTo(0., 0.);
  path.lineTo(20., 0.);
  path.arcTo(0., 0., 40., 40., 90., 90.);
  path.lineTo(0., 0.);

  path.moveTo(w, 0.);
  path.lineTo(w - 20., 0.);
  path.arcTo(w - 40., 0., 40., 40., 90., -90.);
  path.lineTo(w, 0.);

  painter->fillPath(path, *white);
}

void Maep::GpsMap::zoomIn()
{
  gfloat factor;

  g_object_get(G_OBJECT(map), "factor", &factor, NULL);
  osm_gps_map_set_factor(map, 1.f);
  if (factor >= 1.)
    osm_gps_map_zoom_in(map);
}
void Maep::GpsMap::zoomOut()
{
  gfloat factor;

  g_object_get(G_OBJECT(map), "factor", &factor, NULL);
  osm_gps_map_set_factor(map, 1.f);
  if (factor <= 1.)
    osm_gps_map_zoom_out(map);
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
  else if (event->key() == Qt::Key_S)
    emit searchRequest();
}

// void Maep::GpsMap::mousePressEvent(QMouseEvent *event)
// {
// // #ifdef ENABLE_OSD
// //   int step;
// //   osd_button_t but = 
// //     osd->check(osd, TRUE, event->x(), event->y());
  
// //   dragging = FALSE;

// //   step = width() / OSM_GPS_MAP_SCROLL_STEP;

// //   if(but != OSD_NONE)
// //     switch(but)
// //       {
// //       case OSD_UP:
// //         osm_gps_map_scroll(map, 0, -step);
// //         g_object_set(G_OBJECT(map), "auto-center", FALSE, NULL);
// //         return;

// //       case OSD_DOWN:
// //         osm_gps_map_scroll(map, 0, +step);
// //         g_object_set(G_OBJECT(map), "auto-center", FALSE, NULL);
// //         return;

// //       case OSD_LEFT:
// //         osm_gps_map_scroll(map, -step, 0);
// //         g_object_set(G_OBJECT(map), "auto-center", FALSE, NULL);
// //         return;
                
// //       case OSD_RIGHT:
// //         osm_gps_map_scroll(map, +step, 0);
// //         g_object_set(G_OBJECT(map), "auto-center", FALSE, NULL);
// //         return;
                
// //       case OSD_IN:
// //         osm_gps_map_zoom_in(map);
// //         return;
                
// //       case OSD_OUT:
// //         osm_gps_map_zoom_out(map);
// //         return;

// //       case OSD_GPS:
// //         if (lastGps.isValid())
// //           {
// //             osm_gps_map_set_center(map, lastGps.coordinate().latitude(),
// //                                    lastGps.coordinate().longitude());

// //             g_object_set(map, "auto-center", TRUE, NULL);
// //           }
// //         return;
                
// //       default:
// //         g_warning("Hey don't know what to do!");
// //         /* all custom buttons are forwarded to the application */
// //         // if(osd->cb)
// //         //   osd->cb(but, osd->data);
// //         return;
// //       }
// // #endif
//   g_message("press me");
//   if (osm_gps_map_layer_button(OSM_GPS_MAP_LAYER(wiki),
//                                event->x(), event->y(), TRUE))
//     return;

//   dragging = TRUE;
//   drag_start_mouse_x = event->x();
//   drag_start_mouse_y = event->y();
// }
// void Maep::GpsMap::mouseReleaseEvent(QMouseEvent *event)
// {
//   g_message("release me %d", dragging);
//   if (dragging)
//     {
//       dragging = FALSE;
//       osm_gps_map_scroll(map, -drag_mouse_dx, -drag_mouse_dy);
//       drag_mouse_dx = 0;
//       drag_mouse_dy = 0;
//       g_object_set(map, "auto-center", FALSE, NULL);
//     }
//   osm_gps_map_layer_button(OSM_GPS_MAP_LAYER(wiki),
//                            event->x(), event->y(), FALSE);
// }
// void Maep::GpsMap::mouseMoveEvent(QMouseEvent *event)
// {
//   g_message("dragging %d", dragging);
//   if (dragging)
//     {
//       drag_mouse_dx = event->x() - drag_start_mouse_x;
//       drag_mouse_dy = event->y() - drag_start_mouse_y;
//       mapUpdate();
//       update();
//     }
// }
void Maep::GpsMap::touchEvent(QTouchEvent *touchEvent)
{
  qreal factor;
  int zoom;

  switch (touchEvent->type()) {
  case QEvent::TouchBegin:
    {
      QList<QTouchEvent::TouchPoint> touchPoints = touchEvent->touchPoints();
      // Drag/zoom if one or two finger and no wiki layer.
      dragging = (touchPoints.count() == 2 ||
                  (touchPoints.count() == 1 &&
                   !osm_gps_map_layer_button(OSM_GPS_MAP_LAYER(wiki),
                                             touchPoints.first().pos().x(),
                                             touchPoints.first().pos().y(), TRUE)));
      factor0 = 0.f;
      // g_message("touch begin %d", dragging);
      return;
    }
  case QEvent::TouchUpdate:
    {
      // g_message("touch update %d", haveMouseEvent);
      if (!dragging)
        return;
      QList<QTouchEvent::TouchPoint> touchPoints = touchEvent->touchPoints();
      if (touchPoints.count() == 2) {
        // Zoom and drag case
        const QTouchEvent::TouchPoint &touchPoint0 = touchPoints.first();
        const QTouchEvent::TouchPoint &touchPoint1 = touchPoints.last();
        factor =
          QLineF(touchPoint0.pos(), touchPoint1.pos()).length()
          / QLineF(touchPoint0.startPos(), touchPoint1.startPos()).length();
        if (factor0 == 0.f)
          factor0 = osm_gps_map_get_factor(map);
        osm_gps_map_set_factor(map, factor0 * factor);
        QPointF delta = (touchPoint0.pos() + touchPoint1.pos() -
                         touchPoint0.startPos() - touchPoint1.startPos()) * 0.5;
        drag_mouse_dx = drag_map_dx = delta.x();
        drag_mouse_dy = drag_map_dy = delta.y();
        mapUpdate();
        update();
      }
      else if (touchPoints.count() == 1) {
        // Drag case only
        const QTouchEvent::TouchPoint &touchPoint0 = touchPoints.first();
        QPointF delta = touchPoint0.pos() - touchPoint0.startPos();
        drag_mouse_dx = drag_map_dx = delta.x();
        drag_mouse_dy = drag_map_dy = delta.y();
        mapUpdate();
        update();

        factor0 = 0.f;
      }
      return;
    }
  case QEvent::TouchEnd:
    {
      QList<QTouchEvent::TouchPoint> touchPoints = touchEvent->touchPoints();
      // g_message("touch end %d", dragging);
      if (dragging)
        {
          dragging = FALSE;
          osm_gps_map_scroll(map, -drag_mouse_dx, -drag_mouse_dy);
          drag_mouse_dx = 0;
          drag_mouse_dy = 0;
          g_object_set(map, "auto-center", FALSE, NULL);

          // Adjust zoom and factor.
          factor = osm_gps_map_get_factor(map);
          if (factor >= 1.5) {
            g_object_get(map, "zoom", &zoom, NULL);
            if (osm_gps_map_zoom_in(map) != zoom)
              osm_gps_map_set_factor(map, factor / 2.);
          } else if (factor <= 0.66666666666666667) {
            g_object_get(map, "zoom", &zoom, NULL);
            if (osm_gps_map_zoom_out(map) != zoom)
              osm_gps_map_set_factor(map, factor * 2.);
          }
        }
      else if (touchPoints.count() == 1)
        osm_gps_map_layer_button(OSM_GPS_MAP_LAYER(wiki),
                                 touchPoints.first().pos().x(),
                                 touchPoints.first().pos().y(), FALSE);
    }
  default:
    QQuickItem::touchEvent(touchEvent);
    break;
  }
}

static void osm_gps_map_qt_source(Maep::GpsMap *widget)
{
  widget->sourceChanged(widget->source());
}
void Maep::GpsMap::setSource(int value)
{
  int orig;

  orig = source();
  if (orig == value)
    return;

  g_object_set(map, "map-source", maep_source_manager_getById(sources, value), NULL);
}
static void osm_gps_map_qt_overlay_source(Maep::GpsMap *widget)
{
  widget->overlaySourceChanged(widget->overlaySource());
}
void Maep::GpsMap::setOverlaySource(int value)
{
  int orig;
  const MaepSource *source;

  orig = overlaySource();
  if (orig == value)
    return;

  source = maep_source_manager_getById(sources, value);
  ensureOverlay(source);
  g_object_set(overlay, "map-source", source, NULL);

  // Overlay becoming NULL will never emit dirty, thus we redraw by hand
  if (!value)
    {
      mapUpdate();
      update();
    }
}

static void osm_gps_map_qt_double_pixel(Maep::GpsMap *widget,
                                        GParamSpec *pspec, OsmGpsMap *map)
{
  Q_UNUSED(pspec);
  Q_UNUSED(map);

  widget->doublePixelChanged(widget->doublePixel());
}
void Maep::GpsMap::setDoublePixel(bool status)
{
  gboolean orig;

  orig = doublePixel();
  if ((orig && status) || (!orig && !status))
    return;

  g_object_set(map, "double-pixel", status, NULL);
}
static void osm_gps_map_qt_auto_center(Maep::GpsMap *widget,
                                       GParamSpec *pspec, OsmGpsMap *map)
{
  Q_UNUSED(pspec);
  Q_UNUSED(map);

  widget->autoCenterChanged(widget->autoCenter());
}
void Maep::GpsMap::setAutoCenter(bool status)
{
  bool set;

  set = autoCenter();
  if ((set && status) || (!set && !status))
    return;

  if (status && lastGps.isValid())
    osm_gps_map_set_center(map, lastGps.coordinate().latitude(),
                           lastGps.coordinate().longitude());
  g_object_set(map, "auto-center", status, NULL);
}

void Maep::GpsMap::setScreenRotation(bool status)
{
  if (status == screenRotation)
    return;

  screenRotation = status;
  emit screenRotationChanged(status);
}

void Maep::GpsMap::setWikiStatus(bool status)
{
  if (status == wiki_enabled)
    return;

  wiki_enabled = status;
  emit wikiStatusChanged(wiki_enabled);

  maep_wiki_context_enable(wiki, (wiki_enabled)?map:NULL);
}

void Maep::GpsMap::setWikiEntry(const MaepGeonamesEntry *entry)
{
  if (wiki_entry)
    delete(wiki_entry);
  wiki_entry = new Maep::GeonamesEntry(entry);
  emit wikiEntryChanged();
}

static void osm_gps_map_qt_wiki(Maep::GpsMap *widget, MaepGeonamesEntry *entry, MaepWikiContext *wiki)
{
  Q_UNUSED(wiki);

  widget->setWikiEntry(entry);
}

void Maep::GpsMap::setSearchResults(MaepSearchContextSource source, GSList *places)
{
  g_message("hello got %d places", g_slist_length(places));

  if (searchRes.count() == 0 || source == MaepSearchContextGeonames)
    for (; places; places = places->next)
      {
        Maep::GeonamesPlace *place =
          new Maep::GeonamesPlace((const MaepGeonamesPlace*)places->data);
        searchRes.append(place);
      }
  else
    for (; places; places = places->next)
      {
        Maep::GeonamesPlace *place =
          new Maep::GeonamesPlace((const MaepGeonamesPlace*)places->data);
        searchRes.prepend(place);
      }
  searchFinished |= source;
  
  if (searchFinished == (MaepSearchContextGeonames | MaepSearchContextNominatim))
    {
      g_message("search finished !");
      emit searchResults();
    }
}
static void osm_gps_map_qt_places(Maep::GpsMap *widget, MaepSearchContextSource source,
                                  GSList *places, MaepSearchContext *wiki)
{
  Q_UNUSED(wiki);

  g_message("Got %d matching places.", g_slist_length(places));
  widget->setSearchResults(source, places);
}
static void osm_gps_map_qt_places_failure(Maep::GpsMap *widget,
                                          MaepSearchContextSource source,
                                          GError *error, MaepSearchContext *wiki)
{
  Q_UNUSED(wiki);

  g_message("Got download error '%s'.", error->message);
  widget->setSearchResults(source, NULL);
}

void Maep::GpsMap::setSearchRequest(const QString &request)
{
  qDeleteAll(searchRes);
  searchRes.clear();

  searchFinished = 0;
  maep_search_context_request(search, request.toLocal8Bit().data(),
                              MaepSearchContextGeonames |
                              MaepSearchContextNominatim);
}

void Maep::GpsMap::setLookAt(float lat, float lon)
{
  g_message("move to %fx%f", lat, lon);
  osm_gps_map_set_center(map, lat, lon);
  // We're not updating coordinate since the signal of map will do it.

  // QGeoCoordinate coord = QGeoCoordinate(lat, lon);
  // QGeoPositionInfo info = QGeoPositionInfo(coord, QDateTime::currentDateTime());
  // info.setAttribute(QGeoPositionInfo::HorizontalAccuracy, 2567.);
  // positionUpdate(info);
}
void Maep::GpsMap::setCoordinate(float lat, float lon)
{
  coordinate = QGeoCoordinate(lat, lon);
  emit coordinateChanged();
}
static void osm_gps_map_qt_coordinate(Maep::GpsMap *widget, GParamSpec *pspec, OsmGpsMap *map)
{
  Q_UNUSED(pspec);

  float lat, lon;

  g_object_get(G_OBJECT(map), "latitude", &lat, "longitude", &lon, NULL);
  widget->setCoordinate(lat, lon);
}
// QString Maep::GpsMap::orientationTo(QGeoCoordinate coord)
// {
// }

void Maep::GpsMap::positionUpdate(const QGeoPositionInfo &info)
{
  float track, hprec;

  // g_message("position is %f %f, heading %g, h_acc %g", info.coordinate().latitude(),
  //           info.coordinate().longitude(), track,
  //           info.attribute(QGeoPositionInfo::HorizontalAccuracy));

  // Redraw GPS position.
  maep_layer_gps_set_active(lgps, TRUE);
  hprec = 0.;
  if (info.hasAttribute(QGeoPositionInfo::HorizontalAccuracy))
    hprec = info.attribute(QGeoPositionInfo::HorizontalAccuracy);
  track = OSM_GPS_MAP_INVALID;
  if (info.hasAttribute(QGeoPositionInfo::Direction))
    track = info.attribute(QGeoPositionInfo::Direction);
  else if (lastGps.isValid())
    /* Approximate heading with last known position */
    track = lastGps.coordinate().azimuthTo(info.coordinate());
  maep_layer_gps_set_coordinates(lgps, info.coordinate().latitude(),
                                 info.coordinate().longitude(), hprec, track);
  // To generate auto-center if necessary.
  osm_gps_map_auto_center_at(map, info.coordinate().latitude(),
                      info.coordinate().longitude());

  lastGps = info;
  emit gpsCoordinateChanged();

  // Add track capture, if any.
  if (track_capture)
    gpsToTrack();
}
void Maep::GpsMap::unsetGps()
{
  lastGps = QGeoPositionInfo();
  emit gpsCoordinateChanged();

  maep_layer_gps_set_active(lgps, FALSE);
}
void Maep::GpsMap::compassReadingChanged()
{
  // Apparently this event fires spuriously once when the class is initialized, so double-check
  // if we're really activated.
  if (compassMode() != COMPASS_MODE_OFF && compass.isActive())
    {
      QCompassReading *compass_reading = compass.reading();
      if (compass_reading)
        {
          qreal azimuth = compass_reading->azimuth();
          if (lastAzimuth == -1. || std::abs(lastAzimuth - azimuth) > 2)
            {
              maep_layer_gps_set_azimuth(lgps, static_cast<gfloat>(azimuth));
              lastAzimuth = azimuth;
            }
        }
    }
}
void Maep::GpsMap::setCompassMode(CompassMode mode)
{
  if (compassMode_ == mode)
    return;
  
  if (mode == COMPASS_MODE_OFF)
    {
      qDebug() << "Disabling compass.";
      compass.stop();
    }
  else
    {
      if (compassMode_ == COMPASS_MODE_OFF)
        {
          qDebug() << "Enabling compass.";
          if (compass.isFeatureSupported(QCompass::SkipDuplicates))
            {
              qDebug() << "Enabling compass powersaving.";
              compass.setSkipDuplicates(true);
            }
          lastAzimuth = -1.;
          compass.start();
        }
    }
  compassMode_ = mode;
  maep_layer_gps_set_compass_mode(lgps, static_cast<MaepLayerCompassMode>(mode));
  emit compassModeChanged(mode);
}

void Maep::GpsMap::positionLost()
{
  g_message("loosing position");
  if (track_capture && track_current)
    track_current->finalizeSegment();
  unsetGps();
}
void Maep::GpsMap::setGpsRefreshRate(unsigned int rate)
{
  bool restart;

  if (gpsRefreshRate_ == rate)
    return;

  g_message("set GPS refresh rate to %d.", rate);
  restart = (gpsRefreshRate_ == 0);
  gpsRefreshRate_ = rate;
  emit gpsRefreshRateChanged(rate);

  if (rate == 0 && gps)
    {
      gps->stopUpdates();
      unsetGps();
    }
  else if (gps)
    {
      gps->setUpdateInterval(rate);
      if (restart)
        gps->startUpdates();
    }
}

static void osm_gps_map_qt_track_width(Maep::GpsMap *widget, GParamSpec *pspec, OsmGpsMap *map)
{
  Q_UNUSED(pspec);
  Q_UNUSED(map);

  emit widget->trackWidthChanged();
}
void Maep::GpsMap::setTrackWidth(unsigned int value)
{
  gint width;

  g_object_get(G_OBJECT(map), "gps-track-width", &width, NULL);
  if ((gint)value == width)
    return;

  g_object_set(G_OBJECT(map), "gps-track-width", value, NULL);
}
unsigned int Maep::GpsMap::getTrackWidth() const
{
  gint width;

  g_object_get(G_OBJECT(map), "gps-track-width", &width, NULL);
  return (unsigned int)width;
}

static void osm_gps_map_qt_track_color(Maep::GpsMap *widget, GParamSpec *pspec, OsmGpsMap *map)
{
  Q_UNUSED(pspec);
  Q_UNUSED(map);

  emit widget->trackColorChanged();
}
void Maep::GpsMap::setTrackColor(const QColor &value)
{
  OsmColor_t *color;

  g_object_get(G_OBJECT(map), "gps-track-color", &color, NULL);
  if (color->red   == value.redF() &&
      color->green == value.greenF() &&
      color->blue  == value.blueF() &&
      color->alpha == value.alphaF())
    {
      g_boxed_free(OSM_TYPE_COLOR, color);
      return;
    }

  color->red   = value.redF();
  color->green = value.greenF();
  color->blue  = value.blueF();
  color->alpha = value.alphaF();
  g_object_set(G_OBJECT(map), "gps-track-color", color, NULL);
  g_boxed_free(OSM_TYPE_COLOR, color);
}
QColor Maep::GpsMap::getTrackColor() const
{
  OsmColor_t *color;
  QColor out;

  g_object_get(G_OBJECT(map), "gps-track-color", &color, NULL);
  out.setRgbF(color->red, color->green, color->blue, color->alpha);
  g_boxed_free(OSM_TYPE_COLOR, color);
  return out;
}

void Maep::GpsMap::setTrackCapture(bool status)
{
  if (status == track_capture)
    return;

  track_capture = status;
  emit trackCaptureChanged(track_capture);
  
  if (status && lastGps.isValid())
    gpsToTrack();
}

void Maep::GpsMap::setTrack(Maep::Track *track)
{
  osm_gps_map_clear_tracks(map);

  g_message("Set track %p (track parent is %p)", track,
            (track)?(gpointer)track->parent():NULL);

  if (track_current && track_current->parent() == this)
    delete(track_current);

  if (track)
    {
      g_message("Reparenting.");
      track->setParent(this);

      if (!track->getPath().isEmpty())
        maep_conf_set_string(MAEP_CONF_KEY_TRACK_PATH, track->getPath().toLocal8Bit().data());
    }
  else if (track_capture && lastGps.isValid())
    {
      track = new Maep::Track();
      g_message("Regenerate a new track.");
      track->setParent(this);
      track->addPoint(lastGps);
    }
  track_current = track;

  emit trackChanged(track != NULL);

  if (track && track->get())
    {
      coord_t top_left, bottom_right;

      /* Set track for map. */
      osm_gps_map_add_track(map, track->get());

      /* Adjust map zoom and location according to track bounding box. */
      if (maep_geodata_get_bounding_box(track->get(), &top_left, &bottom_right))
        osm_gps_map_adjust_to(map, &top_left, &bottom_right);
    }
}
void Maep::GpsMap::gpsToTrack()
{
  if (!track_current)
    {
      Maep::Track *track = new Maep::Track();
      track->setParent(this);
      track->addPoint(lastGps);
      setTrack(track);
    }
  else
    track_current->addPoint(lastGps);
}
QString Maep::GpsMap::getCenteredTile(int source) const
{
  gchar *file, *uri;
  int zoom, x, y;
  QString out;

  const MaepSource *src;
  src = maep_source_manager_getById(sources, source);
  if (!src)
      return QString();

  osm_gps_map_get_tile_xy_at(map, coordinate.latitude(), coordinate.longitude(),
                             &zoom, &x, &y);

  file = maep_source_manager_get_cached_tile(sources, src, zoom, x, y);
  if (file) {
    // g_message("Get cached file for source %d: %s.", source, file);
    out = QString(file);
    g_free(file);
    return out;
  }
  
  uri = maep_source_get_tile_uri(src, zoom, x, y);
  // g_message("Get uri for source %d: %s.", source, uri);
  out = QString(uri);
  g_free(uri);
  return out;
}


/****************/
/* GpsMapCover. */
/****************/

Maep::GpsMapCover::GpsMapCover(QQuickItem *parent)
  : QQuickPaintedItem(parent)
{
  map_ = NULL;
  status_ = false;
}
Maep::GpsMapCover::~GpsMapCover()
{
  map_ = NULL;
}
Maep::GpsMap* Maep::GpsMapCover::map() const
{
  return map_;
}
void Maep::GpsMapCover::setMap(Maep::GpsMap *map)
{
  map_ = map;
  QObject::connect(map, &Maep::GpsMap::mapChanged,
                   this, &Maep::GpsMapCover::updateCover);
  emit mapChanged();
}
void Maep::GpsMapCover::updateCover()
{
  update();
}
bool Maep::GpsMapCover::status()
{
  return status_;
}
void Maep::GpsMapCover::setStatus(bool status)
{
  status_ = status;
  emit statusChanged();

  update();
}
void Maep::GpsMapCover::paint(QPainter *painter)
{
  if (!map_ || !status_)
    return;

  g_message("repainting cover %fx%f!", width(), height());
  map_->paintTo(painter, width(), height());
}
