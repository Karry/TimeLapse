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

#ifndef PIPELINE_SOURCE_H
#define	PIPELINE_SOURCE_H

#include <QtCore/QObject>
#include <QtCore/QDebug>
#include <QtCore/QTemporaryDir>

#include <Magick++.h>

#include "input_image_info.h"
#include "pipeline_handler.h"

namespace timelapse {

  class PipelineSource {
  public:

    virtual ~PipelineSource() {
    };
    virtual void process() = 0;
  };

  class PipelineFileSource : public InputHandler, public PipelineSource {
    Q_OBJECT
  public:
    PipelineFileSource(QStringList inputArguments, bool recursive, QTextStream *verboseOutput, QTextStream *err);
    virtual void process();
  protected:
    QList<InputImageInfo> listDirectory(QDir d);
    QList<InputImageInfo> parseArguments();
  public slots:
    virtual void onInput(InputImageInfo info);
  private:
    QStringList inputArguments;
    bool recursive;
    QTextStream *verboseOutput;
    QTextStream *err;
  };
}

#endif	/* PIPELINE_SOURCE_H */

