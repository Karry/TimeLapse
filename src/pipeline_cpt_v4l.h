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

#ifndef PIPELINECPTV4L_H
#define	PIPELINECPTV4L_H

#include <QtCore/QObject>
#include <QtCore/QDebug>
#include <QtCore/QTimer>
#include <QtCore/QTextStream>
#include <QtCore/QCoreApplication>
#include <QtCore/QFileInfo>
#include <QtCore/QTemporaryDir>

#include <Magick++.h>

#include "black_hole_device.h"

namespace timelapse {

#define CLEAR(x) memset(&(x), 0, sizeof(x))

  struct buffer {
    void *start;
    size_t length;
  };

  class V4LDevice : public QObject {
    Q_OBJECT
  public:
    V4LDevice(QString dev = "/dev/video0");
    V4LDevice(const timelapse::V4LDevice& other);
    virtual ~V4LDevice();

    Magick::Image capture();

    QString toString();
    V4LDevice operator=(const timelapse::V4LDevice&);

    static void ioctl(int fh, unsigned long int request, void *arg);
    static QList<V4LDevice> listDevices(QDir devDir = QDir("/dev"), int max = 32);

  protected:
    void initialize();
    int open();

    bool initialized;
    QString dev;
    struct v4l2_format dst;
    struct v4l2_format src;

  };


}

#endif	/* PIPELINECPTV4L_H */

