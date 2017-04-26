TEMPLATE = app
TARGET = harbour-maep-qt
DEPENDPATH += .
INCLUDEPATH += .
CONFIG += link_pkgconfig hide_symbols
PKGCONFIG += gobject-2.0 cairo libsoup-2.4 dconf libxml-2.0 libcurl
QT += qml quick positioning sensors
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
DEFINES += APP=\"\\\"\"$${TARGET}\"\\\"\"
DEFINES += DATADIR=\"\\\"\"$${DEPLOYMENT_PATH}\"\\\"\"
DEFINES += SAILFISH
DEFINES += VERSION=\"\\\"\"1.4.5\"\\\"\"

# Input
HEADERS += src/config.h src/misc.h src/conf.h src/net_io.h src/geonames.h src/search.h src/track.h src/img_loader.h src/icon.h src/converter.h src/osm-gps-map/osm-gps-map.h src/osm-gps-map/osm-gps-map-layer.h src/osm-gps-map/osm-gps-map-qt.h src/osm-gps-map/osm-gps-map-osd-classic.h src/osm-gps-map/layer-wiki.h src/osm-gps-map/layer-gps.h src/osm-gps-map/source.h
SOURCES += src/misc.c src/conf.c src/net_io.c src/geonames.c src/search.c src/track.c src/img_loader.c src/icon.c src/converter.c src/osm-gps-map/osm-gps-map.c src/osm-gps-map/osm-gps-map-layer.c src/osm-gps-map/osm-gps-map-qt.cpp src/osm-gps-map/osm-gps-map-osd-classic.c src/osm-gps-map/layer-wiki.c src/osm-gps-map/layer-gps.c src/osm-gps-map/source.c src/main.cpp

# Installation
target.path = $$PREFIX/bin

desktop.path = $$PREFIX/share/applications
desktop.files = data/harbour-maep-qt.desktop

icon86.path = $$PREFIX/share/icons/hicolor/86x86/apps
icon86.files = data/86x86/harbour-maep-qt.png
icon108.path = $$PREFIX/share/icons/hicolor/108x108/apps
icon108.files = data/108x108/harbour-maep-qt.png
icon128.path = $$PREFIX/share/icons/hicolor/128x128/apps
icon128.files = data/128x128/harbour-maep-qt.png
icon256.path = $$PREFIX/share/icons/hicolor/256x256/apps
icon256.files = data/256x256/harbour-maep-qt.png

qml.path = $$DEPLOYMENT_PATH
qml.files = src/main.qml src/Header.qml src/PlaceHeader.qml src/TrackHeader.qml src/TrackView.qml src/Sources.qml src/About.qml src/Settings.qml src/FileChooser.qml

resources.path = $$DEPLOYMENT_PATH
resources.files = data/wikipedia_w.48.png data/icon-camera-zoom-wide.png data/icon-camera-zoom-tele.png data/icon-cover-remove.png data/AUTHORS data/COPYING

INSTALLS += target desktop icon86 icon108 icon128 icon256 qml resources

OTHER_FILES += rpm/maep-qt.spec

# This part is to circumvent harbour limitations.
QMAKE_RPATHDIR = $$DEPLOYMENT_PATH/lib

QT += qml-private core-private
SOURCES += qmlLibs/qquickfolderlistmodel.cpp qmlLibs/fileinfothread.cpp
HEADERS += qmlLibs/qquickfolderlistmodel.h qmlLibs/fileproperty_p.h qmlLibs/fileinfothread_p.h
