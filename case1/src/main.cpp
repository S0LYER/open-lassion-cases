#include "videobackend.h"
#include "videoitem.h"
#include <QApplication> // QApplication обязателен для cv::imshow и Widgets
#include <QMetaType>
#include <QQmlApplicationEngine>
#include <QQmlContext>

int main(int argc, char *argv[]) {
  // Force software rendering (for wayland-bugs execution)
  qputenv("QT_QUICK_BACKEND", "software");

  // Hyprland/Wayland settings
  qputenv("QT_QPA_PLATFORM", "wayland;xcb");

  QApplication app(argc, argv);

  qRegisterMetaType<QImage>("QImage");

  VideoBackend backend;
  VideoImageProvider *provider = new VideoImageProvider();

  QQmlApplicationEngine engine;

  // Image provider registration with name "video"
  engine.addImageProvider("video", provider);

  engine.rootContext()->setContextProperty("videoBackend", &backend);

  // Backend signal connection with frames
  QObject::connect(
      &backend, &VideoBackend::frameReady,
      [provider](const QImage &frame) { provider->updateFrame(frame); });

  engine.load(QUrl("qrc:/App/qml/Main.qml"));
  if (engine.rootObjects().isEmpty())
    return -1;

  return app.exec();
}
