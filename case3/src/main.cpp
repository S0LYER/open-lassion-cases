#include "videobackend.h"
#include "videoitem.h"
#include <QApplication>
#include <QMetaType>
#include <QQmlApplicationEngine>
#include <QQmlContext>

int main(int argc, char *argv[]) {
  qputenv("QT_QUICK_BACKEND", "software");
  qputenv("QT_QPA_PLATFORM", "wayland;xcb");

  QApplication app(argc, argv);

  qRegisterMetaType<QImage>("QImage");

  VideoBackend backend;
  VideoImageProvider *provider = new VideoImageProvider();

  QQmlApplicationEngine engine;
  engine.addImageProvider("video", provider);
  engine.rootContext()->setContextProperty("videoBackend", &backend);

  QObject::connect(
      &backend, &VideoBackend::frameReady,
      [provider](const QImage &frame) { provider->updateFrame(frame); });

  engine.load(QUrl("qrc:/App/qml/Main.qml"));
  if (engine.rootObjects().isEmpty())
    return -1;

  return app.exec();
}
