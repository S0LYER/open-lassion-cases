#pragma once
#include <QImage>
#include <QObject>
#include <QString>
#include <QTimer>
#include <opencv2/dnn.hpp>
#include <opencv2/opencv.hpp>
#include <vector>

struct PassengerTrack {
  int id;
  cv::Rect box;
  std::vector<cv::Point> path;
  int age;
  bool counted;
};

class VideoBackend : public QObject {
  Q_OBJECT
public:
  explicit VideoBackend(QObject *parent = nullptr);
  ~VideoBackend();

public slots:
  void loadModel(const QString &pathCfg);
  void loadWeights(const QString &pathWeights);
  void loadVideo(const QString &path);
  void pauseVideo();
  void resumeVideo();
  void restartVideo();
  void stopVideo();

  void setMode(int mode);
  void resetStats();
  void setVideoPosition(int frameIdx);

signals:
  void frameReady(const QImage &frame);
  void videoEnded();
  void statusMessage(const QString &msg);
  void statsUpdated(int inCount, int outCount, int insideCount);
  void videoOpened(int totalFrames);
  void framePositionChanged(int currentFrame);

private slots:
  void processFrame();

private:
  QString m_videoPath;
  QString m_cfgPath;
  QString m_weightsPath;
  bool m_isModelLoaded;

  cv::dnn::Net m_net;
  cv::VideoCapture m_cap;
  QTimer m_timer;

  int m_currentMode;
  int m_frameCount;
  int m_nextPassengerId;

  int m_countIn;
  int m_countOut;

  std::vector<PassengerTrack> m_tracks;
};
