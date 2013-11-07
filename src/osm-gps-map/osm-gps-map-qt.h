/*
 * osm-gps-map-qt.h
 * Copyright (C) Damien Caliste 2013 <dcaliste@free.fr>
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
#include <QImage>
#include <QString>
#include <QQmlListProperty>
#include <QGeoCoordinate>
#include <QGeoPositionInfoSource>
#include <cairo.h>
#include "search.h"
#include "track.h"
#include "osm-gps-map/osm-gps-map.h"
#include "osm-gps-map/layer-wiki.h"
#include "osm-gps-map/osm-gps-map-osd-classic.h"

namespace Maep {

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

class GpsMap : public QQuickPaintedItem
{
  Q_OBJECT
  Q_PROPERTY(QGeoCoordinate coordinate READ getCoord WRITE setLookAt NOTIFY coordinateChanged)
  Q_PROPERTY(bool wiki_status READ wikiStatus WRITE setWikiStatus NOTIFY wikiStatusChanged)
  Q_PROPERTY(Maep::GeonamesEntry *wiki_entry READ getWikiEntry NOTIFY wikiEntryChanged)
  Q_PROPERTY(QQmlListProperty<Maep::GeonamesPlace> search_results READ getSearchResults)
  Q_PROPERTY(bool track_capture READ trackCapture WRITE setTrackCapture NOTIFY trackCaptureChanged)
  Q_PROPERTY(bool track_available READ hasTrack NOTIFY trackAvailable)
  Q_PROPERTY(bool screen_rotation READ screen_rotation WRITE setScreenRotation NOTIFY screenRotationChanged)

 public:
  GpsMap(QQuickItem *parent = 0);
  ~GpsMap();
  inline QGeoCoordinate getCoord() const {
    return coordinate;
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
  bool hasTrack();
  inline bool screen_rotation() const {
    return screenRotation;
  }

 protected:
  void paint(QPainter *painter);
  void keyPressEvent(QKeyEvent * event);
  void mousePressEvent(QMouseEvent *event);
  void mouseReleaseEvent(QMouseEvent *event);
  void mouseMoveEvent(QMouseEvent *event);

 signals:
  void mapChanged();
  void coordinateChanged();
  void wikiStatusChanged(bool status);
  void wikiEntryChanged();
  void searchRequest();
  void searchResults();
  void trackCaptureChanged(bool status);
  void trackAvailable(bool status);
  void screenRotationChanged(bool status);

 public slots:
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
  void setTrackFromFile(const QString &filename);
  void exportTrackToFile(const QString &filename);
  void clearTrack();

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
  void gps_to_track();

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
  QImage *img;

  /* GPS */
  QGeoPositionInfoSource *gps;
  bool gps_fix;
  float gps_lat, gps_lon;

  /* Tracks */
  bool track_capture;
  track_state_t *track_current;
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
