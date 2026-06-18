#pragma once
#include <QImage>
#include <QObject>
#include <QString>
#include <QTimer>
#include <opencv2/dnn.hpp>
#include <opencv2/opencv.hpp>

class VideoBackend : public QObject {
  Q_OBJECT
public:
  explicit VideoBackend(QObject *parent = nullptr);
  ~VideoBackend();

public slots:
  void loadModel(const QString &path);
  void loadVideo(const QString &path);
  void loadPhoto(const QString &path);
  void startCamera();
  void pauseVideo();
  void resumeVideo();
  void restartVideo();
  void stopVideo();

  void setConfidenceThreshold(double val);
  void resetStats();
  void exportCSV();
  void setVideoPosition(int frameIdx);

signals:
  void frameReady(const QImage &frame);
  void videoEnded();
  void statusMessage(const QString &msg);

  // Signal with stats counter
  void statsUpdated(int currCrit, int currCracks, int currWear, int currInfra,
                    int accCrit, int accCracks, int accWear, int accInfra,
                    int processedFrames);

  void videoOpened(int totalFrames);
  void framePositionChanged(int currentFrame);

private slots:
  void processFrame();

private:
  void processSingleFrame(const cv::Mat &sourceFrame, bool updateStats = true);

  QString m_videoPath;
  bool m_isModelLoaded;
  bool m_isPhotoMode;
  double m_confThreshold;

  cv::dnn::Net m_net;
  cv::VideoCapture m_cap;
  cv::Mat m_lastRawFrame;
  QTimer m_timer;

  int m_accumCritical;
  int m_accumCracks;
  int m_accumWear;
  int m_accumInfra;
  int m_processedFramesCounter;

  int m_frameCount;
  std::vector<cv::Rect> lastBoxes;
  std::vector<int> lastClassIds;
  std::vector<float> lastConfidences;
};
