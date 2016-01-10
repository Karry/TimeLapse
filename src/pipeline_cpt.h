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

#ifndef PIPELINE_CPT_H
#define	PIPELINE_CPT_H

#include <QtCore/QObject>
#include <QTimer>
#include <QtCore/QSharedPointer>

#include <Magick++.h>

#include "input_image_info.h"
#include "pipeline_source.h"

namespace timelapse {

  class CaptureDevice {
  public:

    virtual ~CaptureDevice() {
    };

    virtual Magick::Image capture() = 0;
    virtual QString toString() = 0;
  };

#define INFINITE_CNT -1

  class PipelineCaptureSource : public ImageHandler, public PipelineSource {
    Q_OBJECT
  public:
    PipelineCaptureSource(QSharedPointer<CaptureDevice> dev, uint64_t intervalMs, int32_t cnt,
            QTextStream *verboseOutput, QTextStream *err);\
        virtual ~PipelineCaptureSource();

    virtual void process();
    
  public slots:
    virtual void capture();
        virtual void onInput(InputImageInfo info, Magick::Image img);
  signals:
    void input(InputImageInfo info, Magick::Image img);
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

#endif	/* PIPELINE_CPT_H */

