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

#pragma once

#include <TimeLapse/pipeline_cpt.h>

#include <Magick++.h>

#include <QtCore/QObject>
#include <QtCore/QDebug>
#include <QtMultimedia/QCameraInfo>
#include <QtMultimedia/QCamera>
#include <QtMultimedia/QCameraImageCapture>

#include <memory>

namespace timelapse {

class QCameraDevice : public CaptureDevice {
  Q_OBJECT
public slots:
  void onLockFailed();
  void onLocked();
  void onImageAvailable(int id, const QVideoFrame &frame);
  void onReadyForCaptureChanged(bool ready);
  void onStatusChanged(QCamera::Status status);

public:
  explicit QCameraDevice(const QCameraInfo &info);
  QCameraDevice(const QCameraDevice &other);
  QCameraDevice(QCameraDevice &&other) = delete;
  ~QCameraDevice() override = default;

  QCameraDevice& operator=(const QCameraDevice &other);
  QCameraDevice& operator=(QCameraDevice &&other) = delete;

  void capture(QTextStream *verboseOut, ShutterSpeedChoice shutterSpeed = ShutterSpeedChoice()) override;

  QString backend() override;
  QString device() override;
  QString name() override;
  QString toString() override;
  QString toShortString() override;
  QCamera::Position position() override;

  QSize resolution() override;

  ShutterSpeedChoice currentShutterSpeed() override;
  void setShutterSpeed(const ShutterSpeedChoice &) override;
  QList<ShutterSpeedChoice> getShutterSpeedChoices() override;

  QString currentAperture() override;
  void setAperture(const QString &) override;
  QStringList getApertureChoices() override;

  QString currentIso() override;
  void setIso(const QString &iso) override;
  QStringList getIsoChoices() override;

  static QString focusName(QCameraFocus::FocusMode focus);
  static QString focusPointName(QCameraFocus::FocusPointMode focus);

  QString currentFocusMode() override;
  void setFocusMode(const QString &) override;
  QStringList getFocusModeChoices() override;

  QString currentFocusPointMode() override;
  void setFocusPointMode(const QString &) override;
  QStringList getFocusPointModeChoices() override;

  QPointF customFocusPoint() override;
  void setCustomFocusPoint(const QPointF &p) override;

  bool focusLockSupported() override {
    return true;
  }

  bool persistentFocusLock() override {
    return persistentFocusLockVal;
  }

  void setPersistentFocusLock(bool b) override {
    persistentFocusLockVal = b;
    emit update();
  }

  static QList<QCameraDevice> listDevices(QTextStream *verboseOut);

  bool initialize(QTextStream *verboseOut=nullptr);

  void start() override;
  void stop() override;

  virtual QMediaObject* viewfinder() override;

private:
  void setupMaxResolution();

private:
  QCameraInfo info;
  std::unique_ptr<QCamera> camera;
  std::unique_ptr<QCameraImageCapture> imageCapture;
  bool postponedCapture = false;
  bool captureRequest = false;
  bool persistentFocusLockVal = true;
};

}
