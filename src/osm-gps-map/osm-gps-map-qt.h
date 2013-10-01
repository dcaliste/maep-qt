#ifndef OSM_GPS_MAP_QT_H
#define OSM_GPS_MAP_QT_H

#include <QQuickPaintedItem>
#include "osm-gps-map/osm-gps-map.h"

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
  int drag_start_mouse_x, drag_start_mouse_y;
  int drag_mouse_dx, drag_mouse_dy;

};

}

#endif
