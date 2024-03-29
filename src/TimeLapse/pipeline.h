/*
 *   Copyright (C) 2015 Lukáš Karas <lukas.karas@centrum.cz>
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
#include <TimeLapse/pipeline_handler.h>
#include <TimeLapse/pipeline_source.h>
#include <TimeLapse/pipeline_cpt.h>

#include <QtCore/QObject>
#include <QtCore/QDebug>
#include <QtCore/QSharedPointer>

#include <Magick++.h>

using namespace std;
using namespace timelapse;

namespace timelapse {

  class TIME_LAPSE_API Pipeline : public QObject {
    Q_OBJECT

  protected:
    Pipeline(PipelineSource *src, InputHandler *firstInputHandler,
             QTextStream *verboseOutput, QTextStream *err);
    Pipeline(PipelineSource *src, ImageHandler *firstImageHandler,
            QTextStream *verboseOutput, QTextStream *err);

  public:
    virtual ~Pipeline();

    void operator<<(ImageHandler *handler);
    void operator<<(InputHandler *handler);

    static Pipeline* createWithCaptureSource(QSharedPointer<CaptureDevice> dev, int64_t interval, int32_t cnt,
            QTextStream *verboseOutput, QTextStream *err);
    static Pipeline* createWithFileSource(const QStringList &inputArguments, const QStringList &fileSuffixes, bool recursive,
            QTextStream *verboseOutput, QTextStream *err);


  public slots:
    void process();
    void handlerFinished();
    void onError(QString msg);

  signals:
    void error(QString msg);
    void done();
    void imageLoaded(int stage, int cnt);

  private:
    void append(PipelineHandler* handler);

  private:
    QTextStream *verboseOutput;
    QTextStream *err;
    QList<PipelineHandler*> elements;
    PipelineSource *src;
    int stage=0;
    InputHandler *lastInputHandler;
    ImageHandler *lastImageHandler;
  };
}
