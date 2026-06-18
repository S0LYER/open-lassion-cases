#include "videobackend.h"
#include <QDebug>
#include <QImage>
#include <QUrl>
#include <algorithm>

VideoBackend::VideoBackend(QObject *parent)
    : QObject(parent), m_isModelLoaded(false), m_currentMode(0),
      m_frameCount(0), m_nextPassengerId(1), m_countIn(0), m_countOut(0) {
  connect(&m_timer, &QTimer::timeout, this, &VideoBackend::processFrame);
}

VideoBackend::~VideoBackend() { stopVideo(); }

void VideoBackend::loadModel(const QString &pathCfg) {
  m_cfgPath = QUrl(pathCfg).toLocalFile();
  emit statusMessage(
      "Конфигурация YOLOv4-tiny загружена. Выберите веса (.weights)");
}

void VideoBackend::loadWeights(const QString &pathWeights) {
  m_weightsPath = QUrl(pathWeights).toLocalFile();
  if (m_cfgPath.isEmpty()) {
    emit statusMessage("Ошибка: Загрузите сначала файл .cfg!");
    return;
  }

  try {
    m_net = cv::dnn::readNetFromDarknet(m_cfgPath.toStdString(),
                                        m_weightsPath.toStdString());
    m_net.setPreferableBackend(cv::dnn::DNN_BACKEND_OPENCV);
    m_net.setPreferableTarget(cv::dnn::DNN_TARGET_CPU);
    m_isModelLoaded = true;
    emit statusMessage("Нейросеть YOLOv4-tiny успешно инициализирована!");
  } catch (const cv::Exception &e) {
    emit statusMessage("Ошибка инициализации сети Darknet!");
  }
}

void VideoBackend::loadVideo(const QString &path) {
  stopVideo();
  m_videoPath = QUrl(path).toLocalFile();

  m_cap.open(m_videoPath.toStdString());
  if (!m_cap.isOpened()) {
    emit statusMessage("Ошибка: Не удалось открыть видео!");
    return;
  }

  m_frameCount = 0;
  m_tracks.clear();
  m_nextPassengerId = 1;

  int total = m_cap.get(cv::CAP_PROP_FRAME_COUNT);
  emit videoOpened(total);

  emit statsUpdated(m_countIn, m_countOut, std::max(0, m_countIn - m_countOut));
  emit statusMessage("Подсчет пассажиропотока активен...");
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

void VideoBackend::setMode(int mode) {
  m_currentMode = mode;
  emit statsUpdated(m_countIn, m_countOut, std::max(0, m_countIn - m_countOut));
}

void VideoBackend::resetStats() {
  m_countIn = 0;
  m_countOut = 0;
  m_tracks.clear();
  m_nextPassengerId = 1;
  emit statsUpdated(0, 0, 0);
  emit statusMessage("Статистика пассажиропотока сброшена.");
}

void VideoBackend::setVideoPosition(int frameIdx) {
  if (m_cap.isOpened()) {
    bool wasRunning = m_timer.isActive();
    m_timer.stop();

    m_cap.set(cv::CAP_PROP_POS_FRAMES, frameIdx);
    m_frameCount = frameIdx;

    cv::Mat frame;
    m_cap >> frame;
    if (!frame.empty()) {
      cv::Mat rgbFrame;
      cv::cvtColor(frame, rgbFrame, cv::COLOR_BGR2RGB);
      QImage qimg((const uchar *)rgbFrame.data, rgbFrame.cols, rgbFrame.rows,
                  rgbFrame.step, QImage::Format_RGB888);
      emit frameReady(qimg.rgbSwapped().convertToFormat(QImage::Format_RGB32));
    }

    if (wasRunning) {
      m_timer.start(15);
    }
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
    emit statusMessage("Видео успешно обработано полностью");
    return;
  }
  m_frameCount++;

  emit framePositionChanged(m_frameCount); // for slider

  std::vector<cv::Rect> currentDetections;
  std::vector<float> confidences;

  if (m_isModelLoaded) {
    int64 tStart = cv::getTickCount();

    cv::Mat blob;
    cv::dnn::blobFromImage(frame, blob, 1.0 / 255.0, cv::Size(416, 416),
                           cv::Scalar(), true, false);
    m_net.setInput(blob);

    std::vector<cv::Mat> outputs;
    m_net.forward(outputs, m_net.getUnconnectedOutLayersNames());

    for (const auto &out : outputs) {
      float *data = (float *)out.data;
      for (int i = 0; i < out.rows; ++i, data += out.cols) {
        float confidence = data[4];
        if (confidence > 0.3) {
          cv::Mat scores = out.row(i).colRange(5, out.cols);
          cv::Point classIdPoint;
          double maxClassScore;
          cv::minMaxLoc(scores, 0, &maxClassScore, 0, &classIdPoint);

          if (classIdPoint.x == 0 && maxClassScore > 0.3) {
            int cx = (int)(data[0] * frame.cols);
            int cy = (int)(data[1] * frame.rows);
            int w = (int)(data[2] * frame.cols);
            int h = (int)(data[3] * frame.rows);

            int left = cx - w / 2;
            int top = cy - h / 2;

            if (w * h > 2000) {
              currentDetections.push_back(cv::Rect(left, top, w, h));
              confidences.push_back(maxClassScore);
            }
          }
        }
      }
    }

    std::vector<int> indices;
    cv::dnn::NMSBoxes(currentDetections, confidences, 0.3f, 0.4f, indices);

    std::vector<cv::Rect> filteredDetections;
    for (int idx : indices) {
      filteredDetections.push_back(currentDetections[idx]);
    }
    currentDetections = filteredDetections;

    std::vector<bool> matchedDetections(currentDetections.size(), false);
    for (auto &track : m_tracks) {
      cv::Point trackCenter =
          track.box.tl() + cv::Point(track.box.width / 2, track.box.height / 2);
      double minDist = 9999.0;
      int bestIdx = -1;

      for (size_t d = 0; d < currentDetections.size(); ++d) {
        if (matchedDetections[d])
          continue;
        cv::Point detCenter = currentDetections[d].tl() +
                              cv::Point(currentDetections[d].width / 2,
                                        currentDetections[d].height / 2);
        double dist = cv::norm(trackCenter - detCenter);
        if (dist < minDist) {
          minDist = dist;
          bestIdx = d;
        }
      }

      if (minDist < 80.0 && bestIdx != -1) {
        track.box = currentDetections[bestIdx];
        track.path.push_back(track.box.tl() + cv::Point(track.box.width / 2,
                                                        track.box.height / 2));
        if (track.path.size() > 15)
          track.path.erase(track.path.begin());
        track.age = 0;
        matchedDetections[bestIdx] = true;
      } else {
        track.age++;
      }
    }

    for (size_t d = 0; d < currentDetections.size(); ++d) {
      if (!matchedDetections[d]) {
        cv::Point detCenter = currentDetections[d].tl() +
                              cv::Point(currentDetections[d].width / 2,
                                        currentDetections[d].height / 2);

        bool tooClose = false;
        for (const auto &track : m_tracks) {
          if (track.counted && !track.path.empty()) {
            if (cv::norm(detCenter - track.path.back()) < 60.0) {
              tooClose = true;
              break;
            }
          }
        }
        if (tooClose)
          continue;

        PassengerTrack newTrack;
        newTrack.id = m_nextPassengerId++;
        newTrack.box = currentDetections[d];
        newTrack.path.push_back(detCenter);
        newTrack.age = 0;
        newTrack.counted = false;
        m_tracks.push_back(newTrack);
      }
    }

    double timeMs =
        (cv::getTickCount() - tStart) * 1000.0 / cv::getTickFrequency();
    qDebug() << "[YOLOv4-tiny] Кадр ИИ обработан за:" << timeMs << "мс";
  }

  int minVerticalMove = std::max(30, int(frame.rows * 0.08));

  for (auto &track : m_tracks) {
    if (!track.counted && track.path.size() >= 5) {
      cv::Point startPt = track.path.front();
      cv::Point endPt = track.path.back();
      int verticalShift = endPt.y - startPt.y;

      if (std::abs(verticalShift) > minVerticalMove) {
        if (verticalShift < 0) {
          if (m_currentMode == 0 || m_currentMode == 1) {
            m_countIn++;
          }
        } else {
          if (m_currentMode == 0 || m_currentMode == 2) {
            m_countOut++;
          }
        }
        track.counted = true;
        emit statsUpdated(m_countIn, m_countOut,
                          std::max(0, m_countIn - m_countOut));
      }
    }
  }

  m_tracks.erase(std::remove_if(m_tracks.begin(), m_tracks.end(),
                                [](const PassengerTrack &t) {
                                  return (t.counted && t.age > 15) ||
                                         (!t.counted && t.age > 8);
                                }),
                 m_tracks.end());

  for (const auto &track : m_tracks) {
    cv::Scalar color;
    if (track.counted) {
      color = cv::Scalar(128, 128, 128);
    } else if (track.path.size() >= 10) {
      color = cv::Scalar(0, 255, 0);
    } else {
      color = cv::Scalar(0, 165, 255);
    }

    cv::rectangle(frame, track.box, color, 3);
    std::string label = "ID:" + std::to_string(track.id);
    cv::putText(frame, label, cv::Point(track.box.x, track.box.y - 10),
                cv::FONT_HERSHEY_SIMPLEX, 0.6, color, 2);

    for (size_t p = 1; p < track.path.size(); ++p) {
      cv::line(frame, track.path[p - 1], track.path[p], color, 2);
    }
  }

  cv::Mat rgbFrame;
  cv::cvtColor(frame, rgbFrame, cv::COLOR_BGR2RGB);
  QImage qimg((const uchar *)rgbFrame.data, rgbFrame.cols, rgbFrame.rows,
              rgbFrame.step, QImage::Format_RGB888);

  QImage deepCopyFrame = qimg.rgbSwapped();
  emit frameReady(deepCopyFrame.convertToFormat(QImage::Format_RGB32));
}
