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

#include "pipeline_cpt.h"
#include "pipeline_cpt.moc"

#include "timelapse.h"

#include <QTimer>

using namespace std;

namespace timelapse {

  ShutterSpeedChoice::ShutterSpeedChoice(bool bulb, int divident, int factor) :
  bulb(bulb), divident(divident), factor(factor) {
    //normalize();
  }

  ShutterSpeedChoice &ShutterSpeedChoice::operator=(const ShutterSpeedChoice &o)
  {
    bulb=o.bulb;
    divident=o.divident;
    factor=o.factor;
    return *this;
  }

  ShutterSpeedChoice::ShutterSpeedChoice(const QString str) : bulb(false), divident(-1), factor(-1) {
    bool ok;

    if (str.startsWith("BULB", Qt::CaseInsensitive)) {
      bulb = true;
      QStringList list = str.split(':', QString::SkipEmptyParts);
      if (list.size() >= 2) {
        divident = list[1].toInt(&ok);
        if (!ok)
          throw std::invalid_argument("Can't parse Bulb time");
        factor = 1;
      }
    } else {
      QStringList list = str.split('/', QString::SkipEmptyParts);
      if (list.size() < 2) {
        divident = str.toInt(&ok);
        if (!ok)
          throw std::invalid_argument("Can't parse shutter speed");
        factor = 1;
      } else {
        divident = list[0].toInt(&ok);
        if (!ok)
          throw std::invalid_argument("Can't parse shutter speed");
        factor = list[1].toInt(&ok);
        if (!ok)
          throw std::invalid_argument("Can't parse shutter speed");
      }
    }
    //normalize();
  }

  ShutterSpeedChoice::~ShutterSpeedChoice() {
  }

    /*
    int ShutterSpeedChoice::gcd(int a, int b) {
      int c;
      while (b != 0) {
        c = b;
        b = a % b;
        a = c;
      }
      return a;
    }

    void ShutterSpeedChoice::normalize() {
      if (toMicrosecond() > 0) {
        if (factor > 1) {
          int gcd = ShutterSpeedChoice::gcd(divident, factor);
          if (gcd > 1) {
            divident /= gcd;
            factor /= gcd;
          }
        }
      }
    }
   */
  QString ShutterSpeedChoice::toString() {
    if (bulb) {
      if (divident > 0) {
        return QString("Bulb:%1").arg(divident);
      } else {
        return "Bulb";
      }
    } else {
      if (divident <= 0 || factor <= 0) {
        return "Unknown";
      } else {
        if (factor == 1) {
          return QString("%1").arg(divident);
        } else {
          return QString("%1/%2").arg(divident).arg(factor);
        }
      }
    }
  }

  int64_t ShutterSpeedChoice::toSecond() {
    int64_t us = toMicrosecond();
    if (us < 0)
      return us;
    return us / 1000000;
  }

  int64_t ShutterSpeedChoice::toMs() {
    int64_t us = toMicrosecond();
    if (us < 0)
      return us;
    return us / 1000;
  }

  int64_t ShutterSpeedChoice::toMicrosecond() {
    int64_t us = 1000000;
    if (bulb) {
      if (divident > 0) {
        return ((int64_t) divident) * us;
      } else {
        return -1;
      }
    } else {
      if (divident <= 0 || factor <= 0) {
        return -1;
      } else {
        return ( ((int64_t) divident) * us) / factor;
      }
    }
  }

  bool ShutterSpeedChoice::isBulb() {
    return bulb;
  }

  PipelineCaptureSource::PipelineCaptureSource(QSharedPointer<CaptureDevice> dev, uint64_t intervalMs, int32_t cnt,
    QTextStream *verboseOutput, QTextStream *err) :
  dev(dev), intervalMs(intervalMs), capturedCnt(0), cnt(cnt),
  verboseOutput(verboseOutput), err(err), timer(nullptr) {

    timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, &PipelineCaptureSource::capture);
    connect(dev.get(), &CaptureDevice::imageCaptured,
      this, &PipelineCaptureSource::imageCaptured);
  }

  PipelineCaptureSource::~PipelineCaptureSource() {
    //delete dev;
    if (timer != nullptr) {
      timer->stop();
      delete timer;
      timer = nullptr;
    }
  }

  void PipelineCaptureSource::process() {
    *verboseOutput << "Start timer with interval " << intervalMs << " ms" << endl;
    timer->start(intervalMs);
    capture();
  }

  void PipelineCaptureSource::capture() {
    if (cnt >= 0 && capturedCnt >= cnt) {
      timer->stop();
      emit last();
      return;
    }

    try {
      dev->capture(verboseOutput);
      capturedCnt++;
    } catch (std::exception &e) {
      *err << "Capturing failed: " << QString::fromUtf8(e.what()) << endl;
      emit error(e.what());
    }
  }

  void PipelineCaptureSource::imageCaptured(QString format, Magick::Blob blob, Magick::Geometry sizeHint) {
    Magick::Image capturedImage;


    if (format == "RGB") {
      capturedImage.read(blob, sizeHint, 8, "RGB");
    } else {
      capturedImage.read(blob, format.toStdString());
    }

    QDateTime now = QDateTime::currentDateTime();
    QString exifDateTime = now.toString("yyyy:MM:dd HH:mm:ss");\
          
    // ImageMagick don't support writing of exif data
    // TODO: setup exif timestamp correctly
    capturedImage.attribute("EXIF:DateTime", exifDateTime.toStdString());
    //capturedImage.defineValue("EXIF", "DateTime", exifDateTime.toStdString());

    InputImageInfo ii;
    ii.width = capturedImage.columns();
    ii.height = capturedImage.rows();
    ii.frame = capturedCnt;

    emit inputImg(ii, capturedImage);
  }

  void PipelineCaptureSource::onInputImg([[maybe_unused]] InputImageInfo info, [[maybe_unused]] Magick::Image img) {
    // ignore, we are the source
  }

}
