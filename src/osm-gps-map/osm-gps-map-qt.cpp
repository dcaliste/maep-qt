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

#define MAP_SOURCE  OSM_GPS_MAP_SOURCE_OPENCYCLEMAP

QString Maep::GeonamesPlace::coordinateToString(QGeoCoordinate::CoordinateFormat format) const
{
  return coordinate.toString(format);
}
QString Maep::GeonamesEntry::coordinateToString(QGeoCoordinate::CoordinateFormat format) const
{
  return coordinate.toString(format);
}

static void osm_gps_map_qt_repaint(Maep::GpsMap *widget, OsmGpsMap *map);
static void osm_gps_map_qt_coordinate(Maep::GpsMap *widget, GParamSpec *pspec, OsmGpsMap *map);
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

  gint source = gconf_get_int(GCONF_KEY_SOURCE, MAP_SOURCE);
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
      gps->startUpdates();
    }
  else
    g_message("no source...");
  gps_fix = false;

  track_capture = track;
  track_current = NULL;
  setTrackFromFile("/home/nemo/devel/Tinchebray");
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
  if (img)
    delete(img);
  if (gps)
    delete(gps);
  if (track_current)
    track_state_unref(track_current);

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
  widget->mapUpdate();
  widget->update();
}

void Maep::GpsMap::mapUpdate()
{
  cairo_surface_t *map_surf;

  if (width() < 1 || height() < 1)
    return;

  if (!surf)
    {
      surf = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, width(), height());
      cr = cairo_create(surf);
      img = new QImage(cairo_image_surface_get_data(surf),
                       cairo_image_surface_get_width(surf),
                       cairo_image_surface_get_height(surf),
                       QImage::Format_ARGB32);
      osm_gps_map_set_viewport(map, width(), height());
    }

  cairo_set_source_rgb(cr, 1., 1., 1.);
  cairo_paint(cr);

  map_surf = osm_gps_map_get_surface(map);
  cairo_set_source_surface(cr, map_surf,
                           drag_mouse_dx - EXTRA_BORDER,
                           drag_mouse_dy - EXTRA_BORDER);
  cairo_paint(cr);
  cairo_surface_destroy(map_surf);

#ifdef ENABLE_OSD
  osd->draw (osd, cr);
#endif

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
  if (!img)
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
#ifdef ENABLE_OSD
  /* pressed inside OSD control? */
  int step;
  osd_button_t but = 
    osd->check(osd, TRUE, event->x(), event->y());
  
  dragging = FALSE;

  step = width() / OSM_GPS_MAP_SCROLL_STEP;

  if(but != OSD_NONE)
    switch(but)
      {
      case OSD_UP:
        osm_gps_map_scroll(map, 0, -step);
        g_object_set(G_OBJECT(map), "auto-center", FALSE, NULL);
        return;

      case OSD_DOWN:
        osm_gps_map_scroll(map, 0, +step);
        g_object_set(G_OBJECT(map), "auto-center", FALSE, NULL);
        return;

      case OSD_LEFT:
        osm_gps_map_scroll(map, -step, 0);
        g_object_set(G_OBJECT(map), "auto-center", FALSE, NULL);
        return;
                
      case OSD_RIGHT:
        osm_gps_map_scroll(map, +step, 0);
        g_object_set(G_OBJECT(map), "auto-center", FALSE, NULL);
        return;
                
      case OSD_IN:
        osm_gps_map_zoom_in(map);
        return;
                
      case OSD_OUT:
        osm_gps_map_zoom_out(map);
        return;

      case OSD_GPS:
        if (gps_fix)
          {
            osm_gps_map_set_center(map, gps_lat, gps_lon);

            /* re-enable centering */
            g_object_set(map, "auto-center", TRUE, NULL);
          }
        return;
                
      default:
        g_warning("Hey don't know what to do!");
        /* all custom buttons are forwarded to the application */
        // if(osd->cb)
        //   osd->cb(but, osd->data);
        return;
      }
#endif
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
  g_message("Got %d matching places.", g_slist_length(places));
  widget->setSearchResults(source, places);
}
static void osm_gps_map_qt_places_failure(Maep::GpsMap *widget,
                                          MaepSearchContextSource source,
                                          GError *error, MaepSearchContext *wiki)
{
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

  QGeoCoordinate coord = QGeoCoordinate(lat, lon);
  QGeoPositionInfo info = QGeoPositionInfo(coord, QDateTime::currentDateTime());
  info.setAttribute(QGeoPositionInfo::HorizontalAccuracy, 2567.);
  positionUpdate(info);
}
void Maep::GpsMap::setCoordinate(float lat, float lon)
{
  coordinate = QGeoCoordinate(lat, lon);
  emit coordinateChanged();
}
static void osm_gps_map_qt_coordinate(Maep::GpsMap *widget, GParamSpec *pspec, OsmGpsMap *map)
{
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

  g_message("position is %f %f\n", info.coordinate().latitude(),
            info.coordinate().longitude());
  if (info.hasAttribute(QGeoPositionInfo::HorizontalAccuracy))
    g_object_set(map, "gps-track-highlight-radius",
                 (int)(info.attribute(QGeoPositionInfo::HorizontalAccuracy) /
                       osm_gps_map_get_scale(map)), NULL);
  if (info.hasAttribute(QGeoPositionInfo::Direction))
    track = info.attribute(QGeoPositionInfo::Direction);
  else
    track = 0.;

  osm_gps_map_osd_enable_gps(osd, TRUE);

  gps_fix = true;
  gps_lat = info.coordinate().latitude();
  gps_lon = info.coordinate().longitude();
  osm_gps_map_draw_gps(map, gps_lat, gps_lon, track);

  // Add track capture, if any.
  if (track_capture)
    gps_to_track();
}
void Maep::GpsMap::positionLost()
{
  osm_gps_map_osd_enable_gps(osd, FALSE);

  gps_fix = false;
  osm_gps_map_clear_gps(map);
}

void Maep::GpsMap::setTrackCapture(bool status)
{
  if (status == track_capture)
    return;

  track_capture = status;
  emit trackCaptureChanged(track_capture);
  
  if (status && gps_fix)
    gps_to_track();
}

void Maep::GpsMap::setTrackFromFile(const QString &filename)
{
  track_state_t *track_state;

  osm_gps_map_clear_tracks(map);

  track_state = track_read(filename.toLocal8Bit().data(), FALSE);
  if (!track_state)
    return;

  gconf_set_string(GCONF_KEY_TRACK_PATH, filename.toLocal8Bit().data());
  if (track_current)
    track_state_unref(track_current);
  track_current = track_state;
  osm_gps_map_add_track(map, track_state);
  
  emit trackAvailable(true);
}
void Maep::GpsMap::exportTrackToFile(const QString &filename)
{
  track_write(filename.toLocal8Bit().data(), track_current);
}
void Maep::GpsMap::clearTrack()
{
  if (track_current)
    track_state_unref(track_current);
  track_current = NULL;

  osm_gps_map_clear_tracks(map);
  
  emit trackAvailable(false);
}
bool Maep::GpsMap::hasTrack()
{
  return (track_current != NULL);
}
void Maep::GpsMap::gps_to_track()
{
  track_state_t *track_state;
      
  track_state = track_current;
  track_current = track_point_new(track_current, gps_lat, gps_lon,
                                  0., 0., 0., 0.);
  if (!track_state)
    {
      osm_gps_map_clear_tracks(map);
      osm_gps_map_add_track(map, track_current);
    }
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
