#include "videobackend.h"
#include <QDateTime>
#include <QDebug>
#include <QUrl>

VideoBackend::VideoBackend(QObject *parent)
    : QObject(parent), m_frameCount(0), m_isPhotoMode(false) {
  connect(&m_timer, &QTimer::timeout, this, &VideoBackend::processFrame);
}

VideoBackend::~VideoBackend() { stopVideo(); }

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
  m_varianceHistory.clear();

  m_mog2 = cv::createBackgroundSubtractorMOG2(150, 24.0, false);

  int total = m_cap.get(cv::CAP_PROP_FRAME_COUNT);
  emit videoOpened(total);

  emit logMessage(QString("[%1] === ЗАПУСК СИСТЕМЫ КЛАССИФИКАЦИИ ГРУЗОВ ===")
                      .arg(QDateTime::currentDateTime().toString("hh:mm:ss")));
  emit logMessage(QString("[%1] Загружен файл: %2")
                      .arg(QDateTime::currentDateTime().toString("hh:mm:ss"))
                      .arg(m_videoPath));
  emit statusMessage("Анализ груза...");
  m_timer.start(15);
}

void VideoBackend::pauseVideo() {
  m_timer.stop();
  emit statusMessage("Пауза");
  emit logMessage(QString("[%1] Анализ приостановлен пользователем.")
                      .arg(QDateTime::currentDateTime().toString("hh:mm:ss")));
}

void VideoBackend::resumeVideo() {
  if (m_cap.isOpened()) {
    m_timer.start(15);
    emit statusMessage("Анализ...");
    emit logMessage(
        QString("[%1] Анализ возобновлен.")
            .arg(QDateTime::currentDateTime().toString("hh:mm:ss")));
  }
}

void VideoBackend::restartVideo() {
  if (!m_videoPath.isEmpty()) {
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

void VideoBackend::setVideoPosition(int frameIdx) {
  if (m_cap.isOpened()) {
    bool wasRunning = m_timer.isActive();
    m_timer.stop();

    m_cap.set(cv::CAP_PROP_POS_FRAMES, frameIdx);
    m_frameCount = frameIdx;

    m_varianceHistory.clear();
    m_mog2 = cv::createBackgroundSubtractorMOG2(150, 24.0, false);

    cv::Mat frame;
    m_cap >> frame;
    if (!frame.empty()) {
      processSingleFrame(frame);
    }

    if (wasRunning)
      m_timer.start(15);
  }
}

double VideoBackend::calculateLaplacianVariance(const cv::Mat &image) {
  cv::Mat lap;
  cv::Laplacian(image, lap, CV_64F);

  cv::Mat mean, stddev;
  cv::meanStdDev(lap, mean, stddev);

  return stddev.at<double>(0) * stddev.at<double>(0);
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
    emit logMessage(
        QString("[%1] === АНАЛИЗ ЗАВЕРШЕН ===")
            .arg(QDateTime::currentDateTime().toString("hh:mm:ss")));
    return;
  }
  m_frameCount++;

  emit framePositionChanged(m_frameCount);

  processSingleFrame(frame);
}

void VideoBackend::processSingleFrame(const cv::Mat &sourceFrame) {
  cv::Mat frame = sourceFrame.clone();

  cv::Mat blurFrame, fgMask;
  cv::GaussianBlur(frame, blurFrame, cv::Size(15, 15), 0);

  m_mog2->apply(blurFrame, fgMask, 0.02);

  cv::threshold(fgMask, fgMask, 200, 255, cv::THRESH_BINARY);

  cv::Mat kernel =
      cv::getStructuringElement(cv::MORPH_ELLIPSE, cv::Size(35, 35));
  cv::morphologyEx(fgMask, fgMask, cv::MORPH_CLOSE, kernel);
  cv::dilate(fgMask, fgMask, kernel);

  std::vector<std::vector<cv::Point>> contours;
  cv::findContours(fgMask, contours, cv::RETR_EXTERNAL,
                   cv::CHAIN_APPROX_SIMPLE);

  cv::Mat overlay = frame.clone();
  cv::Scalar activeGreen(0, 255, 0);

  for (size_t i = 0; i < contours.size(); ++i) {
    double area = cv::contourArea(contours[i]);

    if (area > 4000) {
      cv::drawContours(overlay, contours, (int)i, activeGreen, cv::FILLED);
      cv::drawContours(frame, contours, (int)i, activeGreen, 3);

      cv::Rect r = cv::boundingRect(contours[i]);
      cv::putText(frame, "MACHINERY", cv::Point(r.x, r.y - 10),
                  cv::FONT_HERSHEY_SIMPLEX, 0.7, activeGreen, 2);
    }
  }

  cv::addWeighted(overlay, 0.4, frame, 0.6, 0, frame);

  int roiX = int(frame.cols * 0.20);
  int roiY = int(frame.rows * 0.40);
  int roiW = int(frame.cols * 0.60);
  int roiH = int(frame.rows * 0.45);
  cv::Rect roi(roiX, roiY, roiW, roiH);
  roi &= cv::Rect(0, 0, frame.cols, frame.rows);

  cv::rectangle(frame, roi, cv::Scalar(255, 0, 0), 3);

  cv::Mat cargoFrame = frame(roi);
  cv::Mat grayCargo;
  cv::cvtColor(cargoFrame, grayCargo, cv::COLOR_BGR2GRAY);

  double variance = calculateLaplacianVariance(grayCargo);

  m_varianceHistory.push_back(variance);
  if (m_varianceHistory.size() > 300) {
    m_varianceHistory.pop_front();
  }

  double avgVariance = 0.0;
  for (double v : m_varianceHistory) {
    avgVariance += v;
  }
  avgVariance /= m_varianceHistory.size();

  QString stateText;
  cv::Scalar textColor;
  QString logClassStr;

  if (m_varianceHistory.size() < 300 && m_frameCount <= 300) {
    int progress = (m_varianceHistory.size() * 100) / 300;
    stateText = QString("CALIBRATING 10S... %1%").arg(progress);
    textColor = cv::Scalar(0, 255, 255);

    if (m_frameCount % 30 == 0) {
      emit logMessage(
          "-----------------------------------\n" +
          QString("[%1] === РЕЖИМ КАЛИБРОВКИ СИСТЕМЫ (10 СЕКУНД) ===\n")
              .arg(QDateTime::currentDateTime().toString("hh:mm:ss")) +
          QString(" Прогресс калибровки: %1%\n").arg(progress) +
          QString(" Накоплено кадров:    %1 / 300\n")
              .arg(m_varianceHistory.size()) +
          QString(" Инд. шероховатости:  %1\n")
              .arg(QString::number(variance, 'f', 1)) +
          " Статус: Накопление стабильного скользящего среднего...");
    }
  } else {
    QString statusDescription;
    if (avgVariance >= 1000) {
      stateText = "LARGE CARGO (CRUSHED STONE)";
      textColor = cv::Scalar(0, 0, 255);
      logClassStr = "КРУПНОДИСПЕРСНЫЙ ГРУЗ (ЩЕБЕНЬ/КАМНИ)";
      statusDescription =
          "Выявлена неоднородная шероховатая текстура фракционного материала.";
    } else if (avgVariance >= 150) {
      stateText = "FINE CARGO (CEMENT/SAND)";
      textColor = cv::Scalar(0, 165, 255);
      logClassStr = "МЕЛКОДИСПЕРСНЫЙ ГРУЗ (ПЕСОК/ЦЕМЕНТ)";
      statusDescription =
          "Кузов заполнен однородным мелкофракционным сыпучим материалом.";
    } else {
      stateText = "EMPTY";
      textColor = cv::Scalar(0, 255, 0);
      logClassStr = "КУЗОВ ПУСТ";
      statusDescription = "Текстура дна кузова распознана как пустая "
                          "металлическая поверхность.";
    }

    if (m_frameCount % 60 == 0) {
      emit logMessage(
          "-----------------------------------\n" +
          QString("[%1] === РЕЗУЛЬТАТ КЛАССИФИКАЦИИ ГРУЗА ===\n")
              .arg(QDateTime::currentDateTime().toString("hh:mm:ss")) +
          QString(" Ср. индекс шероховатости (10с): %1\n")
              .arg(QString::number(avgVariance, 'f', 1)) +
          QString(" ТИП ГРУЗА: %1\n").arg(logClassStr) +
          QString(" Статус:    %1").arg(statusDescription));
    }
  }

  cv::putText(frame, stateText.toStdString(), cv::Point(roi.x + 10, roi.y + 30),
              cv::FONT_HERSHEY_SIMPLEX, 0.8, textColor, 2);

  cv::Mat resizedFrame;
  double scale = 800.0 / frame.cols;
  cv::resize(frame, resizedFrame, cv::Size(), scale, scale);

  cv::Mat rgbFrame;
  cv::cvtColor(resizedFrame, rgbFrame, cv::COLOR_BGR2RGB);
  QImage qimg((const uchar *)rgbFrame.data, rgbFrame.cols, rgbFrame.rows,
              rgbFrame.step, QImage::Format_RGB888);

  QImage deepCopyFrame = qimg.copy();
  emit frameReady(deepCopyFrame.convertToFormat(QImage::Format_RGB32));
}
