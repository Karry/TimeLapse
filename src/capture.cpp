/*
 *   Copyright (C) 2022 Lukáš Karas <lukas.karas@centrum.cz>
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the
 *   Free Software Foundation, Inc.,
 *   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

#include <TimeLapse/capture.h>

#include <Magick++.h>
#include <ImageMagick-6/Magick++/Color.h>

#include <exception>
#include <vector>
#include <utility>

namespace timelapse {

AdaptiveShutterSpeedAlg::AdaptiveShutterSpeedAlg(
  QList<ShutterSpeedChoice> shutterSpeedChoices,
  ShutterSpeedChoice currentShutterSpeed,
  ShutterSpeedChoice minShutterSpeed,
  ShutterSpeedChoice maxShutterSpeed,
  QTextStream *err,
  QTextStream *verboseOutput) :

  shutterSpeedChoices(shutterSpeedChoices),
  currentShutterSpeed(currentShutterSpeed),
  minShutterSpeed(minShutterSpeed),
  maxShutterSpeed(maxShutterSpeed),
  err(err), verboseOutput(verboseOutput) {
  if (this->minShutterSpeed.toMicrosecond() > this->maxShutterSpeed.toMicrosecond() && err) {
    *err << "Shortest shutter speed " << minShutterSpeed.toString() << " is longer than max " << maxShutterSpeed.toString();
  }
  if (this->currentShutterSpeed.toMicrosecond() < this->minShutterSpeed.toMicrosecond()) {
    this->currentShutterSpeed = this->minShutterSpeed;
  } else if (this->currentShutterSpeed.toMicrosecond() > this->maxShutterSpeed.toMicrosecond()) {
    this->currentShutterSpeed = this->maxShutterSpeed;
  }
}

MatrixMeteringAlg::MatrixMeteringAlg(
  QList<ShutterSpeedChoice> shutterSpeedChoices,
  ShutterSpeedChoice currentShutterSpeed,
  ShutterSpeedChoice minShutterSpeed,
  ShutterSpeedChoice maxShutterSpeed,
  QTextStream *err,
  QTextStream *verboseOutput,
  int changeThreshold,
  int step) :

  AdaptiveShutterSpeedAlg(shutterSpeedChoices, currentShutterSpeed,
                          minShutterSpeed, maxShutterSpeed, err, verboseOutput),
  changeThreshold(changeThreshold), greyHistograms(),
  step(step) {
}

void MatrixMeteringAlg::clearHistograms() {
  for (uint32_t *hist : greyHistograms) {
    delete[] hist;
  }
  greyHistograms.clear();
}

MatrixMeteringAlg::~MatrixMeteringAlg() {
  clearHistograms();
}

void MatrixMeteringAlg::update(Magick::Image img) {
  // remove old histograms, keep last (changeThreshold) histograms
  while (greyHistograms.size() >= changeThreshold) {
    QList<uint32_t *>::Iterator it = greyHistograms.begin();
    delete[] * it;
    greyHistograms.removeFirst();
  }

  uint32_t *greyHistogram = new uint32_t[GREY_HISTOGRAM_RESOLUTION]();

  // get img histogram
  std::vector<std::pair < Magick::Color, size_t>> histogram;
  Magick::colorHistogram(&histogram, img);

  // compute grey-scale histogram
  for (std::pair<Magick::Color, size_t> p : histogram) {
    double r = Magick::Color::scaleQuantumToDouble(p.first.redQuantum());
    double g = Magick::Color::scaleQuantumToDouble(p.first.greenQuantum());
    double b = Magick::Color::scaleQuantumToDouble(p.first.blueQuantum());
    double grey = 0.299 * r + 0.587 * g + 0.114 * b;
    int i = std::max(
      std::min(GREY_HISTOGRAM_RESOLUTION, (int) (grey * ((double) GREY_HISTOGRAM_RESOLUTION))),
      0);
    greyHistogram[i] += p.second;
  }

  greyHistograms.append(greyHistogram);
}

ShutterSpeedChoice MatrixMeteringAlg::adjustShutterSpeed() {
  if (greyHistograms.size() < changeThreshold) {
    return currentShutterSpeed;
  }

  // iterate over last grey histograms and compute how many images was
  // under exposured (possitive score) and how many over exposured (negative score)
  // then, change shutter speed if abs(score) > (changeThreshold / 3)
  int changeScore = 0;

  for (uint32_t *histo : greyHistograms) {
    uint32_t pixCnt = 0;
    uint32_t underExposureCnt = 0;
    uint32_t overExposureCnt = 0;
    for (int ii = 0; ii < GREY_HISTOGRAM_RESOLUTION; ii++) {
      pixCnt += histo[ii];
      if (ii < UNDEREXPOSURE_GREY_LIMIT)
        underExposureCnt += histo[ii];
      if (ii > OVEREXPOSURE_GREY_LIMIT)
        overExposureCnt += histo[ii];
    }

    double underExposure = (double) underExposureCnt / (double) pixCnt;
    double overExposure = (double) overExposureCnt / (double) pixCnt;
    if (underExposure > UNDEREXPOSURE_RATIO_LIMIT || overExposure > OVEREXPOSURE_RATIO_LIMIT) {
      if (underExposure > overExposure)
        changeScore += 1;
      else
        changeScore -= 1;
    }
  }

  if (std::abs(changeScore) <= changeThreshold / 3) { // ignore low score
    return currentShutterSpeed;
  }
  if ((changeScore > 0 && currentShutterSpeed.toMicrosecond() >= maxShutterSpeed.toMicrosecond())
      || (changeScore < 0 && currentShutterSpeed.toMicrosecond() <= minShutterSpeed.toMicrosecond())) {
    return currentShutterSpeed;
  }

  // adjust ShutterSpeed
  ShutterSpeedChoice prev = currentShutterSpeed;
  if (currentShutterSpeed.isBulb()) {
    if (changeScore > 0) {
      currentShutterSpeed = ShutterSpeedChoice(
        true,
        std::min(currentShutterSpeed.toSecond() + (BULB_CHANGE_s * step), maxShutterSpeed.toSecond()),
        1);
    } else {
      currentShutterSpeed = ShutterSpeedChoice(
        true,
        std::min(currentShutterSpeed.toSecond() - (BULB_CHANGE_s * step), maxShutterSpeed.toSecond()),
        1);

      // if current bulb time is less than some regular speed, use it
      for (ShutterSpeedChoice ch : shutterSpeedChoices) {
        if ((!ch.isBulb()) && ch.toMicrosecond() >= currentShutterSpeed.toMicrosecond()) {
          currentShutterSpeed = ch;
          break;
        }
      }
    }
  } else {
    int currentStep = 0;
    if (changeScore > 0) {
      for (ShutterSpeedChoice ch : shutterSpeedChoices) {
        if ((!ch.isBulb()) && ch.toMicrosecond() > currentShutterSpeed.toMicrosecond()) {
          currentShutterSpeed = ch;
          currentStep += 1;
          if (currentShutterSpeed.toMicrosecond() >= maxShutterSpeed.toMicrosecond())
            break;
          if (currentStep >= step)
            break;
        }
      }
      if (prev.toMicrosecond() == currentShutterSpeed.toMicrosecond() && maxShutterSpeed.isBulb()) {
        currentShutterSpeed = ShutterSpeedChoice(
          true,
          std::min(currentShutterSpeed.toSecond() + (BULB_CHANGE_s * step), maxShutterSpeed.toSecond()),
          1);
      }
    } else {
      QList<ShutterSpeedChoice> reverse = shutterSpeedChoices;
      std::reverse(reverse.begin(), reverse.end());
      for (ShutterSpeedChoice ch : reverse) {
        if ((!ch.isBulb()) && ch.toMicrosecond() < currentShutterSpeed.toMicrosecond()) {
          currentShutterSpeed = ch;
          currentStep += 1;
          if (currentShutterSpeed.toMicrosecond() <= minShutterSpeed.toMicrosecond())
            break;
          if (currentStep >= step)
            break;
        }
      }
    }
  }

  // print verbose information
  if (verboseOutput) {
    *verboseOutput << "Some of previous frames was "
                   << (changeScore > 0 ? "underexposured" : "overexposured")
                   << " (score " << changeScore << "). Changing shutter speed from "
                   << prev.toString() << " to " << currentShutterSpeed.toString()
                   << endl;
  }
  clearHistograms();

  return currentShutterSpeed;
}

TimeLapseCapture::TimeLapseCapture(QTextStream* err, QTextStream* verboseOutput):
  err(err), verboseOutput(verboseOutput) {
  frameNumberLocale.setNumberOptions(QLocale::OmitGroupSeparator);
}

TimeLapseCapture::~TimeLapseCapture() noexcept {
  timer.stop();

  if (shutterSpdAlg != nullptr) {
    delete shutterSpdAlg;
    shutterSpdAlg = nullptr;
  }
}

void TimeLapseCapture::start() {
  qDebug() << "Start capturing";

  if (dev == nullptr) {
    if (err) {
      *err << "Capture camera is null" << endl;
    }
    return;
  }

  if (_output.exists()) {
    if (verboseOutput) {
      *verboseOutput << "Output directory exists already!" << endl;
    }
  } else {
    if (!_output.mkpath(".")) {
      if (err) {
        *err << "Mkdir failed " << _output.path() << endl;
      }
    }
  }

  connect(&timer, &QTimer::timeout, this, &TimeLapseCapture::capture);
  connect(dev.data(), &timelapse::CaptureDevice::imageCaptured,
          this, &TimeLapseCapture::onImageCaptured);
  connect(dev.data(), &timelapse::CaptureDevice::busyChanged,
          this, &TimeLapseCapture::onCameraBusyChanged);

  dev->start();
  //timer.start(interval);

  QList<ShutterSpeedChoice> choices = dev->getShutterSpeedChoices();
  if (_shutterSpeedChangeThreshold > 0) {
    ShutterSpeedChoice currentShutterSpeed = dev->currentShutterSpeed();

    if (verboseOutput) {
      *verboseOutput << "Using automatic shutter speed:" << endl;
      *verboseOutput << "  current shutter speed: " << currentShutterSpeed.toString() << endl;
      *verboseOutput << "  min. shutter speed:    " << _minShutterSpeed.toString() << endl;
      *verboseOutput << "  max. shutter speed:    " << _maxShutterSpeed.toString() << endl;
      *verboseOutput << "  change threshold:      " << _shutterSpeedChangeThreshold << endl;
      *verboseOutput << "  change step:           " << _shutterSpeedStep << endl;
    }
    shutterSpdAlg = new MatrixMeteringAlg(choices, currentShutterSpeed, _minShutterSpeed, _maxShutterSpeed,
                                          err, verboseOutput, _shutterSpeedChangeThreshold, _shutterSpeedStep);
  }
  qDebug() << "Start timer with interval " << _interval << " ms";
  capture();
}

void TimeLapseCapture::shutdown() {
  // capturing devices can be asynchronous, we should wait a bit for possible events from devices
  dev->stop();
  QTimer::singleShot(1000, this, SIGNAL(done()));
}

void TimeLapseCapture::onCameraBusyChanged() {
  if (postponedCapture && !dev->isBusy()) {
    capture();
  }
}

void TimeLapseCapture::capture() {
  postponedCapture = false;
  if (_count > 0 && capturedCnt >= _count) {
    timer.stop();
    shutdown();
    return;
  }

  if (dev->isBusy()) {
    postponedCapture = true;
    if (verboseOutput) {
      *verboseOutput << "Camera is busy, postpone capturing" << endl;
    }
    timer.stop();
    return;
  }
  if (!timer.isActive()) {
    timer.start(_interval);
  }

  try {
    capturedSubsequence = 0;
    capturedCnt++;
    ShutterSpeedChoice shutterSpeed=_minShutterSpeed;
    if (shutterSpdAlg != nullptr) {
      shutterSpeed = shutterSpdAlg->adjustShutterSpeed();
    }
    dev->capture(verboseOutput, shutterSpeed);
  } catch (std::exception &e) {
    if (err) {
      *err << "Capturing failed: " << QString::fromUtf8(e.what()) << endl;
    }
    emit error(e.what());
  }
}

QString TimeLapseCapture::leadingZeros(int i, int leadingZeros) {
  // default locale can include thousand delimiter
  QString s = frameNumberLocale.toString(i);
  if (leadingZeros <= s.length()) {
    return s;
  }

  return s.prepend(QString(leadingZeros - s.length(), '0'));
}

void TimeLapseCapture::onImageCaptured(QString format, Magick::Blob blob, Magick::Geometry sizeHint) {
  bool readRawFromFile = false;

  QString framePath = _output.path() + QDir::separator()
                      + leadingZeros(capturedCnt, FRAME_FILE_LEADING_ZEROS) + "_" + leadingZeros(capturedSubsequence, 2);

  if (format == "RGB") {
    if (_storeRawImages) {
      // store RAW RGB data in PPM format
      QString pgmHeader = QString("P6\n%1 %2\n255\n").arg(sizeHint.width()).arg(sizeHint.height());
      std::string headerStr = pgmHeader.toStdString();
      const char *headerBytes = headerStr.c_str();
      size_t headerLen = strlen(headerBytes);

      if (shutterSpdAlg != nullptr && capturedSubsequence == 0) {
        Magick::Image capturedImage;
        capturedImage.read(blob, sizeHint, 8, "RGB");
        shutterSpdAlg->update(capturedImage);
      }

      framePath += ".ppm";
      QFile file(framePath);
      file.open(QIODevice::WriteOnly);
      file.write(headerBytes, headerLen);
      file.write((const char*) blob.data(), blob.length());
      file.close();
    } else {
      // convert RGB data to JPEG
      Magick::Image capturedImage;
      capturedImage.read(blob, sizeHint, 8, "RGB");

      if (shutterSpdAlg != nullptr && capturedSubsequence == 0) {
        shutterSpdAlg->update(capturedImage);
      }

      QDateTime now = QDateTime::currentDateTime();
      QString exifDateTime = now.toString("yyyy:MM:dd HH:mm:ss");\

      // ImageMagick don't support writing of exif data
      // TODO: setup exif timestamp correctly
      capturedImage.attribute("EXIF:DateTime", exifDateTime.toStdString());
      //capturedImage.defineValue("EXIF", "DateTime", exifDateTime.toStdString());

      capturedImage.compressType(Magick::JPEGCompression);
      capturedImage.magick("JPEG");
      framePath += ".jpeg";
      capturedImage.write(framePath.toStdString());

    }
  } else {

    if (shutterSpdAlg != nullptr && capturedSubsequence == 0) {
      try {
        Magick::Image capturedImage;
        capturedImage.read(blob, format.toStdString());
        shutterSpdAlg->update(capturedImage);
      } catch (const std::exception &e) {
        if (err) {
          *err << "Failed to decode captured image (" << format << "): " << QString::fromUtf8(e.what()) << endl;
        }
        readRawFromFile = true;
      }
    }

    // store other formats in device specific format
    framePath += "." + format;
    QFile file(framePath);
    file.open(QIODevice::WriteOnly);
    file.write((const char*) blob.data(), blob.length());
    file.close();

    if (readRawFromFile && shutterSpdAlg != nullptr && capturedSubsequence == 0) {
      /* I don't understand ImageMagick correctly, but it fails with reading RAW files
       * from memory blob, but reading from file works (sometimes).
       * Maybe, it don't support delegating (dcraw, ufraw...) with memory data...
       */
      try {
        Magick::Image capturedImage;
        capturedImage.read(framePath.toStdString());
        shutterSpdAlg->update(capturedImage);
      } catch (const std::exception &e) {
        if (err) {
          *err << "Failed to decode captured image (" << framePath << "): " << QString::fromUtf8(e.what()) << endl;
        }
      }
    }
  }

  if (verboseOutput) {
    *verboseOutput << "Captured frame saved to " << framePath << endl;
  }
  capturedSubsequence++;
  emit imageCaptured(framePath);
  emit capturedCountChanged();
}

}
