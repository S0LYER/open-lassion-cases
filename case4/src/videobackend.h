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
  void stopVideo();
  void restartVideo();

  void setConfidenceThreshold(double val);
  void resetStats();
  void exportCSV();
  void setVideoPosition(int frameIdx);

signals:
  void frameReady(const QImage &frame);
  void videoEnded();
  void statusMessage(const QString &msg);
  void playbackStopped();

  void statsUpdated(double avgWorkers, double avgHats, double avgNoHats,
                    double avgVests, double avgNoVests, int processedFrames);
  void videoOpened(int totalFrames);
  void framePositionChanged(int currentFrame);

private slots:
  void processFrame();

private:
  void processSingleFrame(const cv::Mat &sourceFrame);

  QString m_videoPath;
  bool m_isModelLoaded;
  bool m_isPhotoMode;
  double m_confThreshold;

  cv::dnn::Net m_net;
  cv::VideoCapture m_cap;
  cv::Mat m_currentPhotoMat;
  QTimer m_timer;

  double m_accumWorkers;
  double m_accumHats;
  double m_accumNoHats;
  double m_accumVests;
  double m_accumNoVests;
  int m_processedFrames;

  int m_frameCount;
  std::vector<cv::Rect> lastBoxes;
  std::vector<int> lastClassIds;
  std::vector<float> lastConfidences;
};
