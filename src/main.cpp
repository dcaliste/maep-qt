#include "osm-gps-map/osm-gps-map-qt.h"

#include <QApplication>
#if (QT_VERSION >= QT_VERSION_CHECK(5, 0, 0))
#include <QDir>
#include <QQmlEngine>
#include <QQuickView>
#include <QQuickItem>
#else
#include <QtDeclarative>
#include <QDeclarativeView>
#define QQuickView QDeclarativeView
#endif

namespace Maep {
  QApplication *createApplication(int &argc, char **argv);
  QQuickView *createView(const QString &file);
  void showView(QQuickView* view);
}

QApplication *Maep::createApplication(int &argc, char **argv)
{
#ifdef HAS_BOOSTER
    return MDeclarativeCache::qApplication(argc, argv);
#else
    return new QApplication(argc, argv);
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
            view->setSource(QUrl::fromLocalFile(basePath + path + file));
          }
        else
          // Otherwise use deployement path as is
          view->setSource(QUrl::fromLocalFile(path + file));
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
  qmlRegisterType<Maep::GpsMap>("Maep", 1, 0, "GpsMap");

  QScopedPointer<QApplication> app(Maep::createApplication(argc, argv));
  QScopedPointer<QQuickView> view(Maep::createView("main.qml"));
  Maep::showView(view.data());
    
  return app->exec();
}
