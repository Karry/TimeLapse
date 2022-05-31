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

#include <TimeLapse/black_hole_device.h>
#include <TimeLapse/input_image_info.h>
#include <TimeLapse/pipeline.h>
#include <TimeLapse/pipeline_stab.h>

#include <TimeLapse/libvidstab.h>

#include <Magick++.h>

#include <QtCore/QObject>
#include <QtCore/QDebug>
#include <QtCore/QTimer>
#include <QtCore/QTextStream>
#include <QtCore/QCoreApplication>
#include <QtCore/QFileInfo>
#include <QtCore/QTemporaryDir>

namespace timelapse {

  typedef struct {
    //const AVClass *class;

    VSMotionDetect md;
    VSMotionDetectConfig conf;

    char *result;
    FILE *f;
  } StabData;

  typedef struct {
    //const AVClass *class;

    VSTransformData td;
    VSTransformConfig conf;

    VSTransformations trans; // transformations
    char *input; // name of transform file
    int tripod;
    int debug;
  } TransformContext;

  class TimeLapseStabilize : public QCoreApplication {
    Q_OBJECT

  public:
    TimeLapseStabilize(int &argc, char **argv);
    virtual ~TimeLapseStabilize();

  public slots:
    void run();
    void onError(const QString &msg);
    void cleanup();

  protected:
    QStringList parseArguments();
    void cleanup2(int exitCode);

  protected:
    QTextStream out;
    QTextStream err;

    QTextStream verboseOutput;
    BlackHoleDevice *blackHole;

    Pipeline *pipeline;
    StabConfig *stabConf;
    QDir output;

    bool dryRun;
    QTemporaryDir *tempDir;
  };
}
