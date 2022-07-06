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

#include <TimeLapse/pipeline_cpt_qcamera.h>

#include <TimeLapse/timelapse.h>

#include <QtMultimedia/QCameraInfo>
#include <QtMultimedia/QCamera>
#include <QtMultimedia/QCameraImageCapture>

#include <cassert>

namespace timelapse {

QCameraDevice::QCameraDevice(const QCameraInfo &info):
  info(info)
{
}

QCameraDevice::QCameraDevice(const QCameraDevice &other):
  info(other.info)
{
  if (other.camera) {
    [[maybe_unused]] bool b = initialize();
    assert(b);
  }
}

QCameraDevice& QCameraDevice::operator=(const QCameraDevice &other) {
  info = other.info;
  camera.reset();

  if (other.camera) {
    [[maybe_unused]] bool b = initialize();
    assert(b);
  }
  return *this;
}

void QCameraDevice::start() {
  assert(camera);
  camera->start();
}

void QCameraDevice::stop() {
  assert(camera);
  camera->stop();
}

QMediaObject *QCameraDevice::viewfinder() {
  assert(camera);
  return camera.get();
}

void QCameraDevice::capture([[maybe_unused]] QTextStream *verboseOut, ShutterSpeedChoice shutterSpeed) {
  assert(camera);

  if (shutterSpeed.toMicrosecond() > 0) {
    if (QCameraExposure *exposure = camera->exposure(); exposure!=nullptr) {
      exposure->setManualShutterSpeed(qreal(shutterSpeed.toMicrosecond()) / 1000000);
    }
  }

  captureRequest = true;
  if (camera->requestedLocks()==QCamera::LockType::NoLock) {
    onLocked();
  } else {
    if (camera->lockStatus()==QCamera::LockStatus::Locked && persistentFocusLockVal) {
      onLocked();
    } else {
      camera->searchAndLock();
    }
  }
}

QString QCameraDevice::backend() {
  return "Qt";
}

QString QCameraDevice::device() {
  return info.deviceName();
}

QString QCameraDevice::name() {
  return info.description();
}

QCamera::Position QCameraDevice::position() {
  return info.position();
}

QSize QCameraDevice::resolution() {
  assert(imageCapture);
  QImageEncoderSettings encoding = imageCapture->encodingSettings();
  return encoding.resolution();
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
    camera = make_unique<QCamera>(info, this);
    if (!camera->isCaptureModeSupported(QCamera::CaptureStillImage)) {
      if (verboseOut!=nullptr) {
        *verboseOut << "Still image capture is not supported by " << toString() << endl;
      }
      return false;
    }
    camera->setCaptureMode(QCamera::CaptureStillImage);
    connect(camera.get(), &QCamera::lockFailed, this, &QCameraDevice::onLockFailed);
    connect(camera.get(), &QCamera::locked, this, &QCameraDevice::onLocked);
    connect(camera.get(), &QCamera::statusChanged, this, &QCameraDevice::onStatusChanged);

    camera->load(); // load camera, probe resolution and available settings

    imageCapture = std::make_unique<QCameraImageCapture>(camera.get());
    if (!imageCapture->isCaptureDestinationSupported(QCameraImageCapture::CaptureToBuffer)) {
      if (verboseOut!=nullptr) {
        *verboseOut << "Capture to buffer is not supported by " << toString() << endl;
      }
      return false;
    }
    imageCapture->setCaptureDestination(QCameraImageCapture::CaptureToBuffer);

    if (verboseOut) {
      *verboseOut << "supported buffer formats: ";
      for (const auto &format: imageCapture->supportedBufferFormats()) {
        *verboseOut << frameFormatString(format) << " ";
      }
      *verboseOut << endl;
    }

    setupMaxResolution();

    connect(imageCapture.get(), &QCameraImageCapture::imageAvailable, this, &QCameraDevice::onImageAvailable);
    connect(imageCapture.get(), &QCameraImageCapture::readyForCaptureChanged, this, &QCameraDevice::onReadyForCaptureChanged);
  }
  return true;
}

void QCameraDevice::setupMaxResolution() {
  QSize maxSize;
  for (QSize size: imageCapture->supportedResolutions()) {
    if ((!maxSize.isValid()) || (size.width()*size.height() > maxSize.width()*maxSize.height())) {
      maxSize=size;
    }
  }
  QImageEncoderSettings encoding = imageCapture->encodingSettings();
  encoding.setQuality(QMultimedia::EncodingQuality::VeryHighQuality);
  if (maxSize.isValid() && maxSize != encoding.resolution()){
    encoding.setResolution(maxSize);
    qDebug() << "Setup resolution " << maxSize.width() << "x" << maxSize.height() << " for " << name();
    imageCapture->setEncodingSettings(encoding);
    emit update();
  }
}

void QCameraDevice::onStatusChanged(QCamera::Status status) {
  if (status == QCamera::Status::LoadedStatus) {
    setupMaxResolution();
  }
}

void QCameraDevice::onLockFailed() {
  qWarning() << "Lock failed";
}

void QCameraDevice::onLocked() {
  assert(camera);

  if (!captureRequest) {
    return;
  }

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

QString QCameraDevice::frameFormatString(QVideoFrame::PixelFormat format) {
  switch (format) {
    case QVideoFrame::Format_ARGB32: return "ARGB32";
    case QVideoFrame::Format_RGB32: return "RGB32";
    case QVideoFrame::Format_RGB24: return "RGB24";
    case QVideoFrame::Format_RGB565: return "RGB565";
    case QVideoFrame::Format_RGB555: return "RGB555";
    case QVideoFrame::Format_BGRA32: return "BGRA32";
    case QVideoFrame::Format_BGR32: return "BGR32";
    case QVideoFrame::Format_BGR24: return "BGR24";
    case QVideoFrame::Format_BGR565: return "BGR565";
    case QVideoFrame::Format_BGR555: return "BGR555";
    case QVideoFrame::Format_AYUV444: return "AYUV444";
    case QVideoFrame::Format_YUV444: return "YUV444";
    case QVideoFrame::Format_YUV420P: return "YUV420P";
    case QVideoFrame::Format_YV12: return "YV12";
    case QVideoFrame::Format_UYVY: return "UYVY";
    case QVideoFrame::Format_YUYV: return "YUYV";
    case QVideoFrame::Format_NV12: return "NV12";
    case QVideoFrame::Format_NV21: return "NV21";
    case QVideoFrame::Format_IMC1: return "IMC1";
    case QVideoFrame::Format_IMC2: return "IMC2";
    case QVideoFrame::Format_IMC3: return "IMC3";
    case QVideoFrame::Format_IMC4: return "IMC4";
    case QVideoFrame::Format_Y8: return "Y8";
    case QVideoFrame::Format_Y16: return "Y16";
    case QVideoFrame::Format_Jpeg: return "jpeg";
    case QVideoFrame::Format_CameraRaw: return "CameraRaw";
    case QVideoFrame::Format_AdobeDng: return "AdobeDng";
#if QT_VERSION >= QT_VERSION_CHECK(5,13,0)
    case QVideoFrame::Format_ABGR32: return "ABGR32";
    case QVideoFrame::Format_YUV422P: return "YUV422P";
#endif
    default: return "";
  }
}

void QCameraDevice::onImageAvailable([[maybe_unused]] int id, const QVideoFrame &constFrame) {
  captureRequest = false;
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

  QString format = frameFormatString(frame.pixelFormat());
  if (format.isEmpty()) {
    qWarning() << "Video frame format is not supported:" << frame.pixelFormat();
    return;
  }

  emit imageCaptured(format,
                     Magick::Blob(frame.bits(), frame.mappedBytes()),
                     Magick::Geometry(frame.width(), frame.height()));
  if (!persistentFocusLockVal) {
    camera->unlock();
  }
}

ShutterSpeedChoice QCameraDevice::currentShutterSpeed() {
  assert(camera);

  QCameraExposure *exposure = camera->exposure();
  if (exposure==nullptr) {
    return ShutterSpeedChoice();
  }

  ShutterSpeedChoice ch(false, 1000000, exposure->shutterSpeed() / 1000000);
  ch.normalize();
  return ch;
}

void QCameraDevice::setShutterSpeed(const ShutterSpeedChoice &shutterSpeed) {
  assert(camera);

  QCameraExposure *exposure = camera->exposure();
  if (exposure==nullptr) {
    return;
  }
  exposure->setManualShutterSpeed(double(shutterSpeed.toMicrosecond()) / 1000000.0);
  emit update();
}

QList<ShutterSpeedChoice> QCameraDevice::getShutterSpeedChoices() {
  assert(camera);

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

QString QCameraDevice::currentAperture() {
  assert(camera);

  QCameraExposure *exposure = camera->exposure();
  if (exposure==nullptr) {
    return "";
  }

  return QString::fromStdString(std::to_string(exposure->aperture()));
}

void QCameraDevice::setAperture(const QString &aperture) {
  QCameraExposure *exposure = camera->exposure();
  if (exposure==nullptr) {
    return;
  }
  if (aperture=="Auto Aperture") {
    exposure->setAutoAperture();
  } else {
    exposure->setManualAperture(aperture.toDouble());
  }
  emit update();
}

QStringList QCameraDevice::getApertureChoices() {
  assert(camera);

  QCameraExposure *exposure = camera->exposure();
  QStringList result;
  result << "Auto Aperture";
  if (exposure==nullptr) {
    return result;
  }
  for (qreal aperture: exposure->supportedApertures()) {
    result << QString::fromStdString(std::to_string(aperture));
  }
  return result;
}

QString QCameraDevice::currentIso() {
  assert(camera);

  QCameraExposure *exposure = camera->exposure();
  if (exposure==nullptr) {
    return "";
  }

  return QString::fromStdString(std::to_string(exposure->isoSensitivity()));
}

void QCameraDevice::setIso(const QString &iso) {
  QCameraExposure *exposure = camera->exposure();
  if (exposure==nullptr) {
    return;
  }
  if (iso=="Auto ISO") {
    exposure->setAutoIsoSensitivity();
  } else {
    exposure->setManualIsoSensitivity(iso.toInt());
  }
  emit update();
}

QStringList QCameraDevice::getIsoChoices() {
  assert(camera);

  QCameraExposure *exposure = camera->exposure();
  QStringList result;
  result << "Auto ISO";
  if (exposure==nullptr) {
    return result;
  }
  for (int iso: exposure->supportedIsoSensitivities()) {
    result << QString::fromStdString(std::to_string(iso));
  }
  return result;
}

QString QCameraDevice::focusName(QCameraFocus::FocusMode focus) {
  QString name;
  qDebug() << "Checking focus name: " << focus;

  switch (focus) {
    case QCameraFocus::ManualFocus:
      name = "Manual";
      break;
    case QCameraFocus::HyperfocalFocus:
      name = "Hyperfocal";
      break;
    case QCameraFocus::InfinityFocus:
      name = "Infinity";
      break;
    case QCameraFocus::AutoFocus:
      name = "Auto";
      break;
    case QCameraFocus::ContinuousFocus:
      name = "Continuous";
      break;
    case QCameraFocus::MacroFocus:
      name = "Macro";
      break;
    default:
      name = "Unknown";
      break;
  }
  return name;
}

QString QCameraDevice::focusPointName(QCameraFocus::FocusPointMode focus) {
  QString name;
  qDebug() << "Checking focus name: " << focus;

  switch (focus) {
    case QCameraFocus::FocusPointAuto:
      name = "Auto";
      break;
    case QCameraFocus::FocusPointCenter:
      name = "Center";
      break;
    case QCameraFocus::FocusPointFaceDetection:
      name = "Face";
      break;
    case QCameraFocus::FocusPointCustom:
      name = "Custom";
      break;
    default:
      name = "Unknown";
      break;
  }
  return name;
}

QString QCameraDevice::currentFocusMode() {
  assert(camera);

  QStringList result;
  QCameraFocus *focus = camera->focus();

  if (focus == nullptr || !focus->isAvailable()) {
    return "";
  }

  for (int c = (int)QCameraFocus::ManualFocus; c <= (int)QCameraFocus::MacroFocus; c++) {
    if (focus->isFocusModeSupported((QCameraFocus::FocusMode)c) &&
        focus->focusMode().testFlag((QCameraFocus::FocusMode)c) &&
        focusName((QCameraFocus::FocusMode)c) != "Unknown") {
      return focusName((QCameraFocus::FocusMode) c);
    }
  }
  return "";
}

void QCameraDevice::setFocusMode(const QString &focusModeStr){
  assert(camera);

  QCameraFocus *focus = camera->focus();

  if (focus == nullptr || !focus->isAvailable()) {
    return;
  }

  camera->unlock(QCamera::LockType::LockFocus);

  if (focusModeStr=="Manual") {
    focus->setFocusMode(QCameraFocus::ManualFocus);
  } else if (focusModeStr=="Hyperfocal") {
    focus->setFocusMode(QCameraFocus::HyperfocalFocus);
  } else if (focusModeStr=="Infinity") {
    focus->setFocusMode(QCameraFocus::InfinityFocus);
  } else if (focusModeStr=="Auto") {
    focus->setFocusMode(QCameraFocus::AutoFocus);
  } else if (focusModeStr=="Continuous") {
    focus->setFocusMode(QCameraFocus::ContinuousFocus);
  } else if (focusModeStr=="Macro") {
    focus->setFocusMode(QCameraFocus::MacroFocus);
  }
  camera->searchAndLock();
  emit update();
}

QStringList QCameraDevice::getFocusModeChoices() {
  assert(camera);

  QStringList result;
  QCameraFocus *focus = camera->focus();

  if (focus == nullptr || !focus->isAvailable()) {
    return result;
  }

  for (int c = (int)QCameraFocus::ManualFocus; c <= (int)QCameraFocus::MacroFocus; c++) {
    if (focus->isFocusModeSupported((QCameraFocus::FocusMode)c)
        && focusName((QCameraFocus::FocusMode)c) != "Unknown") {
      qDebug() << "Found support for" << (QCameraFocus::FocusMode)c;
      result << focusName((QCameraFocus::FocusMode)c);
    }
  }
  return result;
}


QString QCameraDevice::currentFocusPointMode() {
  assert(camera);

  QCameraFocus *focus = camera->focus();

  if (focus == nullptr || !focus->isAvailable()) {
    return focusPointName(QCameraFocus::FocusPointMode::FocusPointAuto);
  }
  return focusPointName(focus->focusPointMode());
}

void QCameraDevice::setFocusPointMode(const QString &focusModeStr) {
  assert(camera);

  QCameraFocus *focus = camera->focus();

  if (focus == nullptr || !focus->isAvailable()) {
    return;
  }

  camera->unlock(QCamera::LockType::LockFocus);

  if (focusModeStr=="Auto"){
    focus->setFocusPointMode(QCameraFocus::FocusPointAuto);
  } else if (focusModeStr=="Center"){
    focus->setFocusPointMode(QCameraFocus::FocusPointCenter);
  } else if (focusModeStr=="Face"){
    focus->setFocusPointMode(QCameraFocus::FocusPointFaceDetection);
  } else if (focusModeStr=="Custom"){
    focus->setFocusPointMode(QCameraFocus::FocusPointCustom);
  }
  camera->searchAndLock();
  emit update();
}

QPointF QCameraDevice::customFocusPoint() {
  QCameraFocus *focus = camera->focus();
  if (focus == nullptr || !focus->isAvailable()) {
    return QPointF() ;
  }
  return focus->customFocusPoint();
}

void QCameraDevice::setCustomFocusPoint(const QPointF &p) {
  assert(camera);

  QCameraFocus *focus = camera->focus();
  if (focus == nullptr || !focus->isAvailable()) {
    return;
  }
  camera->unlock(QCamera::LockType::LockFocus);
  focus->setCustomFocusPoint(p);
  camera->searchAndLock();
  emit update();
}

QStringList QCameraDevice::getFocusPointModeChoices() {
  assert(camera);

  QCameraFocus *focus = camera->focus();

  QStringList result;
  if (focus == nullptr || !focus->isAvailable()) {
    return result;
  }

  for (int c = (int)QCameraFocus::FocusPointMode::FocusPointAuto; c <= (int)QCameraFocus::FocusPointMode::FocusPointCustom; c++) {
    if (focus->isFocusPointModeSupported((QCameraFocus::FocusPointMode)c)
        && focusPointName((QCameraFocus::FocusPointMode)c) != "Unknown") {
      qDebug() << "Found support for" << (QCameraFocus::FocusPointMode)c;
      result << focusPointName((QCameraFocus::FocusPointMode)c);
    }
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
