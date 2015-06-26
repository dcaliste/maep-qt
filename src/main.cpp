#include "osm-gps-map/osm-gps-map-qt.h"
#include "../qmlLibs/notification.h"
#include "../qmlLibs/locationvaluetypeprovider.h"
#include "../qmlLibs/qquickfolderlistmodel.h"

#include <QGuiApplication>
#ifdef HAS_BOOSTER
#include <MDeclarativeCache>
#endif
#if (QT_VERSION >= QT_VERSION_CHECK(5, 0, 0))
#include <QDir>
#include <QQmlEngine>
#include <QQuickView>
#include <QQuickItem>
#include <QQuickWindow>
#else
#include <QtDeclarative>
#include <QDeclarativeView>
#define QQuickView QDeclarativeView
#endif

namespace Maep {
  QGuiApplication *createApplication(int &argc, char **argv);
  QQuickView *createView(const QString &file);
  void showView(QQuickView* view);
}

QGuiApplication *Maep::createApplication(int &argc, char **argv)
{
#ifdef HAS_BOOSTER
    return MDeclarativeCache::qApplication(argc, argv);
#else
    return new QGuiApplication(argc, argv);
#endif
}
QQuickView *Maep::createView(const QString &file)
{
    QQuickView *view;
#ifdef HAS_BOOSTER
    view = MDeclarativeCache::qQuickView();
#else
    view = new QQuickView;
#endif
    
    bool isDesktop = qApp->arguments().contains("-desktop");
    
    QString path;

    QQuickWindow::setDefaultAlphaBuffer(true);

    if (isDesktop)
      {
        path = qApp->applicationDirPath() + QDir::separator();
#ifdef DESKTOP
        view->setViewport(new QGLWidget);
#endif
      }
    else
      path = QString(DEPLOYMENT_PATH);
    if(file.contains(":"))
      view->setSource(QUrl(file));
    else
      {
        if(QCoreApplication::applicationFilePath().startsWith("/opt/sdk/"))
          {
            // Quick deployed under /opt/sdk
            // parse the base path from application binary's path and use it as base
            QString basePath = QCoreApplication::applicationFilePath();
            basePath.chop(basePath.length() -  basePath.indexOf("/", 9)); // first index after /opt/sdk/
            view->engine()->addImportPath(basePath + path);
            view->setSource(QUrl::fromLocalFile(basePath + path + file));
          }
        else
          {
            view->engine()->addImportPath(path);
            // Otherwise use deployement path as is
            view->setSource(QUrl::fromLocalFile(path + QDir::separator() + file));
          }
      }
    return view;
}
void Maep::showView(QQuickView* view) {
    view->setResizeMode(QQuickView::SizeRootObjectToView);
    
    bool isDesktop = qApp->arguments().contains("-desktop");
    
    if (isDesktop) {
        view->resize(480, 854);
        view->rootObject()->setProperty("_desktop", true);
        view->show();
    } else {
        view->showFullScreen();
    }
}

Q_DECL_EXPORT int main(int argc, char *argv[])
{
  bool isDesktop;
  LocationValueTypeProvider provider;

  qmlRegisterType<QQuickFolderListModel>("harbour.maep.qt", 1, 0, "FolderListModel");

  qmlRegisterType<Notification>("harbour.maep.qt", 1, 0, "Notification");

  qRegisterMetaType<QGeoCoordinate>("QGeoCoordinate");
  QQml_addValueTypeProvider(&provider);

  qmlRegisterType<Maep::Conf>("harbour.maep.qt", 1, 0, "Conf");
  qmlRegisterType<Maep::GeonamesPlace>("harbour.maep.qt", 1, 0, "GeonamesPlace");
  qmlRegisterType<Maep::GeonamesEntry>("harbour.maep.qt", 1, 0, "GeonamesEntry");
  qmlRegisterType<Maep::Track>("harbour.maep.qt", 1, 0, "Track");
  qmlRegisterType<Maep::GpsMap>("harbour.maep.qt", 1, 0, "GpsMap");
  qmlRegisterType<Maep::GpsMapCover>("harbour.maep.qt", 1, 0, "GpsMapCover");

  QScopedPointer<QGuiApplication> app(Maep::createApplication(argc, argv));
  isDesktop = app->arguments().contains("-desktop");

  QScopedPointer<QQuickView> view(Maep::createView((isDesktop)?"main-nosilica.qml":"main.qml"));
  Maep::showView(view.data());
    
  return app->exec();
}
