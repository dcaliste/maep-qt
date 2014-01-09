/*
 * osm-gps-map-qt.cpp
 * Copyright (C) Damien Caliste 2013 <dcaliste@free.fr>
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

#include "osm-gps-map-qt.h"
#include "osm-gps-map.h"
#undef WITH_GTK
#include "misc.h"

#include <QPainter>

#define GCONF_KEY_ZOOM       "zoom"
#define GCONF_KEY_SOURCE     "source"
#define GCONF_KEY_LATITUDE   "latitude"
#define GCONF_KEY_LONGITUDE  "longitude"
#define GCONF_KEY_DOUBLEPIX  "double-pixel"
#define GCONF_KEY_WIKIPEDIA  "wikipedia"
#define GCONF_KEY_TRACK_CAPTURE "track_capture_enabled"
#define GCONF_KEY_TRACK_PATH "track_path"
#define GCONF_KEY_SCREEN_ROTATE "screen-rotate"

QString Maep::GeonamesPlace::coordinateToString(QGeoCoordinate::CoordinateFormat format) const
{
  return coordinate.toString(format);
}
QString Maep::GeonamesEntry::coordinateToString(QGeoCoordinate::CoordinateFormat format) const
{
  return coordinate.toString(format);
}

void Maep::Track::set(track_state_t *t)
{
  if (!t)
    return;

  track_state_unref(track);
  track_state_ref(t);
  track = t;
}
bool Maep::Track::set(const QString &filename)
{
  track_state_t *t;
  GError *error;

  error = NULL;
  t = track_read(filename.toLocal8Bit().data(), FALSE, &error);
  if (t)
    {
      set(t);
      source = filename;
      return true;
    }
  if (error)
    {
      emit fileError(QString(error->message));
      g_error_free(error);
    }
  return false;
}
bool Maep::Track::toFile(const QString &filename)
{
  GError *error;
  bool res;

  error = NULL;
  res = track_write(track, filename.toLocal8Bit().data(), &error);
  if (error)
    {
      emit fileError(QString(error->message));
      g_error_free(error);
    }
  return res;
}
void Maep::Track::addPoint(QGeoPositionInfo &info)
{
  QGeoCoordinate coord = info.coordinate();
  qreal speed;

  speed = 0.;
  if (info.hasAttribute(QGeoPositionInfo::GroundSpeed))
    speed = info.attribute(QGeoPositionInfo::GroundSpeed);

  track_point_new(track, coord.latitude(), coord.longitude(), coord.altitude(),
                  speed, 0., 0.);
}

static void osm_gps_map_qt_repaint(Maep::GpsMap *widget, OsmGpsMap *map);
static void osm_gps_map_qt_coordinate(Maep::GpsMap *widget, GParamSpec *pspec, OsmGpsMap *map);
static void osm_gps_map_qt_double_pixel(Maep::GpsMap *widget, GParamSpec *pspec, OsmGpsMap *map);
static void osm_gps_map_qt_auto_center(Maep::GpsMap *widget, GParamSpec *pspec, OsmGpsMap *map);
static void osm_gps_map_qt_source(Maep::GpsMap *widget, GParamSpec *pspec, OsmGpsMap *map);
static void osm_gps_map_qt_wiki(Maep::GpsMap *widget, MaepGeonamesEntry *entry, MaepWikiContext *wiki);
static void osm_gps_map_qt_places(Maep::GpsMap *widget, MaepSearchContextSource source,
                                  GSList *places, MaepSearchContext *wiki);
static void osm_gps_map_qt_places_failure(Maep::GpsMap *widget,
                                          MaepSearchContextSource source,
                                          GError *error, MaepSearchContext *wiki);

Maep::GpsMap::GpsMap(QQuickItem *parent)
    : QQuickPaintedItem(parent)
{
  char *path;

  gint source = gconf_get_int(GCONF_KEY_SOURCE, OSM_GPS_MAP_SOURCE_OPENSTREETMAP);
  gint zoom = gconf_get_int(GCONF_KEY_ZOOM, 3);
  gfloat lat = gconf_get_float(GCONF_KEY_LATITUDE, 50.0);
  gfloat lon = gconf_get_float(GCONF_KEY_LONGITUDE, 21.0);
  gboolean dpix = gconf_get_bool(GCONF_KEY_DOUBLEPIX, FALSE);
  bool wikipedia = gconf_get_bool(GCONF_KEY_WIKIPEDIA, FALSE);
  bool track = gconf_get_bool(GCONF_KEY_TRACK_CAPTURE, FALSE);
  bool orientation = gconf_get_bool(GCONF_KEY_SCREEN_ROTATE, TRUE);

  path = g_build_filename(g_get_user_data_dir(), "maep", NULL);

  screenRotation = orientation;
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
  coordinate = QGeoCoordinate(lat, lon);

  g_signal_connect_swapped(G_OBJECT(map), "dirty",
                           G_CALLBACK(osm_gps_map_qt_repaint), this);
  g_signal_connect_swapped(G_OBJECT(map), "notify::latitude",
                           G_CALLBACK(osm_gps_map_qt_coordinate), this);
  g_signal_connect_swapped(G_OBJECT(map), "notify::auto-center",
                           G_CALLBACK(osm_gps_map_qt_auto_center), this);
  g_signal_connect_swapped(G_OBJECT(map), "notify::double-pixel",
                           G_CALLBACK(osm_gps_map_qt_double_pixel), this);
  g_signal_connect_swapped(G_OBJECT(map), "notify::map-source",
                           G_CALLBACK(osm_gps_map_qt_source), this);

  g_free(path);

  osd = osm_gps_map_osd_classic_init(map);
  wiki = maep_wiki_context_new();
  wiki_enabled = wikipedia;
  if (wiki_enabled)
    maep_wiki_context_enable(wiki, map);
  wiki_entry = NULL;
  g_signal_connect_swapped(G_OBJECT(wiki), "entry-selected",
                           G_CALLBACK(osm_gps_map_qt_wiki), this);
  search = maep_search_context_new();
  g_signal_connect_swapped(G_OBJECT(search), "places-available",
                           G_CALLBACK(osm_gps_map_qt_places), this);
  g_signal_connect_swapped(G_OBJECT(search), "download-error",
                           G_CALLBACK(osm_gps_map_qt_places_failure), this);

  drag_start_mouse_x = 0;
  drag_start_mouse_y = 0;
  drag_mouse_dx = 0;
  drag_mouse_dy = 0;

  surf = NULL;
  cr = NULL;
  pat = NULL;
  img = NULL;

  forceActiveFocus();
  setAcceptedMouseButtons(Qt::LeftButton);

  gps = QGeoPositionInfoSource::createDefaultSource(this);
  if (gps)
    {
      connect(gps, SIGNAL(positionUpdated(QGeoPositionInfo)),
              this, SLOT(positionUpdate(QGeoPositionInfo)));
      connect(gps, SIGNAL(updateTimeout()),
              this, SLOT(positionLost()));
      gps->setUpdateInterval(1000);
      gps->startUpdates();
    }
  else
    g_message("no gps source...");
  lastGps = QGeoPositionInfo();

  track_capture = track;
  track_current = NULL;
  // setTrackFromFile("/home/nemo/devel/Tinchebray");
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
  osm_gps_map_osd_classic_free(osd);
  maep_wiki_context_enable(wiki, NULL);
  g_object_unref(wiki);
  if (wiki_entry)
    delete(wiki_entry);
  g_object_unref(search);

  if (surf)
    cairo_surface_destroy(surf);
  if (cr)
    cairo_destroy(cr);
  if (pat)
    cairo_pattern_destroy(pat);
  if (img)
    delete(img);
  if (gps)
    delete(gps);
  if (track_current && track_current->parent() == this)
    delete(track_current);

  /* ... and store it in gconf */
  gconf_set_int(GCONF_KEY_ZOOM, zoom);
  gconf_set_int(GCONF_KEY_SOURCE, source);
  gconf_set_float(GCONF_KEY_LATITUDE, lat);
  gconf_set_float(GCONF_KEY_LONGITUDE, lon);
  gconf_set_bool(GCONF_KEY_DOUBLEPIX, dpix);

  gconf_set_bool(GCONF_KEY_WIKIPEDIA, wiki_enabled);

  gconf_set_bool(GCONF_KEY_TRACK_CAPTURE, track_capture);

  gconf_set_bool(GCONF_KEY_SCREEN_ROTATE, screenRotation);

  g_object_unref(map);
}

static void osm_gps_map_qt_repaint(Maep::GpsMap *widget, OsmGpsMap *map)
{
  Q_UNUSED(map);

  widget->mapUpdate();
  widget->update();
}

bool Maep::GpsMap::mapSized()
{
  if (width() < 1 || height() < 1)
    return false;

  if (surf && (cairo_image_surface_get_width(surf) != width() &&
               cairo_image_surface_get_height(surf) != height()))
    {
      cairo_surface_destroy(surf);
      cairo_destroy(cr);
      cairo_pattern_destroy(pat);
      delete(img);
      surf = NULL;
    }

  if (!surf)
    {
      surf = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, width(), height());
      cr = cairo_create(surf);
      pat = cairo_pattern_create_linear (0.0, height(),
                                         0.0, height() - 100);
      cairo_pattern_add_color_stop_rgba (pat, 1, 0, 0, 0, 1);
      cairo_pattern_add_color_stop_rgba (pat, 0, 0, 0, 0, 0);
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
  double w, h;
  cairo_surface_t *map_surf;

  cairo_set_operator (cr, CAIRO_OPERATOR_CLEAR);
  cairo_set_source_rgba(cr, 1., 1., 1., 0.);
  cairo_paint(cr);

  map_surf = osm_gps_map_get_surface(map);
  cairo_set_source_surface(cr, map_surf,
                           drag_mouse_dx - EXTRA_BORDER,
                           drag_mouse_dy - EXTRA_BORDER);
  cairo_set_operator (cr, CAIRO_OPERATOR_SOURCE);
  cairo_paint(cr);
  cairo_surface_destroy(map_surf);

#ifdef ENABLE_OSD
  cairo_set_operator (cr, CAIRO_OPERATOR_OVER);
  osd->draw(osd, cr);
#endif

  w = cairo_image_surface_get_width(surf);
  h = cairo_image_surface_get_height(surf);

  /* Remove the bottom part for a Sailfish toolbar. */
  cairo_save(cr);
  cairo_rectangle (cr, 0., h - 100., w, h);
  cairo_clip(cr);
  cairo_set_operator (cr, CAIRO_OPERATOR_DEST_IN);
  cairo_set_source (cr, pat);
  cairo_paint(cr);
  cairo_restore(cr);
  /* Make top rounded corners. */
  cairo_set_operator (cr, CAIRO_OPERATOR_CLEAR);
  cairo_move_to(cr, 0., 0.);
  cairo_line_to(cr, 20., 0.);
  cairo_arc_negative(cr, 20., 20., 20., 1.5 * G_PI, G_PI);
  cairo_line_to(cr, 0., 0.);
  cairo_set_source_rgb (cr, 1., 1., 1.);
  cairo_fill(cr);

  cairo_move_to(cr, w, 0.);
  cairo_line_to(cr, w - 20., 0.);
  cairo_arc(cr, w - 20., 20., 20., 1.5 * G_PI, 2. * G_PI);
  cairo_line_to(cr, w, 0.);
  cairo_set_source_rgb (cr, 1., 1., 1.);
  cairo_fill(cr);

  emit mapChanged();
}

void Maep::GpsMap::paintTo(QPainter *painter, int width, int height)
{
  int w, h;

  if (!img || !surf)
    return;

  w = cairo_image_surface_get_width(surf);
  h = cairo_image_surface_get_height(surf);
  g_message("Paint to %dx%d from %fx%f - %fx%f.", width, height,
            (w - width) * 0.5, (h - height) * 0.5,
            (w + width) * 0.5, (h + height) * 0.5);
  QRectF target(0, 0, width, height);
  QRectF source((w - width) * 0.5, (h - height) * 0.5,
                width, height);
  painter->drawImage(target, *img, source);
}

void Maep::GpsMap::paint(QPainter *painter)
{
  if (mapSized())
    mapUpdate();
  paintTo(painter, width(), height());
}

void Maep::GpsMap::zoomIn()
{
  osm_gps_map_zoom_in(map);
}
void Maep::GpsMap::zoomOut()
{
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

void Maep::GpsMap::mousePressEvent(QMouseEvent *event)
{
// #ifdef ENABLE_OSD
//   int step;
//   osd_button_t but = 
//     osd->check(osd, TRUE, event->x(), event->y());
  
//   dragging = FALSE;

//   step = width() / OSM_GPS_MAP_SCROLL_STEP;

//   if(but != OSD_NONE)
//     switch(but)
//       {
//       case OSD_UP:
//         osm_gps_map_scroll(map, 0, -step);
//         g_object_set(G_OBJECT(map), "auto-center", FALSE, NULL);
//         return;

//       case OSD_DOWN:
//         osm_gps_map_scroll(map, 0, +step);
//         g_object_set(G_OBJECT(map), "auto-center", FALSE, NULL);
//         return;

//       case OSD_LEFT:
//         osm_gps_map_scroll(map, -step, 0);
//         g_object_set(G_OBJECT(map), "auto-center", FALSE, NULL);
//         return;
                
//       case OSD_RIGHT:
//         osm_gps_map_scroll(map, +step, 0);
//         g_object_set(G_OBJECT(map), "auto-center", FALSE, NULL);
//         return;
                
//       case OSD_IN:
//         osm_gps_map_zoom_in(map);
//         return;
                
//       case OSD_OUT:
//         osm_gps_map_zoom_out(map);
//         return;

//       case OSD_GPS:
//         if (lastGps.isValid())
//           {
//             osm_gps_map_set_center(map, lastGps.coordinate().latitude(),
//                                    lastGps.coordinate().longitude());

//             g_object_set(map, "auto-center", TRUE, NULL);
//           }
//         return;
                
//       default:
//         g_warning("Hey don't know what to do!");
//         /* all custom buttons are forwarded to the application */
//         // if(osd->cb)
//         //   osd->cb(but, osd->data);
//         return;
//       }
// #endif
  if (osm_gps_map_layer_button(OSM_GPS_MAP_LAYER(wiki),
                               event->x(), event->y(), TRUE))
    return;

  dragging = TRUE;
  drag_start_mouse_x = event->x();
  drag_start_mouse_y = event->y();
}
void Maep::GpsMap::mouseReleaseEvent(QMouseEvent *event)
{
  if (dragging)
    {
      dragging = FALSE;
      osm_gps_map_scroll(map, -drag_mouse_dx, -drag_mouse_dy);
      drag_mouse_dx = 0;
      drag_mouse_dy = 0;
    }
  osm_gps_map_layer_button(OSM_GPS_MAP_LAYER(wiki),
                           event->x(), event->y(), FALSE);
}
void Maep::GpsMap::mouseMoveEvent(QMouseEvent *event)
{
  if (dragging)
    {
      drag_mouse_dx = event->x() - drag_start_mouse_x;
      drag_mouse_dy = event->y() - drag_start_mouse_y;
      mapUpdate();
      update();
    }
}

static void osm_gps_map_qt_source(Maep::GpsMap *widget,
                                  GParamSpec *pspec, OsmGpsMap *map)
{
  Q_UNUSED(pspec);
  Q_UNUSED(map);

  widget->sourceChanged(widget->source());
}
void Maep::GpsMap::setSource(Maep::GpsMap::Source value)
{
  Source orig;

  orig = source();
  if (orig == value)
    return;

  g_object_set(map, "map-source", (OsmGpsMapSource_t)value, NULL);
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

  g_object_set(map, "auto-center", status, NULL);

  if (status && lastGps.isValid())
    osm_gps_map_set_center(map, lastGps.coordinate().latitude(),
                           lastGps.coordinate().longitude());
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
  float track;

  g_message("position is %f %f", info.coordinate().latitude(),
            info.coordinate().longitude());
  if (info.hasAttribute(QGeoPositionInfo::HorizontalAccuracy))
    g_object_set(map, "gps-track-highlight-radius",
                 (int)(info.attribute(QGeoPositionInfo::HorizontalAccuracy) /
                       osm_gps_map_get_scale(map)), NULL);
  if (info.hasAttribute(QGeoPositionInfo::Direction))
    track = info.attribute(QGeoPositionInfo::Direction);
  else
    track = OSM_GPS_MAP_INVALID;
  g_message("heading is %g", track);

  lastGps = info;
  emit gpsCoordinateChanged();

  osm_gps_map_draw_gps(map, info.coordinate().latitude(),
                       info.coordinate().longitude(), track);

  // Add track capture, if any.
  if (track_capture)
    gpsToTrack();
}
void Maep::GpsMap::positionLost()
{
  lastGps = QGeoPositionInfo();
  emit gpsCoordinateChanged();

  osm_gps_map_clear_gps(map);
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
  track_current = track;

  if (track)
    {
      g_message("Reparenting.");
      track->setParent(this);

      if (!track->getSource().isEmpty())
        gconf_set_string(GCONF_KEY_TRACK_PATH, track->getSource().toLocal8Bit().data());
    }

  emit trackChanged(track != NULL);

  if (track && track->get())
    {
      coord_t top_left, bottom_right;

      /* Set track for map. */
      osm_gps_map_add_track(map, track->get());

      /* Adjust map zoom and location according to track bounding box. */
      if (track_bounding_box(track->get(), &top_left, &bottom_right))
        osm_gps_map_adjust_to(map, &top_left, &bottom_right);
    }
}
void Maep::GpsMap::gpsToTrack()
{
  if (!track_current)
    {
      Maep::Track *track = new Maep::Track();
      track->setParent(this);

      setTrack(track);
    }

  track_current->addPoint(lastGps);
}
QString Maep::GpsMap::getCenteredTile(Maep::GpsMap::Source source) const
{
  gchar *cache_dir, *base, *file, *uri;
  int zoom, x, y;
  QString out;

  g_object_get(G_OBJECT(map), "tile-cache-base", &base, NULL);
  osm_gps_map_get_tile_xy_at(map, coordinate.latitude(), coordinate.longitude(),
                             &zoom, &x, &y);
  cache_dir = osm_gps_map_source_get_cache_dir((OsmGpsMapSource_t)source,
                                               OSM_GPS_MAP_CACHE_FRIENDLY,
                                               base);
  g_free(base);

  file = osm_gps_map_source_get_cached_file((OsmGpsMapSource_t)source,
                                            cache_dir, zoom, x, y);
  g_free(cache_dir);
  if (file) {
    g_message("Get cached file for source %d: %s.", source, file);
    out = QString(file);
    g_free(file);
    return out;
  }
  
  uri = osm_gps_map_source_get_tile_uri((OsmGpsMapSource_t)source,
                                        zoom, x, y);
  g_message("Get uri for source %d: %s.", source, uri);
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
