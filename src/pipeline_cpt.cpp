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
    if (cnt >= 0 && cnt >= capturedCnt) {
      timer->stop();
      return;
    }

    try {
      Magick::Image img = dev->capture();

      InputImageInfo ii;
      ii.width = img.columns();
      ii.height = img.rows();
      ii.frame = capturedCnt;

      emit input(ii, img);
      capturedCnt++;
    } catch (std::exception &e) {
      *err << "Capturing failed: " << e.what() << endl;
      emit error(e.what());
    }
  }

  void PipelineCaptureSource::onInput(InputImageInfo info, Magick::Image img) {
    // ignore, we are the source
  }

}
