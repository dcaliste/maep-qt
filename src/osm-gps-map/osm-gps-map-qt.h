#ifndef OSM_GPS_MAP_QT_H
#define OSM_GPS_MAP_QT_H

#include <QQuickPaintedItem>
#include <QImage>
#include <QString>
#include <cairo.h>
#include "search.h"
#include "osm-gps-map/osm-gps-map.h"
#include "osm-gps-map/layer-wiki.h"
#include "osm-gps-map/osm-gps-map-osd-classic.h"

namespace Maep {

class GpsMap : public QQuickPaintedItem
{
    Q_OBJECT

 public:
  GpsMap(QQuickItem *parent = 0);
  ~GpsMap();

 protected:
  void paint(QPainter *painter);
  void keyPressEvent(QKeyEvent * event);
  void mousePressEvent(QMouseEvent *event);
  void mouseReleaseEvent(QMouseEvent *event);
  void mouseMoveEvent(QMouseEvent *event);

 signals:
  void wikiURLSelected(const QString &title, const QString &url);
  void searchRequest();

 public slots:
  void setWikiURL(const char *title, const char *url);
  void setSearchRequest(const QString &request);

 private:
  OsmGpsMap *map;
  MaepWikiContext *wiki;
  MaepSearchContext *search;
  osm_gps_map_osd_t *osd;
  gboolean dragging;
  int drag_start_mouse_x, drag_start_mouse_y;
  int drag_mouse_dx, drag_mouse_dy;

  /* Wiki entry. */
  QString wiki_title, wiki_url;

  /* Printing. */
  cairo_surface_t *surf;
  cairo_t *cr;
  QImage *img;
};

}

#endif
