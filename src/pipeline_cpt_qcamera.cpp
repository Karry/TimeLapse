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

#include "pipeline_cpt_qcamera.h"
#include "pipeline_cpt_qcamera.moc"

#include "timelapse.h"

#include <QtMultimedia/QCameraInfo>
#include <QtMultimedia/QCamera>
#include <QtMultimedia/QCameraImageCapture>

namespace timelapse {

QCameraDevice::QCameraDevice(const QCameraInfo &info):
  info(info)
{
}

QCameraDevice::QCameraDevice(const QCameraDevice &other):
  info(other.info)
{
}

QCameraDevice& QCameraDevice::operator=(const QCameraDevice &other) {
  info = other.info;
  camera.reset();
  return *this;
}

void QCameraDevice::capture([[maybe_unused]] QTextStream *verboseOut, ShutterSpeedChoice shutterSpeed) {
  [[maybe_unused]] bool b = initialize();
  assert(b);

  if (shutterSpeed.toMicrosecond() > 0) {
    if (QCameraExposure *exposure = camera->exposure(); exposure!=nullptr) {
      exposure->setManualShutterSpeed(qreal(shutterSpeed.toMicrosecond()) / 1000000);
    }
  }

  camera->start();

  if (camera->requestedLocks()==QCamera::LockType::NoLock) {
    onLocked();
  } else {
    camera->searchAndLock();
  }
}

QString QCameraDevice::toString() {
  QString description = "qt:";
  description += info.deviceName();
  if (auto position = info.position() ; position == QCamera::Position::BackFace) {
    description += " (back)";
  } else if (position == QCamera::Position::FrontFace) {
    description += " (front)";
  }
  description += "\t\"" + info.description() + "\"";
  return description;
}

QString QCameraDevice::toShortString() {
  return info.deviceName();
}

bool QCameraDevice::initialize(QTextStream *verboseOut) {
  if (!camera) {
    camera = make_unique<QCamera>(info);
    if (!camera->isCaptureModeSupported(QCamera::CaptureStillImage)) {
      if (verboseOut!=nullptr) {
        *verboseOut << "Still image capture is not supported by " << toString() << endl;
      }
      return false;
    }
    camera->setCaptureMode(QCamera::CaptureStillImage);
    connect(camera.get(), &QCamera::lockFailed, this, &QCameraDevice::onLockFailed);
    connect(camera.get(), &QCamera::locked, this, &QCameraDevice::onLocked);

    camera->load(); // load camera, probe resolution and available settings

    imageCapture = std::make_unique<QCameraImageCapture>(camera.get());
    if (!imageCapture->isCaptureDestinationSupported(QCameraImageCapture::CaptureToBuffer)) {
      if (verboseOut!=nullptr) {
        *verboseOut << "Capture to buffer is not supported by " << toString() << endl;
      }
      return false;
    }
    imageCapture->setCaptureDestination(QCameraImageCapture::CaptureToBuffer);

    QSize maxSize;
    for (QSize size: imageCapture->supportedResolutions()) {
      if ((!maxSize.isValid()) || (size.width()*size.height() > maxSize.width()*maxSize.height())) {
        maxSize=size;
      }
    }
    QImageEncoderSettings encoding = imageCapture->encodingSettings();
    encoding.setQuality(QMultimedia::EncodingQuality::VeryHighQuality);
    if (maxSize.isValid()){
      encoding.setResolution(maxSize);
      if (verboseOut!=nullptr) {
        *verboseOut << "using resolution " << maxSize.width() << "x" << maxSize.height() << endl;
      }
    }
    imageCapture->setEncodingSettings(encoding);

    connect(imageCapture.get(), &QCameraImageCapture::imageAvailable, this, &QCameraDevice::onImageAvailable);
    connect(imageCapture.get(), &QCameraImageCapture::readyForCaptureChanged, this, &QCameraDevice::onReadyForCaptureChanged);
  }
  return true;
}

void QCameraDevice::onLockFailed() {
  qWarning() << "Lock failed";
}

void QCameraDevice::onLocked() {
  [[maybe_unused]] bool b = initialize();
  assert(b);

  if (imageCapture->isReadyForCapture()) {
    postponedCapture = false;
    imageCapture->capture();
  } else {
    postponedCapture = true;
  }
}

void QCameraDevice::onReadyForCaptureChanged(bool ready) {
  if (ready && postponedCapture) {
    postponedCapture = false;
    imageCapture->capture();
  }
}

void QCameraDevice::onImageAvailable([[maybe_unused]] int id, const QVideoFrame &constFrame) {
  QVideoFrame frame(constFrame); // make a copy to be able map the frame
  if (!frame.isValid()) {
    qWarning() << "Video frame is not valid";
    return;
  }
  if (!frame.map(QAbstractVideoBuffer::MapMode::ReadOnly)) {
    qWarning() << "Video frame cannot be mapped";
    return;
  }
  if (!frame.isReadable()) {
    qWarning() << "Video frame is not readable";
    return;
  }

  QString format;
  switch (frame.pixelFormat()) {
      case QVideoFrame::Format_ARGB32: format = "ARGB32"; break;
      case QVideoFrame::Format_RGB32: format = "RGB32"; break;
      case QVideoFrame::Format_RGB24: format = "RGB24"; break;
      case QVideoFrame::Format_RGB565: format = "RGB565"; break;
      case QVideoFrame::Format_RGB555: format = "RGB555"; break;
      case QVideoFrame::Format_BGRA32: format = "BGRA32"; break;
      case QVideoFrame::Format_BGR32: format = "BGR32"; break;
      case QVideoFrame::Format_BGR24: format = "BGR24"; break;
      case QVideoFrame::Format_BGR565: format = "BGR565"; break;
      case QVideoFrame::Format_BGR555: format = "BGR555"; break;
      case QVideoFrame::Format_AYUV444: format = "AYUV444"; break;
      case QVideoFrame::Format_YUV444: format = "YUV444"; break;
      case QVideoFrame::Format_YUV420P: format = "YUV420P"; break;
      case QVideoFrame::Format_YV12: format = "YV12"; break;
      case QVideoFrame::Format_UYVY: format = "UYVY"; break;
      case QVideoFrame::Format_YUYV: format = "YUYV"; break;
      case QVideoFrame::Format_NV12: format = "NV12"; break;
      case QVideoFrame::Format_NV21: format = "NV21"; break;
      case QVideoFrame::Format_IMC1: format = "IMC1"; break;
      case QVideoFrame::Format_IMC2: format = "IMC2"; break;
      case QVideoFrame::Format_IMC3: format = "IMC3"; break;
      case QVideoFrame::Format_IMC4: format = "IMC4"; break;
      case QVideoFrame::Format_Y8: format = "Y8"; break;
      case QVideoFrame::Format_Y16: format = "Y16"; break;
      case QVideoFrame::Format_Jpeg: format = "jpeg"; break;
      case QVideoFrame::Format_CameraRaw: format = "CameraRaw"; break;
      case QVideoFrame::Format_AdobeDng: format = "AdobeDng"; break;
#if QT_VERSION >= QT_VERSION_CHECK(5,13,0)
      case QVideoFrame::Format_ABGR32: format = "ABGR32"; break;
      case QVideoFrame::Format_YUV422P: format = "YUV422P"; break;
#endif
      default: break;
  }
  if (format.isEmpty()) {
    qWarning() << "Video frame format is not supported:" << frame.pixelFormat();
    return;
  }

  emit imageCaptured(format,
                     Magick::Blob(frame.bits(), frame.mappedBytes()),
                     Magick::Geometry(frame.width(), frame.height()));
  camera->unlock();
}

ShutterSpeedChoice QCameraDevice::currentShutterSpeed() {
  [[maybe_unused]] bool b = initialize();
  assert(b);

  QCameraExposure *exposure = camera->exposure();
  if (exposure==nullptr) {
    return ShutterSpeedChoice();
  }

  ShutterSpeedChoice ch(false, 1000000, exposure->shutterSpeed() / 1000000);
  ch.normalize();
  return ch;
}

QList<ShutterSpeedChoice> QCameraDevice::getShutterSpeedChoices() {
  [[maybe_unused]] bool b = initialize();
  assert(b);

  QCameraExposure *exposure = camera->exposure();
  QList<ShutterSpeedChoice> result;
  if (exposure==nullptr) {
    return result;
  }
  for (qreal speed: exposure->supportedShutterSpeeds()) {
    ShutterSpeedChoice ch(false, 1000000, speed / 1000000);
    ch.normalize();
    result << ch;
  }
  return result;
}

QList<QCameraDevice> QCameraDevice::listDevices(QTextStream *verboseOut) {
  QList<QCameraDevice> result;
  const QList<QCameraInfo> cameras = QCameraInfo::availableCameras();
  for (const QCameraInfo &cameraInfo : cameras) {
    if (QCameraDevice cam(cameraInfo); cam.initialize(verboseOut)) {
      result << cam;
    }
  }
  return result;
}

}
