#pragma once
#include <QImage>
#include <QObject>
#include <QString>
#include <QTimer>
#include <deque>
#include <opencv2/opencv.hpp>

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

  void setAlarmThreshold(double val);
  void setVideoPosition(int frameIdx);
  void setReferenceArea(double nx, double ny, double nw, double nh);

signals:
  void frameReady(const QImage &frame);
  void videoEnded();
  void statusMessage(const QString &msg);
  void logMessage(const QString &msg);
  void alarmStatusUpdated(bool isAlarm, double currentDensity);
  void videoOpened(int totalFrames);
  void framePositionChanged(int currentFrame);

private slots:
  void processFrame();

private:
  void processSingleFrame(const cv::Mat &sourceFrame);

  QString m_videoPath;
  cv::VideoCapture m_cap;
  QTimer m_timer;
  int m_frameCount;

  cv::Mat m_lastRawFrame;

  bool m_hasReference;
  cv::Scalar m_hsvMean;
  cv::Scalar m_hsvStd;
  double m_alarmThreshold;
  std::deque<double> m_densityHistory;
};
