#pragma once
#include <QImage>
#include <QObject>
#include <QString>
#include <QTimer>
#include <deque>
#include <opencv2/opencv.hpp>
#include <opencv2/video/background_segm.hpp>

class VideoBackend : public QObject {
  Q_OBJECT
public:
  explicit VideoBackend(QObject *parent = nullptr);
  ~VideoBackend();

public slots:
  void loadVideo(const QString &path);
  void pauseVideo();
  void resumeVideo();
  void stopVideo();
  void restartVideo();
  void setVideoPosition(int frameIdx);

signals:
  void frameReady(const QImage &frame);
  void videoEnded();
  void statusMessage(const QString &msg);
  void logMessage(const QString &msg);
  void videoOpened(int totalFrames);
  void framePositionChanged(int currentFrame);

private slots:
  void processFrame();

private:
  void processSingleFrame(const cv::Mat &sourceFrame);
  double calculateLaplacianVariance(const cv::Mat &image);

  QString m_videoPath;
  cv::VideoCapture m_cap;
  QTimer m_timer;
  int m_frameCount;
  bool m_isPhotoMode;

  cv::Ptr<cv::BackgroundSubtractorMOG2> m_mog2;
  std::deque<double> m_varianceHistory;
};
