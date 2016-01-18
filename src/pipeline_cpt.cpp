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

#include <QTimer>

#include "pipeline_cpt.h"
#include "pipeline_cpt.moc"

using namespace std;

namespace timelapse {

  PipelineCaptureSource::PipelineCaptureSource(QSharedPointer<CaptureDevice> dev, uint64_t intervalMs, int32_t cnt,
    QTextStream *verboseOutput, QTextStream *err) :
  dev(dev), intervalMs(intervalMs), capturedCnt(0), cnt(cnt),
  verboseOutput(verboseOutput), err(err), timer(NULL) {

    timer = new QTimer(this);
    connect(timer, SIGNAL(timeout()), this, SLOT(capture()));
    connect(dev->qObject(), SIGNAL(imageCaptured(QString, Magick::Blob, Magick::Geometry)),
      this, SLOT(imageCaptured(QString, Magick::Blob, Magick::Geometry)));
  }

  PipelineCaptureSource::~PipelineCaptureSource() {
    //delete dev;
    if (timer != NULL) {
      timer->stop();
      delete timer;
      timer = NULL;
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
      dev->capture();
      capturedCnt++;
    } catch (std::exception &e) {
      *err << "Capturing failed: " << e.what() << endl;
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

    emit input(ii, capturedImage);
  }

  void PipelineCaptureSource::onInput(InputImageInfo info, Magick::Image img) {
    // ignore, we are the source
  }

}
