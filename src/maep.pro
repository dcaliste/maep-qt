TEMPLATE = app
TARGET = maep-qt
DEPENDPATH += .
INCLUDEPATH += .
CONFIG += link_pkgconfig
PKGCONFIG += gobject-2.0 cairo libsoup-2.4 gconf-2.0 libxml-2.0 libcurl
QT += quick positioning widgets #declarative

DEPLOYMENT_PATH = /usr/share/$$TARGET
DEFINES += DEPLOYMENT_PATH=\"\\\"\"$${DEPLOYMENT_PATH}/\"\\\"\"
DEFINES += APP=\"\\\"\"maep\"\\\"\"
DEFINES += DATADIR=\"\\\"\"$${DEPLOYMENT_PATH}/\"\\\"\"
DEFINES += SAILFISH
DEFINES += VERSION=\"\\\"\"1.3.7\"\\\"\"

# Input
HEADERS += misc.h net_io.h geonames.h search.h track.h icon.h converter.h osm-gps-map/osm-gps-map.h osm-gps-map/osm-gps-map-layer.h osm-gps-map/osm-gps-map-qt.h osm-gps-map/osm-gps-map-osd-classic.h osm-gps-map/layer-wiki.h
SOURCES += misc.c net_io.c geonames.c search.c track.c icon.c converter.c osm-gps-map/osm-gps-map.c osm-gps-map/osm-gps-map-layer.c osm-gps-map/osm-gps-map-qt.cpp osm-gps-map/osm-gps-map-osd-classic.c osm-gps-map/layer-wiki.c main.cpp
