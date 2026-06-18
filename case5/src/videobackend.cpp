#include "videobackend.h"
#include <QDebug>
#include <QUrl>
#include <algorithm>

VideoBackend::VideoBackend(QObject *parent)
    : QObject(parent), m_frameCount(0), m_hasReference(false),
      m_alarmThreshold(20.0) {
  connect(&m_timer, &QTimer::timeout, this, &VideoBackend::processFrame);
}

VideoBackend::~VideoBackend() { stopVideo(); }

void VideoBackend::loadVideo(const QString &path) {
  stopVideo();
  m_videoPath = QUrl(path).toLocalFile();

  m_cap.open(m_videoPath.toStdString());
  if (!m_cap.isOpened()) {
    emit statusMessage("Ошибка: Не удалось открыть видео!");
    return;
  }

  m_frameCount = 0;
  m_hasReference = false;
  m_densityHistory.clear();

  int total = m_cap.get(cv::CAP_PROP_FRAME_COUNT);
  emit videoOpened(total);

  m_cap >> m_lastRawFrame;
  if (!m_lastRawFrame.empty()) {
    m_frameCount = 1;
    processSingleFrame(m_lastRawFrame);
  }

  emit logMessage("Видео загружено. Выделите эталонное облако пыли на кадре.");
  emit statusMessage("Ожидание выделения эталона...");
}

void VideoBackend::pauseVideo() {
  m_timer.stop();
  emit statusMessage("Пауза");
}

void VideoBackend::resumeVideo() {
  if (m_cap.isOpened()) {
    m_timer.start(33);
    emit statusMessage("Мониторинг пыли...");
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
  emit statusMessage("Остановлено");
}

void VideoBackend::setAlarmThreshold(double val) {
  m_alarmThreshold = val;
  emit statusMessage(
      QString("Порог тревоги изменен: %1%").arg(m_alarmThreshold));
}

void VideoBackend::setVideoPosition(int frameIdx) {
  if (m_cap.isOpened()) {
    bool wasRunning = m_timer.isActive();
    m_timer.stop();

    m_cap.set(cv::CAP_PROP_POS_FRAMES, frameIdx);
    m_frameCount = frameIdx;

    m_cap >> m_lastRawFrame;
    if (!m_lastRawFrame.empty()) {
      processSingleFrame(m_lastRawFrame);
    }

    if (wasRunning)
      m_timer.start(33);
  }
}

void VideoBackend::setReferenceArea(double nx, double ny, double nw,
                                    double nh) {
  if (m_lastRawFrame.empty())
    return;

  int x = std::clamp(int(nx * m_lastRawFrame.cols), 0, m_lastRawFrame.cols - 1);
  int y = std::clamp(int(ny * m_lastRawFrame.rows), 0, m_lastRawFrame.rows - 1);
  int w = std::clamp(int(nw * m_lastRawFrame.cols), 1, m_lastRawFrame.cols - x);
  int h = std::clamp(int(nh * m_lastRawFrame.rows), 1, m_lastRawFrame.rows - y);

  cv::Rect roi(x, y, w, h);
  cv::Mat dustPatch = m_lastRawFrame(roi);

  cv::Mat hsvPatch;
  cv::cvtColor(dustPatch, hsvPatch, cv::COLOR_BGR2HSV);

  cv::Mat mean, stddev;
  cv::meanStdDev(hsvPatch, mean, stddev);

  m_hsvMean = cv::Scalar(mean.at<double>(0, 0), mean.at<double>(1, 0),
                         mean.at<double>(2, 0));
  m_hsvStd = cv::Scalar(stddev.at<double>(0, 0), stddev.at<double>(1, 0),
                        stddev.at<double>(2, 0));

  m_hasReference = true;
  m_densityHistory.clear();

  emit logMessage(QString("Эталон захвачен! H:%1 S:%2 V:%3")
                      .arg(int(m_hsvMean[0]))
                      .arg(int(m_hsvMean[1]))
                      .arg(int(m_hsvMean[2])));
  emit statusMessage("Эталон готов. Нажмите Плей.");

  processSingleFrame(m_lastRawFrame);
}

void VideoBackend::processFrame() {
  if (!m_cap.isOpened())
    return;

  m_cap >> m_lastRawFrame;
  if (m_lastRawFrame.empty()) {
    m_timer.stop();
    m_cap.release();
    emit videoEnded();
    emit statusMessage("Видео обработано");
    return;
  }
  m_frameCount++;
  emit framePositionChanged(m_frameCount);

  processSingleFrame(m_lastRawFrame);
}

void VideoBackend::processSingleFrame(const cv::Mat &sourceFrame) {
  cv::Mat frame = sourceFrame.clone();

  double currentDensity = 0.0;
  bool isAlarm = false;

  if (m_hasReference) {
    cv::Mat hsv;
    cv::cvtColor(frame, hsv, cv::COLOR_BGR2HSV);

    double h_tol = std::max(15.0, 3.5 * m_hsvStd[0]);
    double s_tol = std::max(40.0, 3.5 * m_hsvStd[1]);
    double v_tol = std::max(40.0, 3.5 * m_hsvStd[2]);

    cv::Scalar lowerBound(std::max(0.0, m_hsvMean[0] - h_tol),
                          std::max(0.0, m_hsvMean[1] - s_tol),
                          std::max(0.0, m_hsvMean[2] - v_tol));
    cv::Scalar upperBound(std::min(180.0, m_hsvMean[0] + h_tol),
                          std::min(255.0, m_hsvMean[1] + s_tol),
                          std::min(255.0, m_hsvMean[2] + v_tol));

    cv::Mat colorMask;
    cv::inRange(hsv, lowerBound, upperBound, colorMask);

    cv::Mat gray, gray32f, sqGray, mean, sqMean, variance, stddev;
    cv::cvtColor(frame, gray, cv::COLOR_BGR2GRAY);
    gray.convertTo(gray32f, CV_32F);
    cv::multiply(gray32f, gray32f, sqGray);

    cv::boxFilter(gray32f, mean, CV_32F, cv::Size(15, 15));
    cv::boxFilter(sqGray, sqMean, CV_32F, cv::Size(15, 15));

    variance = sqMean - mean.mul(mean);
    cv::max(variance, 0.0, variance);
    cv::sqrt(variance, stddev);

    cv::Mat textureMask = (stddev <= 35.0);

    cv::Mat finalMask;
    cv::bitwise_and(colorMask, textureMask, finalMask);

    cv::Mat kClose =
        cv::getStructuringElement(cv::MORPH_ELLIPSE, cv::Size(35, 35));
    cv::Mat kOpen =
        cv::getStructuringElement(cv::MORPH_ELLIPSE, cv::Size(9, 9));

    cv::morphologyEx(finalMask, finalMask, cv::MORPH_CLOSE, kClose);
    cv::morphologyEx(finalMask, finalMask, cv::MORPH_OPEN, kOpen);
    cv::dilate(finalMask, finalMask, kClose);

    std::vector<std::vector<cv::Point>> contours;
    cv::findContours(finalMask, contours, cv::RETR_EXTERNAL,
                     cv::CHAIN_APPROX_SIMPLE);

    double totalDensitySum = 0.0;
    int validClouds = 0;

    cv::Mat overlay = frame.clone();

    for (size_t i = 0; i < contours.size(); ++i) {
      double area = cv::contourArea(contours[i]);
      if (area > 800) { // Отсекаем мелкий шум
        cv::Rect box = cv::boundingRect(contours[i]);

        cv::Mat boxMask = finalMask(box);
        int dustPixels = cv::countNonZero(boxMask);
        double density = (double)dustPixels / (box.width * box.height) * 100.0;

        totalDensitySum += density;
        validClouds++;

        bool localAlarm = (density > m_alarmThreshold);

        cv::Scalar pinkFill(203, 192, 255);
        cv::Scalar redBorder(0, 0, 255);
        cv::Scalar yellowFill(150, 255, 255);
        cv::Scalar yellowBorder(0, 255, 255);

        cv::Scalar fillColor = localAlarm ? pinkFill : yellowFill;
        cv::Scalar contourColor = localAlarm ? redBorder : yellowBorder;

        cv::drawContours(overlay, contours, (int)i, fillColor, cv::FILLED);

        cv::drawContours(frame, contours, (int)i, contourColor, 3);

        cv::putText(frame, "DUST " + std::to_string(int(density)) + "%",
                    cv::Point(box.x, box.y - 10), cv::FONT_HERSHEY_SIMPLEX, 0.8,
                    contourColor, 2);
      }
    }

    cv::addWeighted(overlay, 0.45, frame, 0.55, 0, frame);

    if (validClouds > 0) {
      double avgFrameDensity = totalDensitySum / validClouds;
      m_densityHistory.push_back(avgFrameDensity);
      if (m_densityHistory.size() > 5)
        m_densityHistory.pop_front();

      double smoothedDensity = 0;
      for (double d : m_densityHistory)
        smoothedDensity += d;
      smoothedDensity /= m_densityHistory.size();

      currentDensity = smoothedDensity;
      isAlarm = (currentDensity >= m_alarmThreshold);
    } else {
      m_densityHistory.clear();
    }

    emit alarmStatusUpdated(isAlarm, currentDensity);
  }

  cv::Mat rgbFrame;
  cv::cvtColor(frame, rgbFrame, cv::COLOR_BGR2RGB);

  QImage qimg((const uchar *)rgbFrame.data, rgbFrame.cols, rgbFrame.rows,
              rgbFrame.step, QImage::Format_RGB888);

  QImage deepCopyFrame = qimg.copy();
  emit frameReady(deepCopyFrame.convertToFormat(QImage::Format_RGB32));
}
