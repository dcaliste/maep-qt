#ifndef OSM_GPS_MAP_QT_H
#define OSM_GPS_MAP_QT_H

#include <QQuickPaintedItem>
#include <QImage>
#include <cairo.h>
#include "osm-gps-map/osm-gps-map.h"
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

 private:
  OsmGpsMap *map;
  osm_gps_map_osd_t *osd;
  int drag_start_mouse_x, drag_start_mouse_y;
  int drag_mouse_dx, drag_mouse_dy;

  /* Printing. */
  cairo_surface_t *surf;
  cairo_t *cr;
  QImage *img;
};

}

#endif
