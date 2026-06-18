#include "videobackend.h"
#include <QDateTime>
#include <QDebug>
#include <QFile>
#include <QImage>
#include <QTextStream>
#include <QUrl>
#include <algorithm>

VideoBackend::VideoBackend(QObject *parent)
    : QObject(parent), m_isModelLoaded(false), m_isPhotoMode(false),
      m_confThreshold(0.30), m_frameCount(0), m_countFree(0), m_countOcc(0),
      m_countPart(0), m_processedFrames(0) {
  connect(&m_timer, &QTimer::timeout, this, &VideoBackend::processFrame);
}

VideoBackend::~VideoBackend() { stopVideo(); }

void VideoBackend::loadModel(const QString &path) {
  QString localPath = QUrl(path).toLocalFile();
  try {
    m_net = cv::dnn::readNetFromONNX(localPath.toStdString());
    m_net.setPreferableBackend(cv::dnn::DNN_BACKEND_OPENCV);
    m_net.setPreferableTarget(cv::dnn::DNN_TARGET_CPU);
    m_isModelLoaded = true;
    emit statusMessage("ONNX модель парковки успешно загружена!");
  } catch (const cv::Exception &e) {
    emit statusMessage("Ошибка загрузки модели!");
  }
}

void VideoBackend::loadVideo(const QString &path) {
  stopVideo();
  m_videoPath = QUrl(path).toLocalFile();
  m_isPhotoMode = false;

  m_cap.open(m_videoPath.toStdString());
  if (!m_cap.isOpened()) {
    emit statusMessage("Ошибка: Не удалось открыть видео!");
    return;
  }

  m_frameCount = 0;
  lastBoxes.clear();
  lastClassIds.clear();
  lastConfidences.clear();

  int total = m_cap.get(cv::CAP_PROP_FRAME_COUNT);
  emit videoOpened(total);

  emit statusMessage("Мониторинг парковочных мест...");
  m_timer.start(15);
}

void VideoBackend::loadPhoto(const QString &path) {
  stopVideo();
  m_videoPath = QUrl(path).toLocalFile();
  m_isPhotoMode = true;

  m_currentPhotoMat = cv::imread(m_videoPath.toStdString());
  if (m_currentPhotoMat.empty()) {
    emit statusMessage("Ошибка: Не удалось загрузить фото!");
    return;
  }

  emit statusMessage("Анализ парковки по фотографии...");
  m_countFree = 0;
  m_countOcc = 0;
  m_countPart = 0;
  m_processedFrames = 0;

  emit videoOpened(1);

  processSingleFrame(m_currentPhotoMat);
}

void VideoBackend::startCamera() {
  stopVideo();
  m_isPhotoMode = false;
  m_videoPath = "";

  m_cap.open(0);
  if (!m_cap.isOpened()) {
    emit statusMessage("Ошибка: Камера не найдена!");
    return;
  }

  m_frameCount = 0;
  lastBoxes.clear();
  lastClassIds.clear();
  lastConfidences.clear();

  emit videoOpened(9999);

  emit statusMessage("Live-захват с веб-камеры...");
  m_timer.start(15);
}

void VideoBackend::pauseVideo() {
  m_timer.stop();
  emit statusMessage("Пауза");
}

void VideoBackend::resumeVideo() {
  if (m_cap.isOpened()) {
    m_timer.start(15);
    emit statusMessage("Анализ...");
  }
}

void VideoBackend::restartVideo() {
  if (!m_isPhotoMode && !m_videoPath.isEmpty()) {
    loadVideo(QUrl::fromLocalFile(m_videoPath).toString());
  }
}

void VideoBackend::stopVideo() {
  m_timer.stop();
  if (m_cap.isOpened()) {
    m_cap.release();
  }
  m_isPhotoMode = false;
  emit statusMessage("Остановлено");
}

void VideoBackend::setConfidenceThreshold(double val) {
  m_confThreshold = val;
  emit statusMessage(
      QString("Порог уверенности изменен: %1").arg(m_confThreshold));

  if (m_isPhotoMode && !m_currentPhotoMat.empty()) {
    m_countFree = 0;
    m_countOcc = 0;
    m_countPart = 0;
    m_processedFrames = 0;
    processSingleFrame(m_currentPhotoMat);
  }
}

void VideoBackend::setVideoPosition(int frameIdx) {
  if (m_cap.isOpened() && !m_isPhotoMode) {
    bool wasRunning = m_timer.isActive();
    m_timer.stop();

    m_cap.set(cv::CAP_PROP_POS_FRAMES, frameIdx);
    m_frameCount = frameIdx;

    cv::Mat frame;
    m_cap >> frame;
    if (!frame.empty()) {
      processSingleFrame(frame);
    }

    if (wasRunning) {
      m_timer.start(15);
    }
  }
}

void VideoBackend::resetStats() {
  m_countFree = 0;
  m_countOcc = 0;
  m_countPart = 0;
  m_processedFrames = 0;
  emit statsUpdated(0, 0, 0, 0);
  emit statusMessage("Статистика парковки сброшена.");
}

void VideoBackend::exportCSV() {
  QString timestamp = QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss");
  QString filename = QString("parking_statistics_%1.csv").arg(timestamp);

  QFile file;
  file.setFileName(filename);

  bool success = file.open(QIODevice::WriteOnly | QIODevice::Text);

  if (success) {
    QTextStream stream(&file);
    stream << "Parameter,Value\n";
    stream << "Cumulative Free Spaces," << m_countFree << "\n";
    stream << "Cumulative Occupied Spaces," << m_countOcc << "\n";
    stream << "Cumulative Partially Occupied," << m_countPart << "\n";
    stream << "Total Frames Processed," << m_processedFrames << "\n";
    if (m_processedFrames > 0) {
      stream << "Average Free Spaces,"
             << (double)m_countFree / m_processedFrames << "\n";
      stream << "Average Occupied Spaces,"
             << (double)m_countOcc / m_processedFrames << "\n";
      stream << "Average Partially Occupied,"
             << (double)m_countPart / m_processedFrames << "\n";
    }

    file.close();
    emit statusMessage(QString("Отчет выгружен: %1").arg(filename));
  } else {
    emit statusMessage("Ошибка записи отчета!");
  }
}

void VideoBackend::processFrame() {
  if (!m_cap.isOpened())
    return;

  cv::Mat frame;
  m_cap >> frame;
  if (frame.empty()) {
    m_timer.stop();
    m_cap.release();
    emit videoEnded();
    emit statusMessage("Видео обработано");
    return;
  }
  m_frameCount++;

  emit framePositionChanged(m_frameCount);

  processSingleFrame(frame);
}

void VideoBackend::processSingleFrame(const cv::Mat &sourceFrame) {
  cv::Mat frame = sourceFrame.clone();

  std::vector<std::string> parkingClasses = {"free_parking_space",
                                             "not_free_parking_space",
                                             "partially_free_parking_space"};

  auto getColor = [](int classId) {
    if (classId == 0)
      return cv::Scalar(0, 255, 0);
    if (classId == 1)
      return cv::Scalar(255, 0, 0);
    if (classId == 2)
      return cv::Scalar(255, 255, 0);
    return cv::Scalar(0, 255, 0);
  };

  static int numClasses = 3;

  if (m_isModelLoaded) {
    const int detectInterval = 5;
    if (m_frameCount % detectInterval == 1 || m_isPhotoMode) {
      int64 tStart = cv::getTickCount();

      cv::Mat blob;
      cv::dnn::blobFromImage(frame, blob, 1.0 / 255.0, cv::Size(640, 640),
                             cv::Scalar(), true, false);
      m_net.setInput(blob);

      std::vector<cv::Mat> outputs;
      m_net.forward(outputs, m_net.getUnconnectedOutLayersNames());

      cv::Mat out = outputs[0];
      int dimensions = out.size[1];
      int rows = out.size[2];
      numClasses = dimensions - 4;

      cv::Mat reshaped = out.reshape(1, dimensions);
      cv::Mat transposed;
      cv::transpose(reshaped, transposed);

      float x_factor = frame.cols / 640.0f;
      float y_factor = frame.rows / 640.0f;

      lastBoxes.clear();
      lastClassIds.clear();
      lastConfidences.clear();

      for (int i = 0; i < rows; ++i) {
        cv::Mat scores = transposed.row(i).colRange(4, dimensions);
        cv::Point classIdPoint;
        double maxClassScore;

        cv::minMaxLoc(scores, 0, &maxClassScore, 0, &classIdPoint);

        if (maxClassScore > m_confThreshold) {
          float cx = transposed.at<float>(i, 0);
          float cy = transposed.at<float>(i, 1);
          float w = transposed.at<float>(i, 2);
          float h = transposed.at<float>(i, 3);

          int left = int((cx - 0.5 * w) * x_factor);
          int top = int((cy - 0.5 * h) * y_factor);
          int width = int(w * x_factor);
          int height = int(h * y_factor);

          double area = width * height;
          double aspect =
              std::max(width, height) / (double)std::min(width, height);

          if (area < 800 || area > 60000)
            continue;

          if (aspect < 1.5 && area < 5000) {
            continue;
          }

          lastConfidences.push_back(maxClassScore);
          lastClassIds.push_back(classIdPoint.x);
          lastBoxes.push_back(cv::Rect(left, top, width, height));
        }
      }

      double timeMs =
          (cv::getTickCount() - tStart) * 1000.0 / cv::getTickFrequency();
      qDebug() << "[YOLO ИИ Парковка] Кадр обработан за:" << timeMs << "мс";
    }
  }

  int frameFree = 0;
  int frameOcc = 0;
  int framePart = 0;

  for (size_t i = 0; i < lastBoxes.size(); ++i) {
    cv::Rect box = lastBoxes[i];
    int classId = lastClassIds[i];
    float conf = lastConfidences[i];

    cv::Scalar color = getColor(classId);
    cv::rectangle(frame, box, color, 3);

    std::string labelName = (classId < parkingClasses.size())
                                ? parkingClasses[classId]
                                : "Space_" + std::to_string(classId);
    std::string label = labelName + " " + std::to_string(int(conf * 100)) + "%";
    cv::putText(frame, label, cv::Point(box.x, box.y - 10),
                cv::FONT_HERSHEY_SIMPLEX, 0.6, color, 2);

    if (classId == 0)
      frameFree++;
    else if (classId == 1)
      frameOcc++;
    else if (classId == 2)
      framePart++;
  }

  m_countFree += frameFree;
  m_countOcc += frameOcc;
  m_countPart += framePart;
  m_processedFrames++;

  emit statsUpdated(m_countFree, m_countOcc, m_countPart, m_processedFrames);

  cv::Mat rgbFrame;
  cv::cvtColor(frame, rgbFrame, cv::COLOR_BGR2RGB);
  QImage qimg((const uchar *)rgbFrame.data, rgbFrame.cols, rgbFrame.rows,
              rgbFrame.step, QImage::Format_RGB888);

  QImage deepCopyFrame = qimg.rgbSwapped();
  emit frameReady(deepCopyFrame.convertToFormat(QImage::Format_RGB32));
}
