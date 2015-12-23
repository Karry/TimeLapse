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

#ifndef PIPELINESTAB_H
#define	PIPELINESTAB_H


#include <QtCore/QObject>
#include <QtCore/QDebug>
#include <QtCore/QTemporaryDir>
#include <QtCore/QTemporaryFile>
#include <QtCore/QIODevice>

#include <Magick++.h>

#include "libvidstab.h"

#include "input_image_info.h"
#include "pipeline_handler.h"

namespace timelapse {

#define STAB_LOG_ERROR   0
#define STAB_LOG_WARNING 1
#define STAB_LOG_INFO    2
#define STAB_LOG_VERBOSE 3

  // static streams and functions for C libstab
  QTextStream *verboseOutput;
  QTextStream *err;

  int stabLog(int type, const char* tag, const char* format, ...);
  void stabInit(QTextStream *verboseOutput, QTextStream *err);

  class StabConfig : public QObject {
    Q_OBJECT

  public:
    StabConfig();
    virtual ~StabConfig();
    FILE* openStabStateFile(const char *mode);

    VSMotionDetectConfig mdConf;
    VSTransformConfig tsConf;
    QFile *stabStateFile;
    bool dryRun;
  };

  class PipelineStabDetect : public ImageHandler {
    Q_OBJECT

  public:
    PipelineStabDetect(StabConfig *stabConf, QTextStream *verboseOutput, QTextStream *err);
    virtual ~PipelineStabDetect();

  public:
  public slots:
    void onInput(InputImageInfo info, Magick::Image img);
    void onLast();

  private:
    void init(Magick::Image img);

    StabConfig *stabConf;
    bool initialized;
    uint32_t width;
    uint32_t height;

    VSMotionDetect md;
    VSFrameInfo fi;

    FILE *f;
    QTextStream *verboseOutput;
    QTextStream *err;
  };

  class PipelineStabTransform : public ImageHandler {
    Q_OBJECT

  public:
    PipelineStabTransform(StabConfig *stabConf, QTextStream *verboseOutput, QTextStream *err);
    virtual ~PipelineStabTransform();

  public:
  public slots:
    virtual void onInput(InputImageInfo info, Magick::Image img);
    void onLast();

  private:
    void init(Magick::Image img);


    VSFrameInfo fi;
    VSTransformData td;

    VSTransformations trans; // transformations

    StabConfig *stabConf;

    bool initialized;
    uint32_t width;
    uint32_t height;

    FILE *f;
    QTextStream *verboseOutput;
    QTextStream *err;
  };

}
#endif	/* PIPELINESTAB_H */

