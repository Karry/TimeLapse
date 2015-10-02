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

#ifndef PIPELINE_H
#define	PIPELINE_H

#include <QtCore/QObject>
#include <QtCore/QDebug>

#include <Magick++.h>

#include "input_image_info.h"
#include "pipeline_handler.h"

using namespace std;
using namespace timelapse;

namespace timelapse {

  class Pipeline : public QObject {
    Q_OBJECT

  public:
    Pipeline(QList<InputImageInfo> inputs, QTextStream *verboseOutput);
    virtual ~Pipeline();

    void operator<<(ImageHandler *handler);
    void operator<<(InputHandler *handler);

  public slots:
    void process();
    void handlerFinished();
  signals:
    void done();

  private:
    void append(PipelineHandler* handler);

  private:
    QTextStream *verboseOutput;
    QList<PipelineHandler*> elements;
    PipelineSource *src;
    InputHandler *lastInputHandler;
    ImageHandler *lastImageHandler;
  };
}
#endif	/* PIPELINE_H */

