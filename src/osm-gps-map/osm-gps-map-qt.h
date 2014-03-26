/*
 * osm-gps-map-qt.h
 * Copyright (C) Damien Caliste 2013-2014 <dcaliste@free.fr>
 *
 * osm-gps-map-qt.h is free software: you can redistribute it and/or modify it
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

#ifndef OSM_GPS_MAP_QT_H
#define OSM_GPS_MAP_QT_H

#include <QQuickPaintedItem>
#include <QFile>
#include <QImage>
#include <QString>
#include <QQmlListProperty>
#include <QGeoCoordinate>
#include <QGeoPositionInfoSource>
#include <cairo.h>
#include "misc.h"
#include "search.h"
#include "track.h"
#include "osm-gps-map/osm-gps-map.h"
#include "osm-gps-map/layer-wiki.h"
#include "osm-gps-map/osm-gps-map-osd-classic.h"

namespace Maep {

class Conf: public QObject
{
  Q_OBJECT
  
public:
  Q_INVOKABLE inline QString getString(const QString &key, const QString &fallback = NULL) const
  {
    gchar *val;
    QString ret;

    val = gconf_get_string(key.toLocal8Bit().data());
    ret = QString(val);
    if (val)
      g_free(val);
    else if (fallback != NULL)
      ret = QString(fallback);
    return ret;
  }
  Q_INVOKABLE inline int getInt(const QString &key, const int fallback) const
  {
    return gconf_get_int(key.toLocal8Bit().data(), fallback);
  }

public slots:
  inline void setString(const QString &key, const QString &value)
  {
    gconf_set_string(key.toLocal8Bit().data(), value.toLocal8Bit().data());
  }
  inline void setInt(const QString &key, const int value)
  {
    gconf_set_int(key.toLocal8Bit().data(), value);
  }
};

class GeonamesPlace: public QObject
{
  Q_OBJECT
  Q_PROPERTY(QString name READ getName NOTIFY nameChanged)
  Q_PROPERTY(QString country READ getCountry NOTIFY countryChanged)
  Q_PROPERTY(QGeoCoordinate coordinate READ getCoord NOTIFY coordinateChanged)

public:
  inline GeonamesPlace(const MaepGeonamesPlace *place = NULL, QObject *parent = NULL) : QObject(parent)
  {
    if (place)
      {
        this->name = QString(place->name);
        this->country = QString(place->country);
        this->coordinate = QGeoCoordinate(rad2deg(place->pos.rlat),
                                          rad2deg(place->pos.rlon));
      }
  }
  inline QString getName() const
  {
    return this->name;
  }    
  inline QString getCountry() const
  {
    return this->country;
  }     
  inline QGeoCoordinate getCoord() const
  {
    return this->coordinate;
  }    

signals:
  void nameChanged();
  void countryChanged();
  void coordinateChanged();

public slots:
  QString coordinateToString(QGeoCoordinate::CoordinateFormat format = QGeoCoordinate::DegreesMinutesSecondsWithHemisphere) const;

private:
  QString name, country;
  QGeoCoordinate coordinate;
};

class GeonamesEntry: public QObject
{
  Q_OBJECT
  Q_PROPERTY(QString title READ getTitle NOTIFY titleChanged)
  Q_PROPERTY(QString summary READ getSummary NOTIFY summaryChanged)
  Q_PROPERTY(QString thumbnail READ getThumbnail NOTIFY thumbnailChanged)
  Q_PROPERTY(QString url READ getURL NOTIFY urlChanged)
  Q_PROPERTY(QGeoCoordinate coordinate READ getCoord NOTIFY coordinateChanged)

public:
  inline GeonamesEntry(const MaepGeonamesEntry *entry = NULL,
                       QObject *parent = NULL) : QObject(parent)
  {
    if (entry)
      {
        this->title = QString(entry->title);
        this->summary = QString(entry->summary);
        this->thumbnail = QString(entry->thumbnail_url);
        this->url = QString(entry->url);
        this->coordinate = QGeoCoordinate(rad2deg(entry->pos.rlat),
                                          rad2deg(entry->pos.rlon));
      }
  }
  inline void set(const Maep::GeonamesEntry *entry)
  {
    title = QString(entry->getTitle());
  }
  inline QString getTitle() const
  {
    return this->title;
  }    
  inline QString getSummary() const
  {
    return this->summary;
  }     
  inline QString getThumbnail() const
  {
    return this->thumbnail;
  }     
  inline QString getURL() const
  {
    return this->url;
  }     
  inline QGeoCoordinate getCoord() const
  {
    return this->coordinate;
  }    

signals:
  void titleChanged();
  void summaryChanged();
  void thumbnailChanged();
  void urlChanged();
  void coordinateChanged();

public slots:
  QString coordinateToString(QGeoCoordinate::CoordinateFormat format = QGeoCoordinate::DegreesMinutesSecondsWithHemisphere) const;

private:
    QString title, summary, thumbnail, url;
    QGeoCoordinate coordinate;
};

class Track: public QObject
{
  Q_OBJECT
  Q_PROPERTY(unsigned int autosavePeriod READ getAutosavePeriod WRITE setAutosavePeriod NOTIFY autosavePeriodChanged)
  Q_PROPERTY(QString path READ getPath NOTIFY pathChanged)
  Q_PROPERTY(unsigned int startDate READ getStartDate NOTIFY startDateSet)
  Q_PROPERTY(qreal length READ getLength NOTIFY characteristicsChanged)
  Q_PROPERTY(unsigned int duration READ getDuration NOTIFY characteristicsChanged)

public:
  Q_INVOKABLE inline Track(track_state_t *track = NULL,
               QObject *parent = NULL) : QObject(parent)
  {
    if (track)
      track_state_ref(track);
    else
      track = track_state_new();
    this->track = track;
    autosavePeriod = 0;
  }
  inline ~Track()
  {
    track_state_unref(track);
  }
  inline track_state_t* get() const {
    return track;
  }
  inline QString getPath() const {
    return source;
  }
  Q_INVOKABLE inline bool isEmpty() {
    return track_length(track) == 0;
  }
  inline unsigned int getAutosavePeriod() {
    return autosavePeriod;
  }
  inline qreal getLength() {
    return (qreal)track_metric_length(track);
  }
  inline unsigned int getDuration() {
    return (unsigned int)track_duration(track);
  }
  inline unsigned int getStartDate() {
    return (unsigned int)track_start_timestamp(track);
  }

signals:
  void fileError(const QString &errorMsg);
  void autosavePeriodChanged(unsigned int value);
  void pathChanged();
  void characteristicsChanged(qreal length, unsigned int duration);
  void startDateSet(unsigned int value);

public slots:
  void set(track_state_t *track);
  bool set(const QString &filename);
  bool toFile(const QString &filename);
  void addPoint(QGeoPositionInfo &info);
  bool setAutosavePeriod(unsigned int value);

private:
  track_state_t *track;
  QString source;
  unsigned int autosavePeriod;
};

class GpsMap : public QQuickPaintedItem
{
  Q_OBJECT

  Q_ENUMS(Source)

  Q_PROPERTY(Source source READ source WRITE setSource NOTIFY sourceChanged)
  Q_PROPERTY(bool double_pixel READ doublePixel WRITE setDoublePixel NOTIFY doublePixelChanged)

  Q_PROPERTY(QGeoCoordinate coordinate READ getCoord WRITE setLookAt NOTIFY coordinateChanged)
  Q_PROPERTY(QGeoCoordinate gps_coordinate READ getGpsCoord NOTIFY gpsCoordinateChanged)

  Q_PROPERTY(bool auto_center READ autoCenter WRITE setAutoCenter NOTIFY autoCenterChanged)

  Q_PROPERTY(bool wiki_status READ wikiStatus WRITE setWikiStatus NOTIFY wikiStatusChanged)
  Q_PROPERTY(Maep::GeonamesEntry *wiki_entry READ getWikiEntry NOTIFY wikiEntryChanged)

  Q_PROPERTY(QQmlListProperty<Maep::GeonamesPlace> search_results READ getSearchResults)

  Q_PROPERTY(bool track_capture READ trackCapture WRITE setTrackCapture NOTIFY trackCaptureChanged)
  Q_PROPERTY(Maep::Track *track READ getTrack WRITE setTrack NOTIFY trackChanged)

  Q_PROPERTY(bool screen_rotation READ screen_rotation WRITE setScreenRotation NOTIFY screenRotationChanged)

  Q_PROPERTY(unsigned int gps_refresh_rate READ gpsRefreshRate WRITE setGpsRefreshRate NOTIFY gpsRefreshRateChanged)

  Q_PROPERTY(QString version READ version CONSTANT)
  Q_PROPERTY(QString compilation_date READ compilation_date CONSTANT)
  Q_PROPERTY(QString authors READ authors CONSTANT)
  Q_PROPERTY(QString license READ license CONSTANT)

 public:

  enum Source {
    SOURCE_NULL,
    SOURCE_OPENSTREETMAP,
    SOURCE_OPENSTREETMAP_RENDERER,
    SOURCE_OPENAERIALMAP,
    SOURCE_OPENSEAMAP,
    SOURCE_MAPS_FOR_FREE,
    SOURCE_OPENCYCLEMAP,
    SOURCE_OSM_PUBLIC_TRANSPORT,
    SOURCE_GOOGLE_STREET,
    SOURCE_GOOGLE_SATELLITE,
    SOURCE_GOOGLE_HYBRID,
    SOURCE_VIRTUAL_EARTH_STREET,
    SOURCE_VIRTUAL_EARTH_SATELLITE,
    SOURCE_VIRTUAL_EARTH_HYBRID,
    SOURCE_YAHOO_STREET,
    SOURCE_YAHOO_SATELLITE,
    SOURCE_YAHOO_HYBRID,
    SOURCE_OSMC_TRAILS,

    SOURCE_LAST};

  GpsMap(QQuickItem *parent = 0);
  ~GpsMap();
  inline QGeoCoordinate getCoord() const {
    return coordinate;
  }
  inline QGeoCoordinate getGpsCoord() const {
    if (lastGps.isValid())
      return lastGps.coordinate();
    else
      return QGeoCoordinate();
  }
  inline bool wikiStatus() {
    return wiki_enabled;
  }
  inline Maep::GeonamesEntry* getWikiEntry() const {
    return wiki_entry;
  }
  inline QQmlListProperty<Maep::GeonamesPlace> getSearchResults() {
    return QQmlListProperty<Maep::GeonamesPlace>(this, NULL,
                                                 GpsMap::countSearchResults,
                                                 GpsMap::atSearchResults);
  }
  void mapUpdate();
  void paintTo(QPainter *painter, int width, int height);
  inline bool trackCapture() {
    return track_capture;
  }
  inline Maep::Track* getTrack() {
    return track_current;
  }
  inline bool screen_rotation() const {
    return screenRotation;
  }
  inline QString version() const {
    return QString(VERSION);
  }
  inline QString compilation_date() const {
    return QString(__DATE__ " " __TIME__);
  }
  inline QString authors() const {
    QFile file(DEPLOYMENT_PATH"/AUTHORS");

    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
      return QString("File AUTHORS not found");

    QTextStream in(&file);
    return in.readAll();
  }
  inline QString license() const {
    QFile file(DEPLOYMENT_PATH"/COPYING");

    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
      return QString("File COPYING not found");

    QTextStream in(&file);
    return in.readAll();
  }
  inline bool autoCenter() {
    gboolean set;
    g_object_get(map, "auto-center", &set, NULL);
    return set;
  }
  inline Source source() {
    OsmGpsMapSource_t source;
    g_object_get(map, "map-source", &source, NULL);
    return (Source)source;
  }
  Q_INVOKABLE inline QString sourceLabel(Source id) const {
    return QString(osm_gps_map_source_get_friendly_name((OsmGpsMapSource_t)id));
  }
  Q_INVOKABLE inline QString sourceCopyrightNotice(Source id) const {
    const gchar *notice, *url;
    osm_gps_map_source_get_repo_copyright((OsmGpsMapSource_t)id, &notice, &url);
    return QString(notice);
  }
  Q_INVOKABLE inline QString sourceCopyrightUrl(Source id) const {
    const gchar *notice, *url;
    osm_gps_map_source_get_repo_copyright((OsmGpsMapSource_t)id, &notice, &url);
    return QString(url);
  }
  inline bool doublePixel() {
    gboolean status;
    g_object_get(map, "double-pixel", &status, NULL);
    return status;
  }
  Q_INVOKABLE QString getCenteredTile(Maep::GpsMap::Source source) const;
  inline unsigned int gpsRefreshRate() {
    return gpsRefreshRate_;
  }

 protected:
  void paint(QPainter *painter);
  void keyPressEvent(QKeyEvent * event);
  void mousePressEvent(QMouseEvent *event);
  void mouseReleaseEvent(QMouseEvent *event);
  void mouseMoveEvent(QMouseEvent *event);

 signals:
  void mapChanged();
  void sourceChanged(Source source);
  void doublePixelChanged(bool status);
  void coordinateChanged();
  void gpsCoordinateChanged();
  void autoCenterChanged(bool status);
  void wikiStatusChanged(bool status);
  void wikiEntryChanged();
  void searchRequest();
  void searchResults();
  void trackCaptureChanged(bool status);
  void trackChanged(bool available);
  void screenRotationChanged(bool status);
  void gpsRefreshRateChanged(unsigned int rate);

 public slots:
  void setSource(Source source);
  void setDoublePixel(bool status);
  void setAutoCenter(bool status);
  void setScreenRotation(bool status);
  void setCoordinate(float lat, float lon);
  void setWikiStatus(bool status);
  void setWikiEntry(const MaepGeonamesEntry *entry);
  void setSearchRequest(const QString &request);
  void setSearchResults(MaepSearchContextSource source, GSList *places);
  void setLookAt(float lat, float lon);
  inline void setLookAt(QGeoCoordinate coord) {
    setLookAt(coord.latitude(), coord.longitude());
  }
  void zoomIn();
  void zoomOut();
  void positionUpdate(const QGeoPositionInfo &info);
  void positionLost();
  void setTrackCapture(bool status);
  void setTrack(Maep::Track *track = NULL);
  void setGpsRefreshRate(unsigned int rate);

 private:
  static int countSearchResults(QQmlListProperty<GeonamesPlace> *prop)
  {
    GpsMap *self = qobject_cast<GpsMap*>(prop->object);
    g_message("#### Hey I've got %d results!", self->searchRes.length());
    return self->searchRes.length();
  }
  static GeonamesPlace* atSearchResults(QQmlListProperty<GeonamesPlace> *prop, int index)
  {
    GpsMap *self = qobject_cast<GpsMap*>(prop->object);
    g_message("#### Hey I've got name %s (%fx%f) for result %d!",
              self->searchRes[index]->getName().toLocal8Bit().data(),
              self->searchRes[index]->getCoord().latitude(),
              self->searchRes[index]->getCoord().longitude(), index);
    return self->searchRes[index];
  }
  bool mapSized();
  void gpsToTrack();

  bool screenRotation;
  OsmGpsMap *map;
  QGeoCoordinate coordinate;
  osm_gps_map_osd_t *osd;

  MaepSearchContext *search;
  QList<GeonamesPlace*> searchRes;
  guint searchFinished;

  gboolean dragging;
  int drag_start_mouse_x, drag_start_mouse_y;
  int drag_mouse_dx, drag_mouse_dy;

  /* Wiki entry. */
  bool wiki_enabled;
  MaepWikiContext *wiki;
  GeonamesEntry *wiki_entry;

  /* Screen display. */
  cairo_surface_t *surf;
  cairo_t *cr;
  cairo_pattern_t *pat;
  QImage *img;

  /* GPS */
  unsigned int gpsRefreshRate_;
  QGeoPositionInfoSource *gps;
  QGeoPositionInfo lastGps;

  /* Tracks */
  bool track_capture;
  Maep::Track *track_current;
};

class GpsMapCover : public QQuickPaintedItem
{
  Q_OBJECT
    Q_PROPERTY(Maep::GpsMap* map READ map WRITE setMap NOTIFY mapChanged)
  Q_PROPERTY(bool status READ status WRITE setStatus NOTIFY statusChanged)

 public:
  GpsMapCover(QQuickItem *parent = 0);
  ~GpsMapCover();
  Maep::GpsMap* map() const;
  bool status();

 protected:
  void paint(QPainter *painter);

 signals:
  void mapChanged();
  void statusChanged();

 public slots:
  void setMap(Maep::GpsMap *map);
  void setStatus(bool active);
  void updateCover();

 private:
  bool status_;
  GpsMap *map_;
};

}

#endif
