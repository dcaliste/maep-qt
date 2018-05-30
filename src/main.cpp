#include "osm-gps-map/sourcemodel.h"
#include "osm-gps-map/osm-gps-map-qt.h"
#include "../qmlLibs/qquickfolderlistmodel.h"

#include <QtCore/QTranslator>
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
  QQuickView *createView();
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
QQuickView *Maep::createView()
{
    QQuickView *view;
#ifdef HAS_BOOSTER
    view = MDeclarativeCache::qQuickView();
#else
    view = new QQuickView;
#endif
    
    QString path, file;

    QQuickWindow::setDefaultAlphaBuffer(true);

    if (qApp->arguments().contains("-desktop"))
      {
        path = qApp->applicationDirPath() + QDir::separator();
#ifdef DESKTOP
        view->setViewport(new QGLWidget);
#endif
        file = QStringLiteral("main-nosilica.qml");
      }
    else
      {
        path = QString(DEPLOYMENT_PATH);
        file = QStringLiteral("main.qml");
      }
    if(QCoreApplication::applicationFilePath().startsWith("/opt/sdk/"))
      {
        // Quick deployed under /opt/sdk
        // parse the base path from application binary's path and use it as base
        QString basePath = QCoreApplication::applicationFilePath();
        basePath.chop(basePath.length() -  basePath.indexOf("/", 9)); // first index after /opt/sdk/
        view->engine()->addImportPath(basePath + path);
        if (!path.endsWith("/"))
          path += "/";
        view->setSource(QUrl::fromLocalFile(basePath + path + file));
      }
    else
      {
         view->engine()->addImportPath(path);
         // Otherwise use deployement path as is
         view->setSource(QUrl::fromLocalFile(path + QDir::separator() + file));
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
  qmlRegisterType<QQuickFolderListModel>("harbour.maep.qt", 1, 0, "FolderListModel");

  qmlRegisterType<Maep::Conf>("harbour.maep.qt", 1, 0, "Conf");
  qmlRegisterType<Maep::GeonamesPlace>("harbour.maep.qt", 1, 0, "GeonamesPlace");
  qmlRegisterType<Maep::GeonamesEntry>("harbour.maep.qt", 1, 0, "GeonamesEntry");
  qmlRegisterType<Maep::Track>("harbour.maep.qt", 1, 0, "Track");
  qmlRegisterType<Maep::GpsMap>("harbour.maep.qt", 1, 0, "GpsMap");
  qmlRegisterType<Maep::GpsMapCover>("harbour.maep.qt", 1, 0, "GpsMapCover");
  qmlRegisterType<Maep::SourceModel>("harbour.maep.qt", 1, 0, "SourceModel");
  qmlRegisterType<Maep::SourceModelFilter>("harbour.maep.qt", 1, 0, "SourceModelFilter");

  QScopedPointer<QGuiApplication> app(Maep::createApplication(argc, argv));
  QTranslator translator;
  if (translator.load(QLocale::system().name(),
                      DEPLOYMENT_PATH "/translations"))
      QGuiApplication::installTranslator(&translator);

  QScopedPointer<QQuickView> view(Maep::createView());
  Maep::showView(view.data());
    
  return app->exec();
}
