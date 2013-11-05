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
  Q_PROPERTY(float latitude READ getLat)
  Q_PROPERTY(float longitude READ getLon)

public:
  inline GeonamesPlace(const MaepGeonamesPlace *place = NULL, QObject *parent = NULL) : QObject(parent)
    {
      if (place)
        {
          this->name = QString(place->name);
          this->country = QString(place->country);
          this->lat = place->pos.rlat;
          this->lon = place->pos.rlon;
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
    inline float getLat()
    {
        return this->lat;
    }    
    inline float getLon()
    {
        return this->lon;
    }

signals:
    void nameChanged();
    void countryChanged();

private:
    QString name, country;
    float lat, lon;
};

class GpsMap : public QQuickPaintedItem
{
  Q_OBJECT
  Q_PROPERTY(bool wiki_status READ wikiStatus WRITE setWikiStatus NOTIFY wikiStatusChanged)
  Q_PROPERTY(QString wiki_title READ wikiTitle)
  Q_PROPERTY(QString wiki_summary READ wikiSummary)
  Q_PROPERTY(QString wiki_thumbnail READ wikiThumbnail)
  Q_PROPERTY(QString wiki_url READ wikiUrl)
  Q_PROPERTY(QGeoCoordinate wiki_coordinate READ wikiCoord)
  Q_PROPERTY(QQmlListProperty<Maep::GeonamesPlace> search_results READ getSearchResults NOTIFY searchResults)
  Q_PROPERTY(bool track_capture READ trackCapture WRITE setTrackCapture NOTIFY trackCaptureChanged)
  Q_PROPERTY(bool track_available READ hasTrack NOTIFY trackAvailable)

 public:
  GpsMap(QQuickItem *parent = 0);
  ~GpsMap();
  inline bool wikiStatus() {
    return wiki_enabled;
  }
  inline QString wikiTitle() const {
    return wiki_title;
  }
  inline QString wikiSummary() const {
    return wiki_summary;
  }
  inline QString wikiThumbnail() const {
    return wiki_thumbnail;
  }
  inline QString wikiUrl() const {
    return wiki_url;
  }
  inline QGeoCoordinate wikiCoord() const {
    return wiki_coord;
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

 protected:
  void paint(QPainter *painter);
  void keyPressEvent(QKeyEvent * event);
  void mousePressEvent(QMouseEvent *event);
  void mouseReleaseEvent(QMouseEvent *event);
  void mouseMoveEvent(QMouseEvent *event);

 signals:
  void mapChanged();
  void wikiStatusChanged(bool status);
  void wikiURLSelected();
  void searchRequest();
  void searchResults();
  void trackCaptureChanged(bool status);
  void trackAvailable(bool status);

 public slots:
  void setWikiStatus(bool status);
  void setWikiInfo(const char *title, const char *summary,
                   const char *thumbnail, const char *url,
                   float lat, float lon);
  void setSearchRequest(const QString &request);
  void setSearchResults(GSList *places);
  void setLookAt(float lat, float lon);
  QString getWikiPosition();
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
              self->searchRes[index]->getLat(), self->searchRes[index]->getLon(), index);
    return self->searchRes[index];
  }
  void gps_to_track();

  OsmGpsMap *map;
  osm_gps_map_osd_t *osd;

  MaepSearchContext *search;
  QList<GeonamesPlace*> searchRes;

  gboolean dragging;
  int drag_start_mouse_x, drag_start_mouse_y;
  int drag_mouse_dx, drag_mouse_dy;

  /* Wiki entry. */
  bool wiki_enabled;
  MaepWikiContext *wiki;
  QString wiki_title, wiki_summary, wiki_thumbnail, wiki_url;
  QGeoCoordinate wiki_coord;

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
