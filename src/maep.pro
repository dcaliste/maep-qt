TEMPLATE = app
TARGET = maep-qt
DEPENDPATH += .
INCLUDEPATH += .
CONFIG += link_pkgconfig
PKGCONFIG += gobject-2.0 cairo libsoup-2.4 gconf-2.0
QT += quick widgets #declarative

DEPLOYMENT_PATH = /usr/share/$$TARGET
DEFINES += DEPLOYMENT_PATH=\"\\\"\"$${DEPLOYMENT_PATH}/\"\\\"\"
DEFINES += APP=\"\\\"\"maep\"\\\"\"
DEFINES += DATADIR=\"\\\"\"$${DEPLOYMENT_PATH}/\"\\\"\"

# Input
HEADERS += misc.h converter.h osm-gps-map/osm-gps-map.h osm-gps-map/osm-gps-map-layer.h osm-gps-map/osm-gps-map-qt.h
SOURCES += misc.c converter.c osm-gps-map/osm-gps-map.c osm-gps-map/osm-gps-map-layer.c osm-gps-map/osm-gps-map-qt.cpp main.cpp
