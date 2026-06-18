#include "videobackend.h"
#include <QDateTime>
#include <QDebug>
#include <QFile>
#include <QTextStream>
#include <QUrl>
#include <algorithm>

VideoBackend::VideoBackend(QObject *parent)
    : QObject(parent), m_isModelLoaded(false), m_isPhotoMode(false),
      m_confThreshold(0.25), m_frameCount(0), m_accumCritical(0),
      m_accumCracks(0), m_accumWear(0), m_accumInfra(0),
      m_processedFramesCounter(0) {
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
    emit statusMessage("Модель RDD2022 успешно загружена!");
  } catch (const cv::Exception &e) {
    emit statusMessage("Ошибка загрузки ONNX модели!");
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

  emit statusMessage("Анализ видео...");
  m_timer.start(15);
}

void VideoBackend::loadPhoto(const QString &path) {
  stopVideo();
  m_videoPath = QUrl(path).toLocalFile();
  m_isPhotoMode = true;

  m_lastRawFrame = cv::imread(m_videoPath.toStdString());
  if (m_lastRawFrame.empty()) {
    emit statusMessage("Ошибка: Не удалось загрузить фото!");
    return;
  }

  emit statusMessage("Анализ статической фотографии...");
  emit videoOpened(1);

  resetStats();
  processSingleFrame(m_lastRawFrame, true);
}

void VideoBackend::startCamera() {
  stopVideo();
  m_isPhotoMode = false;
  m_videoPath = "";

  m_cap.open(0);
  if (!m_cap.isOpened()) {
    emit statusMessage("Ошибка: Веб-камера не найдена!");
    return;
  }

  m_frameCount = 0;
  lastBoxes.clear();
  lastClassIds.clear();
  lastConfidences.clear();

  emit videoOpened(99999);

  emit statusMessage("Захват видеопотока с веб-камеры...");
  m_timer.start(15);
}

void VideoBackend::pauseVideo() {
  m_timer.stop();
  emit statusMessage("Анализ приостановлен (Пауза)");
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
  emit statusMessage(QString("Порог уверенности: %1").arg(m_confThreshold));

  // If photo or video pause - using number from slider
  if (!m_timer.isActive() && !m_lastRawFrame.empty()) {
    processSingleFrame(m_lastRawFrame,
                       false); // false = unplus stat
  }
}

void VideoBackend::setVideoPosition(int frameIdx) {
  if (m_cap.isOpened() && !m_isPhotoMode) {
    bool wasRunning = m_timer.isActive();
    m_timer.stop();

    m_cap.set(cv::CAP_PROP_POS_FRAMES, frameIdx);
    m_frameCount = frameIdx;

    m_cap >> m_lastRawFrame;
    if (!m_lastRawFrame.empty()) {
      processSingleFrame(m_lastRawFrame, false);
    }

    if (wasRunning)
      m_timer.start(15);
  }
}

void VideoBackend::resetStats() {
  m_accumCritical = 0;
  m_accumCracks = 0;
  m_accumWear = 0;
  m_accumInfra = 0;
  m_processedFramesCounter = 0;
  emit statsUpdated(0, 0, 0, 0, 0, 0, 0, 0, 0);
  emit statusMessage("Статистика дефектов успешно сброшена.");
}

void VideoBackend::exportCSV() {
  QString timestamp = QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss");
  QString filename = QString("rdd_statistics_%1.csv").arg(timestamp);

  QFile file;
  file.setFileName(filename);

  bool success = file.open(QIODevice::WriteOnly | QIODevice::Text);
  if (success) {
    QTextStream stream(&file);
    stream << "Category,TotalDetections\n";
    stream << "Critical Defects," << m_accumCritical << "\n";
    stream << "Cracks," << m_accumCracks << "\n";
    stream << "Wear," << m_accumWear << "\n";
    stream << "Infrastructure," << m_accumInfra << "\n";

    file.close();
    emit statusMessage(QString("Статистика экспортирована: %1").arg(filename));
  } else {
    emit statusMessage("Ошибка: Не удалось создать файл статистики!");
  }
}

void VideoBackend::processFrame() {
  if (!m_cap.isOpened())
    return;

  m_cap >> m_lastRawFrame;
  if (m_lastRawFrame.empty()) {
    m_timer.stop();
    m_cap.release();
    emit videoEnded();
    emit statusMessage("Видео успешно обработано полностью");
    return;
  }
  m_frameCount++;

  emit framePositionChanged(m_frameCount);

  processSingleFrame(m_lastRawFrame, true);
}

void VideoBackend::processSingleFrame(const cv::Mat &sourceFrame,
                                      bool updateStats) {
  cv::Mat frame = sourceFrame.clone();

  // RDD2022 standart
  std::vector<std::string> rddClasses = {
      "D00 (Longitudinal Crack)", // 0
      "D01 (Transverse Crack)",   // 1
      "D10 (Alligator Crack)",    // 2
      "D11 (Rutting/Pothole)",    // 3
      "D20 (Pothole)",            // 4
      "D40 (Patching_pothole)",   // 5
      "D43 (Blurry marking)",     // 6
      "D44 (Blurry line)",        // 7
      "D50 (Manhole)",            // 8
      "D0w0 (Wheel rut)"          // 9
  };

  auto getColor = [](int classId) {
    if (classId == 2 || classId == 3 || classId == 4)
      return cv::Scalar(0, 0, 255);
    if (classId == 0 || classId == 1)
      return cv::Scalar(0, 165, 255);
    if (classId == 6 || classId == 7 || classId == 9)
      return cv::Scalar(0, 255, 255);
    if (classId == 5 || classId == 8)
      return cv::Scalar(255, 0, 0);
    return cv::Scalar(0, 255, 0);
  };

  if (m_isModelLoaded) {
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

      // Use force number from slider
      if (maxClassScore > m_confThreshold) {
        lastConfidences.push_back(maxClassScore);
        lastClassIds.push_back(classIdPoint.x);

        float cx = transposed.at<float>(i, 0);
        float cy = transposed.at<float>(i, 1);
        float w = transposed.at<float>(i, 2);
        float h = transposed.at<float>(i, 3);

        int left = int((cx - 0.5 * w) * x_factor);
        int top = int((cy - 0.5 * h) * y_factor);
        int width = int(w * x_factor);
        int height = int(h * y_factor);
        lastBoxes.push_back(cv::Rect(left, top, width, height));
      }
    }

    // Hard NMS: 0.45
    std::vector<int> indices;
    cv::dnn::NMSBoxes(lastBoxes, lastConfidences, m_confThreshold, 0.45f,
                      indices);

    std::vector<cv::Rect> filteredBoxes;
    std::vector<int> filteredClassIds;
    std::vector<float> filteredConfidences;

    for (int idx : indices) {
      filteredBoxes.push_back(lastBoxes[idx]);
      filteredClassIds.push_back(lastClassIds[idx]);
      filteredConfidences.push_back(lastConfidences[idx]);
    }

    lastBoxes = filteredBoxes;
    lastClassIds = filteredClassIds;
    lastConfidences = filteredConfidences;

    double timeMs =
        (cv::getTickCount() - tStart) * 1000.0 / cv::getTickFrequency();
    qDebug() << "[YOLO ИИ RDD] Кадр обработан за:" << timeMs
             << "мс. Найдено дефектов:" << lastBoxes.size();
  }

  int currCrit = 0, currCracks = 0, currWear = 0, currInfra = 0;

  for (size_t i = 0; i < lastBoxes.size(); ++i) {
    cv::Rect box = lastBoxes[i];
    int classId = lastClassIds[i];
    float conf = lastConfidences[i];

    cv::Scalar color = getColor(classId);
    cv::rectangle(frame, box, color, 3);

    std::string labelName = (classId < rddClasses.size())
                                ? rddClasses[classId]
                                : "Obj_" + std::to_string(classId);
    std::string label = labelName + " " + std::to_string(int(conf * 100)) + "%";
    cv::putText(frame, label, cv::Point(box.x, box.y - 10),
                cv::FONT_HERSHEY_SIMPLEX, 0.7, color, 2);

    if (classId == 2 || classId == 3 || classId == 4)
      currCrit++;
    else if (classId == 0 || classId == 1)
      currCracks++;
    else if (classId == 6 || classId == 7 || classId == 9)
      currWear++;
    else if (classId == 5 || classId == 8)
      currInfra++;
  }

  // Stats
  if (updateStats) {
    m_accumCritical += currCrit;
    m_accumCracks += currCracks;
    m_accumWear += currWear;
    m_accumInfra += currInfra;
    m_processedFramesCounter++;
  }

  emit statsUpdated(currCrit, currCracks, currWear, currInfra, m_accumCritical,
                    m_accumCracks, m_accumWear, m_accumInfra,
                    m_processedFramesCounter);

  cv::Mat rgbFrame;
  cv::cvtColor(frame, rgbFrame, cv::COLOR_BGR2RGB);
  QImage qimg((const uchar *)rgbFrame.data, rgbFrame.cols, rgbFrame.rows,
              rgbFrame.step, QImage::Format_RGB888);

  QImage deepCopyFrame = qimg.copy();
  emit frameReady(deepCopyFrame.convertToFormat(QImage::Format_RGB32));
}
