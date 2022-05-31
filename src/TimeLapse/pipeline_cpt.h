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
    QString toString();
    int64_t toSecond();
    int64_t toMs();
    int64_t toMicrosecond();
    bool isBulb();

    void normalize();

  private:
    static int gcd(int a, int b);

    bool bulb=false;
    int divident=-1;
    int factor=-1;
  };

  class CaptureDevice : public QObject {
    Q_OBJECT
  public:

    ~CaptureDevice() override = default;

    virtual void capture(QTextStream *verboseOut, ShutterSpeedChoice shutterSpeed = ShutterSpeedChoice()) = 0;
    virtual QString toString() = 0;

    virtual QString toShortString() {
      return toString();
    }

    virtual ShutterSpeedChoice currentShutterSpeed() {
      return ShutterSpeedChoice();
    }

    virtual QList<ShutterSpeedChoice> getShutterSpeedChoices() {
      return QList<ShutterSpeedChoice>();
    };

    virtual bool isBusy() {
      return false;
    }

    virtual void start() {}
    virtual void stop() {}

  signals:
    void imageCaptured(QString type, Magick::Blob blob, Magick::Geometry sizeHint);

    // virtual PipelineCaptureSource *qObject() = 0;
    // signal: emit imageCaptured(QString type, Magick::Blob blob);
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
