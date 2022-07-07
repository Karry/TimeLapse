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

#include <TimeLapse/timelapse.h>
#include <TimeLapse/pipeline_cpt.h>

#include <QObject>
#include <QStringList>
#include <QTextStream>
#include <QLocale>

#include <cstdio>

using namespace std;
using namespace timelapse;

namespace timelapse {

#define GREY_HISTOGRAM_RESOLUTION 256
#define UNDEREXPOSURE_GREY_LIMIT 13
#define OVEREXPOSURE_GREY_LIMIT 242
#define UNDEREXPOSURE_RATIO_LIMIT .05
#define OVEREXPOSURE_RATIO_LIMIT .05
#define BULB_CHANGE_s 5

class TIME_LAPSE_API AdaptiveShutterSpeedAlg {
public:
  AdaptiveShutterSpeedAlg(
    QList<ShutterSpeedChoice> shutterSpeedChoices,
    ShutterSpeedChoice currentShutterSpeed,
    ShutterSpeedChoice minShutterSpeed,
    ShutterSpeedChoice maxShutterSpeed,
    QTextStream *err,
    QTextStream *verboseOutput
  );

  virtual ~AdaptiveShutterSpeedAlg() = default;
  virtual void update(Magick::Image img) = 0;
  virtual ShutterSpeedChoice adjustShutterSpeed() = 0;
protected:
  QList<ShutterSpeedChoice> shutterSpeedChoices;
  ShutterSpeedChoice currentShutterSpeed;
  ShutterSpeedChoice minShutterSpeed;
  ShutterSpeedChoice maxShutterSpeed;

  QTextStream *err;
  QTextStream *verboseOutput;
};

class TIME_LAPSE_API MatrixMeteringAlg : public AdaptiveShutterSpeedAlg {
public:
  MatrixMeteringAlg(
    QList<ShutterSpeedChoice> shutterSpeedChoices,
    ShutterSpeedChoice currentShutterSpeed,
    ShutterSpeedChoice minShutterSpeed,
    ShutterSpeedChoice maxShutterSpeed,
    QTextStream *err,
    QTextStream *verboseOutput,
    int changeThreshold = 3,
    int step = 1);

  virtual ~MatrixMeteringAlg();
  virtual void update(Magick::Image img) override;
  virtual ShutterSpeedChoice adjustShutterSpeed() override;

protected:
  void clearHistograms();

  int changeThreshold;
  QList< uint32_t * > greyHistograms;
  int step;
};

class TIME_LAPSE_API TimeLapseCapture: public QObject {
  Q_OBJECT

  Q_PROPERTY(QSharedPointer<CaptureDevice> device READ getDevice WRITE setDevice)

  Q_PROPERTY(qint64 interval READ getInterval WRITE setInterval)
  Q_PROPERTY(QDir output READ getOutput  WRITE setOutput)
  Q_PROPERTY(qint64 count READ getCount WRITE setCount)

  Q_PROPERTY(ShutterSpeedChoice minShutterSpeed READ getMinShutterSpeed WRITE setMinShutterSpeed)
  Q_PROPERTY(ShutterSpeedChoice maxShutterSpeed READ getMaxShutterSpeed WRITE setMaxShutterSpeed)
  Q_PROPERTY(int shutterSpeedChangeThreshold READ getShutterSpeedChangeThreshold WRITE setShutterSpeedChangeThreshold)
  Q_PROPERTY(int shutterSpeedStep READ getShutterSpeedStep WRITE setShutterSpeedStep)

  Q_PROPERTY(bool storeRawImages READ getStoreRawImages WRITE setStoreRawImages)

  Q_PROPERTY(int capturedCount READ getCapturedCount NOTIFY capturedCountChanged)

signals:
  void imageCaptured(QString file);
  void capturedCountChanged();
  void done();
  void error(const QString &msg);

public slots :
  virtual void start();
  void onImageCaptured(QString format, Magick::Blob blob, Magick::Geometry sizeHint);
  void onCameraBusyChanged();
  void capture();

public:
  explicit TimeLapseCapture(QTextStream* err, QTextStream* verboseOutput);
  TimeLapseCapture(const TimeLapseCapture&) = delete;
  TimeLapseCapture(TimeLapseCapture&&) = delete;
  ~TimeLapseCapture() override;
  TimeLapseCapture& operator=(const TimeLapseCapture&) = delete;
  TimeLapseCapture& operator=(TimeLapseCapture&&) = delete;

  qint64 getInterval() {
    return _interval;
  }

  QDir getOutput() {
    return _output;
  }

  qint64 getCount() {
    return _count;
  }

  void setInterval(qint64 interval) {
    _interval=interval;
  }

  void setOutput(const QDir &dir) {
    _output=dir;
  }

  void setCount(qint64 count) {
    _count=count;
  }

  bool getStoreRawImages() const {
    return _storeRawImages;
  }

  void setStoreRawImages(bool b) {
    _storeRawImages=b;
  }

  QSharedPointer<timelapse::CaptureDevice> getDevice() {
    return dev;
  }

  void setDevice(QSharedPointer<timelapse::CaptureDevice> dev) {
    this->dev = dev;
  }

  ShutterSpeedChoice getMinShutterSpeed () {
    return _minShutterSpeed;
  }

  void setMinShutterSpeed(const ShutterSpeedChoice& m) {
    _minShutterSpeed = m;
  }

  ShutterSpeedChoice getMaxShutterSpeed() {
    return _maxShutterSpeed;
  }

  void setMaxShutterSpeed(const ShutterSpeedChoice& m) {
    _maxShutterSpeed = m;
  }

  int getShutterSpeedChangeThreshold() {
    return _shutterSpeedChangeThreshold;
  }

  void setShutterSpeedChangeThreshold(int t) {
    _shutterSpeedChangeThreshold = t;
  }

  int getShutterSpeedStep() {
    return _shutterSpeedStep;
  }

  void setShutterSpeedStep(int s) {
    _shutterSpeedStep = s;
  }

  int getCapturedCount() {
    return capturedCnt;
  }

private:
  QString leadingZeros(int i, int leadingZeros);
  void shutdown();

private:
  QTextStream* err; // not owned
  QTextStream* verboseOutput; // not owned
  QTimer timer;
  QSharedPointer<timelapse::CaptureDevice> dev;
  QDir _output;
  QLocale frameNumberLocale=QLocale::c();
  int capturedCnt=0;
  int capturedSubsequence=0;

  ShutterSpeedChoice _minShutterSpeed;
  ShutterSpeedChoice _maxShutterSpeed;
  int _shutterSpeedChangeThreshold=0;
  int _shutterSpeedStep=1;

  AdaptiveShutterSpeedAlg *shutterSpdAlg=nullptr;

  qint64 _interval=10000;
  qint64 _count=-1;

  bool _storeRawImages;
  bool postponedCapture=false;
};
}
