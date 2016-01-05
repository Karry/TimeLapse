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

/**
 * Impementation was inspired by http://linuxtv.org/downloads/v4l-dvb-apis/v4l2grab-example.html
 * and other utils from v4l-utils repository http://git.linuxtv.org//v4l-utils.git
 */

//#include <stdio.h>
//#include <stdlib.h>
//#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/ioctl.h>
//#include <sys/types.h>
#include <sys/time.h>
#include <sys/mman.h>
//#include <linux/videodev2.h>
#include <libv4l2.h>
#include <libv4lconvert.h>

#include <QtCore/QObject>
#include <QtCore/QDebug>
#include <QtCore/QTimer>
#include <QtCore/QTextStream>
#include <QtCore/QByteArray>
#include <QtCore/QCoreApplication>
#include <QtCore/QCommandLineParser>
#include <QtCore/QProcess>

#include <QtCore/QDir>

#include "timelapse_capture.h"
#include "timelapse_capture.moc"

#include "pipeline_cpt_v4l.h"
#include "timelapse.h"
#include "black_hole_device.h"

using namespace std;
using namespace timelapse;

namespace timelapse {


  TimeLapseCapture::TimeLapseCapture(int &argc, char **argv) :
  QCoreApplication(argc, argv),
  out(stdout), err(stderr) {
  }

  TimeLapseCapture::~TimeLapseCapture() {
  }

  QStringList TimeLapseCapture::parseArguments() {
    QCommandLineParser parser;
    ErrorMessageHelper die(err.device(), &parser);

    parser.setApplicationDescription("Tool for capture sequence of images from digital camera (V4L API).");
    parser.addHelpOption();
    parser.addVersionOption();

    return QStringList();
  }

  void TimeLapseCapture::onError(QString msg) {
    emit cleanup(1);
  }

  void TimeLapseCapture::cleanup(int exitCode) {

    exit(exitCode);
  }

  void TimeLapseCapture::run() {

    parseArguments();

    V4LDevice firstDevice;
    bool assigned = false;
    QList<V4LDevice> devices = V4LDevice::listDevices();
    for (V4LDevice d : devices) {
      if (!assigned){
        firstDevice = d;
        assigned = true;
      }
      out << d.toString() << endl;
    }

    firstDevice.capture();

    emit cleanup();
  }
}

/*
 * Main method
 */
int main(int argc, char** argv) {

  TimeLapseCapture app(argc, argv);
  Magick::InitializeMagick(*argv);

  // This will run the task from the application event loop.
  QTimer::singleShot(0, &app, SLOT(run()));

  return app.exec();
}

