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

#pragma once


#include "timelapse.h"
#include "black_hole_device.h"
#include "input_image_info.h"
#include "pipeline.h"
#include "pipeline_cpt.h"
#include "error_message_helper.h"

#include <Magick++.h>
#include <ImageMagick-6/Magick++/Color.h>

#include <QtCore/QObject>
#include <QtCore/QDebug>
#include <QtCore/QTimer>
#include <QtCore/QTextStream>
#include <QtCore/QCoreApplication>
#include <QtCore/QFileInfo>
#include <QtCore/QTemporaryDir>
#include <QtCore/QSharedPointer>

#include <exception>
#include <queue>
#include <vector>
#include <utility>

namespace timelapse {

  class TIME_LAPSE_API AdaptiveShutterSpeedAlg {
  public:
    AdaptiveShutterSpeedAlg(
            QList<ShutterSpeedChoice> shutterSpeedChoices,
            ShutterSpeedChoice currentShutterSpeed,
            ShutterSpeedChoice minShutterSpeed,
            ShutterSpeedChoice maxShutterSpeed,
            QTextStream *err,
            QTextStream *verboseOutput
            );

    virtual ~AdaptiveShutterSpeedAlg();
    virtual void update(Magick::Image img) = 0;
    virtual ShutterSpeedChoice adjustShutterSpeed() = 0;
  protected:
    QList<ShutterSpeedChoice> shutterSpeedChoices;
    ShutterSpeedChoice currentShutterSpeed;
    ShutterSpeedChoice minShutterSpeed;
    ShutterSpeedChoice maxShutterSpeed;

    QTextStream *err;
    QTextStream *verboseOutput;
  };

  class MatrixMeteringAlg : public AdaptiveShutterSpeedAlg {
  public:
    MatrixMeteringAlg(
            QList<ShutterSpeedChoice> shutterSpeedChoices,
            ShutterSpeedChoice currentShutterSpeed,
            ShutterSpeedChoice minShutterSpeed,
            ShutterSpeedChoice maxShutterSpeed,
            QTextStream *err,
            QTextStream *verboseOutput,
            int changeThreshold = 3, 
            int step = 1);

    virtual ~MatrixMeteringAlg();
    virtual void update(Magick::Image img) override;
    virtual ShutterSpeedChoice adjustShutterSpeed() override;

  protected:
    void clearHistograms();

    int changeThreshold;
    QList< uint32_t * > greyHistograms;
    int step;
  };

  class TimeLapseCapture : public QCoreApplication {
    Q_OBJECT

  public:
    TimeLapseCapture(int &argc, char **argv);
    virtual ~TimeLapseCapture();

  public slots:
    void run();
    void done();
    void cleanup(int exitCode = 0);
    void onError(const QString &msg);
    virtual void capture();
    void imageCaptured(QString format, Magick::Blob blob, Magick::Geometry sizeHint);
    QString leadingZeros(int i, int leadingZeros);

  protected:
    QList<QSharedPointer<CaptureDevice>> listDevices();
    QSharedPointer<CaptureDevice> parseArguments();
    ShutterSpeedChoice getShutterSpeed(QString optStr, QList<ShutterSpeedChoice> choices, ErrorMessageHelper *die);

  protected:
    QTextStream out;
    QTextStream err;

    QTextStream verboseOutput;
    BlackHoleDevice *blackHole;

    QDir output;
    QTimer timer;
    QSharedPointer<CaptureDevice> dev;
    QLocale frameNumberLocale;
    int capturedCnt;
    int capturedSubsequence;

    // automatic shutter speed controll
    AdaptiveShutterSpeedAlg *shutterSpdAlg;

    bool storeRawImages;

    int64_t interval;
    int32_t cnt;
  };
}
