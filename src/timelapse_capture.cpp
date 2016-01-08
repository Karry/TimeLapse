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
//#include <libv4lconvert.h>

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

#include "timelapse.h"
#include "black_hole_device.h"
#include "pipeline_frame_mapping.h"
#include "pipeline_cpt.h"
#include "pipeline_cpt_v4l.h"
#include "pipeline_write_frame.h"

using namespace std;
using namespace timelapse;

namespace timelapse {

  TimeLapseCapture::TimeLapseCapture(int &argc, char **argv) :
  QCoreApplication(argc, argv),
  out(stdout), err(stderr),
  verboseOutput(stdout), blackHole(NULL),
  pipeline(NULL), output(),
  dryRun(false), dev(NULL), interval(10000), cnt(-1) {

    setApplicationName("TimeLapse capture tool");
    setApplicationVersion(VERSION_TIMELAPSE);

  }

  TimeLapseCapture::~TimeLapseCapture() {
    if (blackHole != NULL) {
      verboseOutput.flush();
      verboseOutput.setDevice(NULL);
      delete blackHole;
      blackHole = NULL;
    }
    if (dev != NULL) {
      delete dev;
      dev = NULL;
    }
    if (pipeline != NULL) {
      delete pipeline;
      pipeline = NULL;
    }
  }

  QStringList TimeLapseCapture::parseArguments() {
    QCommandLineParser parser;
    ErrorMessageHelper die(err.device(), &parser);

    parser.setApplicationDescription("Tool for capture sequence of images from digital camera (V4L API).");
    parser.addHelpOption();
    parser.addVersionOption();

    QCommandLineOption outputOption(QStringList() << "o" << "output",
      QCoreApplication::translate("main", "Output directory."),
      QCoreApplication::translate("main", "directory"));
    parser.addOption(outputOption);

    QCommandLineOption verboseOption(QStringList() << "V" << "verbose",
      QCoreApplication::translate("main", "Verbose output."));
    parser.addOption(verboseOption);

    QCommandLineOption deviceOption(QStringList() << "d" << "device",
      QCoreApplication::translate("main", "Capture device."),
      QCoreApplication::translate("main", "device"));
    parser.addOption(deviceOption);

    QCommandLineOption intervalOption(QStringList() << "i" << "interval",
      QCoreApplication::translate("main", "Capture interval (in milliseconds). Default is 10000."),
      QCoreApplication::translate("main", "interval"));
    parser.addOption(intervalOption);

    QCommandLineOption cntOption(QStringList() << "c" << "count",
      QCoreApplication::translate("main", "How many images should be captured. Default value is infinite."),
      QCoreApplication::translate("main", "count"));
    parser.addOption(cntOption);

    // Process the actual command line arguments given by the user
    parser.process(*this);

    // output
    if (!parser.isSet(outputOption))
      die << "Output directory is not set";
    output = QDir(parser.value(outputOption));
    if (output.exists())
      err << "Output directory exists already." << endl;
    if (!output.mkpath("."))
      die << QString("Can't create output directory %1 !").arg(output.path());

    // verbose?
    if (!parser.isSet(verboseOption)) {
      blackHole = new BlackHoleDevice();
      verboseOutput.setDevice(blackHole);
    } else {
      // verbose
      verboseOutput << "Turning on verbose output..." << endl;
      verboseOutput << applicationName() << " " << applicationVersion() << endl;
    }

    if (parser.isSet(intervalOption)) {
      bool ok = false;
      long i = parser.value(intervalOption).toLong(&ok);
      if (!ok) die << "Cant parse interval.";
      if (i <= 0) die << "Interval have to be possitive";
      interval = i;
    }
    if (parser.isSet(cntOption)) {
      bool ok = false;
      int i = parser.value(intervalOption).toInt(&ok);
      if (!ok) die << "Cant parse count.";
      cnt = i;
    }
    if (!parser.isSet(deviceOption)) {
      V4LDevice firstDevice;
      bool assigned = false;
      QList<V4LDevice> devices = V4LDevice::listDevices(&verboseOutput);
      verboseOutput << "Found devices: " << endl;
      for (V4LDevice d : devices) {
        if (!assigned) {
          firstDevice = d;
          assigned = true;
        }
        verboseOutput << "  " << d.toString() << endl;
      }
      if (!assigned)
        die << "No supported device.";
      out << "Using device " << firstDevice.toString() << endl;
      dev = new V4LDevice(firstDevice);
    } else {
      QString devVal = parser.value(intervalOption);
      try {
        V4LDevice *d = new V4LDevice(devVal);
        dev = d;
        d->initialize();
      } catch (std::exception &e) {
        die << QString("Device %1 can't be used for capturing.").arg(devVal);
      }
    }

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

    // build processing pipeline
    pipeline = Pipeline::createWithCaptureSource(dev, interval, cnt, &verboseOutput, &err);

    *pipeline << new WriteFrame(output, &verboseOutput, dryRun);

    connect(pipeline, SIGNAL(done()), this, SLOT(cleanup()));
    connect(pipeline, SIGNAL(error(QString)), this, SLOT(onError(QString)));

    // startup pipeline
    emit pipeline->process();
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

