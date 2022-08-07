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
#include <TimeLapse/pipeline_handler.h>

#include <Magick++.h>

#include <QtCore/QObject>
#include <QtCore/QDebug>
#include <QtCore/QTimer>
#include <QtCore/QTextStream>
#include <QtCore/QCoreApplication>
#include <QtCore/QFileInfo>
#include <QtCore/QTemporaryDir>

namespace timelapse {

  class TimeLapseAssembly : public QCoreApplication {
    Q_OBJECT

  public:
    TimeLapseAssembly(int &argc, char **argv);
    virtual ~TimeLapseAssembly();

  public slots:
    void run();
    void cleanup();
    void onError(const QString &msg);

  signals:
    void done();

  protected:
    QStringList parseArguments();
    void cleanup2(int exitCode);

  protected:
    QTextStream _out;
    QTextStream _err;
    QStringList _extensions;
    bool _dryRun;
    bool deflickerAvg;
    bool deflickerDebugView;
    size_t wmaCount;
    QTextStream _verboseOutput;
    BlackHoleDevice *_blackHole;
    bool _forceOverride;
    QString _tmpBaseDir;

    QTemporaryDir *_tempDir;

    /* output properties*/
    /* output file name */
    QFileInfo _output;

    /* output video dimensions. Default is 1920x1080 */
    int _width;
    int _height;

    bool _adaptiveResize;

    /* output video fps, default 25 */
    float _fps;

    /* length of output video in seconds. 
     * if length < 0, then length will be count of inputs images / fps
     */
    float _length;
    int _frameCount;
    QString _bitrate;
    QString _codec;

    /* It is useful when time interval between images is not fixed.
     * Input image to output video frame mapping will be computed from image 
     * timestamp (EXIF metadata will be used).
     */
    bool _noStrictInterval;
    bool _blendFrames;
    bool _blendBeforeResize;

    Pipeline *pipeline;
  };
}
