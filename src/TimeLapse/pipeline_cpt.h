/*
 *   Copyright (C) 2016 Lukáš Karas <lukas.karas@centrum.cz>
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
#include <TimeLapse/input_image_info.h>
#include <TimeLapse/pipeline_source.h>

#include <Magick++.h>

#include <QtCore/QObject>
#include <QTimer>
#include <QtCore/QSharedPointer>
#include <QtMultimedia/QCamera>
#include <QtMultimedia/QMediaObject>

namespace timelapse {

#define CLEAR(x) memset(&(x), 0, sizeof(x))
#define ALLOC_CHECK(ptr) { if ((ptr)==nullptr) throw std::runtime_error("Allocation failure"); }

  class TIME_LAPSE_API ShutterSpeedChoice {
  public:
    ShutterSpeedChoice() = default;
    ShutterSpeedChoice(const ShutterSpeedChoice &o) = default;
    ShutterSpeedChoice(bool bulb, int divident, int factor);
    explicit ShutterSpeedChoice(const QString str);
    virtual ~ShutterSpeedChoice();
    ShutterSpeedChoice &operator=(const ShutterSpeedChoice &o);
    QString toString() const;
    int64_t toSecond() const;
    int64_t toMs() const;
    int64_t toMicrosecond() const;
    bool isBulb() const;

    void normalize();

    bool operator==(const ShutterSpeedChoice&) const;

    bool operator!=(const ShutterSpeedChoice &o) const {
      return !(*this == o);
    }

  private:
    static int gcd(int a, int b);

    bool bulb=false;
    int divident=-1;
    int factor=-1;
  };

  class CaptureDevice : public QObject {
    Q_OBJECT

    Q_PROPERTY(bool busy READ isBusy NOTIFY busyChanged)
    Q_PROPERTY(QString batteryLevel READ getBatteryLevel NOTIFY batteryLevelChanged)

  signals:
    void imageCaptured(QString type, Magick::Blob blob, Magick::Geometry sizeHint);

    // camera properties was update (resolution, shutter speed...)
    void update();

    void busyChanged();
    void batteryLevelChanged();
  public:

    ~CaptureDevice() override = default;

    virtual void capture(QTextStream *verboseOut, ShutterSpeedChoice shutterSpeed = ShutterSpeedChoice()) = 0;
    /// Qt, V4l, Gphoto2
    virtual QString backend() = 0;
    /// hardware device ("usb:001,002", "/dev/video0" ...)
    virtual QString device() = 0;
    /// name of the camera ("Integrated_Webcam_HD", "Nikon D5100" ...)
    virtual QString name() = 0;

    virtual QString toString() = 0;

    virtual QString toShortString() {
      return toString();
    }

    virtual QString getBatteryLevel() const {
      return "";
    }

    virtual QCamera::Position position() {
      return QCamera::Position::UnspecifiedPosition;
    }

    /** Camera resolution. When more are supported, "best" should be selected.
     * May be invalid, when it is not known.
     */
    virtual QSize resolution() {
      return QSize();
    }

    virtual ShutterSpeedChoice currentShutterSpeed() {
      return ShutterSpeedChoice();
    }

    virtual void setShutterSpeed(const ShutterSpeedChoice &) {}

    virtual QList<ShutterSpeedChoice> getShutterSpeedChoices() {
      return QList<ShutterSpeedChoice>();
    };

    virtual QString currentAperture() {
      return "";
    }

    virtual void setAperture(const QString &) {}

    virtual QStringList getApertureChoices() {
      return QStringList();
    }

    virtual QString currentIso() {
      return "";
    }

    virtual void setIso(const QString &) {}

    virtual QStringList getIsoChoices() {
      return QStringList();
    }

    virtual QString currentFocusMode() {
      return "";
    }

    virtual void setFocusMode(const QString &) {}

    virtual QStringList getFocusModeChoices() {
      return QStringList();
    }

    virtual QString currentFocusPointMode() {
      return "";
    }

    virtual void setFocusPointMode(const QString &) {}

    virtual QStringList getFocusPointModeChoices() {
      return QStringList();
    }

    virtual QPointF customFocusPoint() {
      return QPointF();
    }

    virtual void setCustomFocusPoint(const QPointF &) {}

    virtual bool focusLockSupported() {
      return false;
    }

    virtual bool persistentFocusLock() {
      return false;
    }

    virtual void setPersistentFocusLock(bool) { }

    virtual bool isBusy() {
      return false;
    }

    virtual void start() {}
    virtual void stop() {}

    /** Viewfinder video source, or null when it is not supported.
     * Returned object is owned by capture device.
     * @return
     */
    virtual QMediaObject* viewfinder() {
      return nullptr;
    }
  };

#define INFINITE_CNT -1

  class PipelineCaptureSource : public ImageHandler, public PipelineSource {
    Q_OBJECT
  public:
    PipelineCaptureSource(QSharedPointer<CaptureDevice> dev, uint64_t intervalMs, int32_t cnt,
            QTextStream *verboseOutput, QTextStream *err);
    ~PipelineCaptureSource() override;

    void process() override;

  public slots:
    virtual void capture();
    void onInputImg(InputImageInfo info, Magick::Image img) override;
    void imageCaptured(QString format, Magick::Blob blob, Magick::Geometry sizeHint);

  private:
    QSharedPointer<CaptureDevice> dev;
    uint64_t intervalMs;
    int32_t capturedCnt;
    int32_t cnt;
    QTextStream *verboseOutput;
    QTextStream *err;
    QTimer *timer;
  };

}
