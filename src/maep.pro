TEMPLATE = app
TARGET = harbour-maep-qt
DEPENDPATH += .
INCLUDEPATH += .
CONFIG += link_pkgconfig
PKGCONFIG += gobject-2.0 cairo libsoup-2.4 gconf-2.0 libxml-2.0 libcurl
QT += qml quick positioning #declarative
LIBS += -ljpeg

packagesExist(qdeclarative5-boostable) {
  DEFINES += HAS_BOOSTER
  PKGCONFIG += qdeclarative5-boostable
} else {
  warning("qdeclarative-boostable not available; startup times will be slower")
}

isEmpty(PREFIX)
{
  PREFIX = /usr
}

DEPLOYMENT_PATH = $$PREFIX/share/$$TARGET
DEFINES += DEPLOYMENT_PATH=\"\\\"\"$${DEPLOYMENT_PATH}\"\\\"\"
DEFINES += APP=\"\\\"\"maep\"\\\"\"
DEFINES += DATADIR=\"\\\"\"$${DEPLOYMENT_PATH}\"\\\"\"
DEFINES += SAILFISH
DEFINES += VERSION=\"\\\"\"1.3.7\"\\\"\"

# Input
HEADERS += misc.h net_io.h geonames.h search.h track.h img_loader.h icon.h converter.h osm-gps-map/osm-gps-map.h osm-gps-map/osm-gps-map-layer.h osm-gps-map/osm-gps-map-qt.h osm-gps-map/osm-gps-map-osd-classic.h osm-gps-map/layer-wiki.h
SOURCES += misc.c net_io.c geonames.c search.c track.c img_loader.c icon.c converter.c osm-gps-map/osm-gps-map.c osm-gps-map/osm-gps-map-layer.c osm-gps-map/osm-gps-map-qt.cpp osm-gps-map/osm-gps-map-osd-classic.c osm-gps-map/layer-wiki.c main.cpp

# For Harbour restrictions
QMAKE_RPATHDIR = $$DEPLOYMENT_PATH/lib

# Installation
target.path = $$PREFIX/bin

desktop.path = $$PREFIX/share/applications
desktop.files = ../data/harbour-maep-qt.desktop

icon.path = $$PREFIX/share/icons/hicolor/86x86/apps
icon.files = ../data/harbour-maep-qt.png

qml.path = $$DEPLOYMENT_PATH
qml.files = ../src/main.qml ../src/Header.qml ../src/TrackView.qml ../src/About.qml ../src/Settings.qml ../src/FileChooser.qml ../src/Notification.qml

resources.path = $$DEPLOYMENT_PATH
resources.files = ../data/wikipedia_w.48.png ../data/AUTHORS ../data/COPYING

INSTALLS += target desktop icon qml resources
