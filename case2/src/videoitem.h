#pragma once
#include <QImage>
#include <QMutex>
#include <QQuickImageProvider>

class VideoImageProvider : public QQuickImageProvider {
public:
  VideoImageProvider() : QQuickImageProvider(QQuickImageProvider::Image) {}

  QImage requestImage(const QString &id, QSize *size,
                      const QSize &requestedSize) override {
    Q_UNUSED(requestedSize);
    Q_UNUSED(id);

    QMutexLocker locker(&m_mutex);
    if (m_currentFrame.isNull()) {
      QImage img(1, 1, QImage::Format_ARGB32_Premultiplied);
      img.fill(Qt::black);
      if (size)
        *size = img.size();
      return img;
    }

    if (size) {
      *size = m_currentFrame.size();
    }
    return m_currentFrame.copy();
  }

  void updateFrame(const QImage &image) {
    QMutexLocker locker(&m_mutex);
    m_currentFrame = image;
  }

private:
  QImage m_currentFrame;
  QMutex m_mutex;
};
