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

#include <TimeLapse/timelapse.h>
#include <TimeLapse/black_hole_device.h>
#include <TimeLapse/pipeline_cpt.h>

#include <Magick++.h>

#include <libv4l2.h>
#include <linux/videodev2.h>

#include <QtCore/QObject>
#include <QtCore/QDebug>
#include <QtCore/QTimer>
#include <QtCore/QTextStream>
#include <QtCore/QCoreApplication>
#include <QtCore/QFileInfo>
#include <QtCore/QTemporaryDir>

namespace timelapse {

  struct buffer {
    void *start;
    size_t length;
  };

  class TIME_LAPSE_API V4LDevice : public CaptureDevice {
    Q_OBJECT
  public:
    V4LDevice(QString dev = "/dev/video0");
    V4LDevice(const timelapse::V4LDevice& other);
    V4LDevice(timelapse::V4LDevice&& other) = delete;
    ~V4LDevice() override = default;

    V4LDevice& operator=(const timelapse::V4LDevice&);
    V4LDevice& operator=(timelapse::V4LDevice&&) = delete;

    void capture(QTextStream *verboseOut, ShutterSpeedChoice shutterSpeed = ShutterSpeedChoice()) override;

    QString backend() override;
    QString device() override;
    QString name() override;
    QString toString() override;
    QString toShortString() override;

    QSize resolution() override;

    //virtual PipelineCaptureSource* qObject();

    static void ioctl(int fh, unsigned long int request, void *arg);
    static QList<V4LDevice> listDevices(QTextStream *verboseOut, QDir devDir = QDir("/dev"));

    void initialize();

  protected:
    int open();

    bool initialized;
    QString dev;
    struct v4l2_capability capability;
    struct v4l2_format v4lfmt;

  };

}
