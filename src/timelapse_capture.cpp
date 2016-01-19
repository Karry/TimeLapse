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
 * V4L impementation was inspired by http://linuxtv.org/downloads/v4l-dvb-apis/v4l2grab-example.html
 * and other utils from v4l-utils repository http://git.linuxtv.org//v4l-utils.git
 * 
 * GPhoto2 code was copied and inspired by code:
 * 
 * qtmultimedia-gphoto : 
 *    https://github.com/dept2/qtmultimedia-gphoto
 *    LGPL 2.1 Copyright © 2014 Boris Moiseev
 * 
 * gphoto2:
 *    https://github.com/gphoto/gphoto2
 *    GPL 2, various authors
 * 
 */


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
#include "pipeline_cpt.h"
#include "pipeline_cpt_v4l.h"
#include "pipeline_cpt_gphoto2.h"
#include "pipeline_write_frame.h"

using namespace std;
using namespace timelapse;

namespace timelapse {

  TimeLapseCapture::TimeLapseCapture(int &argc, char **argv) :
  QCoreApplication(argc, argv),
  out(stdout), err(stderr),
  verboseOutput(stdout), blackHole(NULL),
  output(), timer(), frameNumberLocale(QLocale::c()), capturedCnt(0),
  dryRun(false), interval(10000), cnt(-1) {

    setApplicationName("TimeLapse capture tool");
    setApplicationVersion(VERSION_TIMELAPSE);
    frameNumberLocale.setNumberOptions(QLocale::OmitGroupSeparator);
  }

  TimeLapseCapture::~TimeLapseCapture() {

    timer.stop();

    if (blackHole != NULL) {
      verboseOutput.flush();
      verboseOutput.setDevice(NULL);
      delete blackHole;
      blackHole = NULL;
    }
  }

  QList<QSharedPointer<CaptureDevice>> TimeLapseCapture::listDevices() {
    QList<QSharedPointer < CaptureDevice>> result;

    QList<V4LDevice> v4lDevices = V4LDevice::listDevices(&verboseOutput);
    for (V4LDevice v4lDev : v4lDevices) {
      result.push_back(QSharedPointer<CaptureDevice>(new V4LDevice(v4lDev)));
    }

    try {
      QList<Gphoto2Device> gp2devices = Gphoto2Device::listDevices(&verboseOutput, &err);
      for (Gphoto2Device gp2Dev : gp2devices) {
        result.push_back(QSharedPointer<Gphoto2Device>(new Gphoto2Device(gp2Dev)));
      }
    } catch (std::exception &e) {
      err << "Can't get Gphoto2 devices. " << e.what() << endl;
    }

    return result;
  }

  QSharedPointer<CaptureDevice> TimeLapseCapture::parseArguments() {
    QCommandLineParser parser;
    ErrorMessageHelper die(err.device(), &parser);

    parser.setApplicationDescription("Tool for capture sequence of images from digital camera (V4L or GPhoto2 API).");
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

    QCommandLineOption listOption(QStringList() << "l" << "list",
      QCoreApplication::translate("main", "List available capture devices and exits."));
    parser.addOption(listOption);

    QCommandLineOption intervalOption(QStringList() << "i" << "interval",
      QCoreApplication::translate("main", "Capture interval (in milliseconds). Default is 10000."),
      QCoreApplication::translate("main", "interval"));
    parser.addOption(intervalOption);

    QCommandLineOption cntOption(QStringList() << "c" << "count",
      QCoreApplication::translate("main", "How many images should be captured. Default value is infinite."),
      QCoreApplication::translate("main", "count"));
    parser.addOption(cntOption);

    QCommandLineOption rowOption(QStringList() << "r" << "raw",
      QCoreApplication::translate("main", "Store all captured images in raw."));
    parser.addOption(rowOption);

    QCommandLineOption getShutterSpeedOption(QStringList() << "s" << "get-shutterspeed",
      QCoreApplication::translate("main", "Prints available shutterspeed setting choices."));
    parser.addOption(getShutterSpeedOption);

    // Process the actual command line arguments given by the user
    parser.process(*this);

    // verbose?
    if (!parser.isSet(verboseOption)) {
      blackHole = new BlackHoleDevice();
      verboseOutput.setDevice(blackHole);
    } else {
      // verbose
      verboseOutput << "Turning on verbose output..." << endl;
      verboseOutput << applicationName() << " " << applicationVersion() << endl;
    }

    // raw?
    storeRawImages = parser.isSet(rowOption);

    // list devices?
    QList<QSharedPointer < CaptureDevice>> devices = listDevices();
    QSharedPointer<CaptureDevice> dev;
    if (parser.isSet(listOption)) {
      if (devices.isEmpty()) {
        die << QCoreApplication::translate("main", "No compatible capture device found");
      } else {
        out << "Found devices: " << endl;
        for (QSharedPointer<CaptureDevice> d : devices) {
          out << "  " << d->toString() << endl;
        }
      }
      std::exit(0);
    }
    // capture device
    bool assigned = false;
    if (!parser.isSet(deviceOption)) {
      verboseOutput << "Found devices: " << endl;
      for (QSharedPointer<CaptureDevice> d : devices) {
        if (!assigned) {
          dev = d;
          assigned = true;
        }
        verboseOutput << "  " << d->toString() << endl;
      }
      if (!assigned)
        die << "No supported device.";
    } else {
      QString devVal = parser.value(deviceOption);
      for (QSharedPointer<CaptureDevice> d : devices) {
        if (d->toString().contains(devVal, Qt::CaseInsensitive)) {
          assigned = true;
          dev = d;
        }
      }
      if (!assigned) {
        die << QString("No device matching \"%1\" found.").arg(devVal);
      }
    }
    out << "Using device " << dev->toString() << endl;

    // getShutterSpeedOption ?
    if (parser.isSet(getShutterSpeedOption)) {
      QList<ShutterSpeedChoice> choices = dev->getShutterSpeedChoices();
      if (choices.isEmpty()) {
        err << "Device " << dev->toShortString() << " don't support shutterspeed setting" << endl;
      } else {
        out << "Device " << dev->toShortString() << " shutterspeed choices:" << endl;
        for (ShutterSpeedChoice ch : choices) {
          out << "  " << ch.toString() << endl;
        }
      }
      std::exit(0);
    }

    // output
    if (!parser.isSet(outputOption))
      die << "Output directory is not set";
    output = QDir(parser.value(outputOption));
    if (output.exists())
      err << "Output directory exists already." << endl;
    if (!output.mkpath("."))
      die << QString("Can't create output directory %1 !").arg(output.path());

    if (parser.isSet(intervalOption)) {
      bool ok = false;
      long i = parser.value(intervalOption).toLong(&ok);
      if (!ok) die << "Cant parse interval.";
      if (i <= 0) die << "Interval have to be possitive";
      interval = i;
    }
    if (parser.isSet(cntOption)) {
      bool ok = false;
      int i = parser.value(cntOption).toInt(&ok);
      if (!ok) die << "Cant parse count.";
      cnt = i;
    }

    return dev;
  }

  void TimeLapseCapture::onError(QString msg) {
    emit cleanup(1);
  }

  void TimeLapseCapture::done() {
    // capturing devices can be asynchronous, we should wait a bit for possible events from devices
    QTimer::singleShot(1000, this, SLOT(cleanup()));
  }

  void TimeLapseCapture::cleanup(int exitCode) {
    exit(exitCode);
  }

  void TimeLapseCapture::run() {

    dev = parseArguments();

    connect(&timer, SIGNAL(timeout()), this, SLOT(capture()));
    connect(dev->qObject(), SIGNAL(imageCaptured(QString, Magick::Blob, Magick::Geometry)),
      this, SLOT(imageCaptured(QString, Magick::Blob, Magick::Geometry)));

    verboseOutput << "Start timer with interval " << interval << " ms" << endl;
    timer.start(interval);
    capture();
  }

  void TimeLapseCapture::capture() {
    if (cnt >= 0 && capturedCnt >= cnt) {
      timer.stop();
      done();
      return;
    }

    try {
      capturedSubsequence = 0;
      capturedCnt++;
      dev->capture();
    } catch (std::exception &e) {
      err << "Capturing failed: " << e.what() << endl;
      onError(e.what());
    }
  }

  QString TimeLapseCapture::leadingZeros(int i, int leadingZeros) {
    // default locale can include thousand delimiter
    QString s = frameNumberLocale.toString(i);
    if (leadingZeros <= s.length())
      return s;

    return s.prepend(QString(leadingZeros - s.length(), '0'));
  }

  void TimeLapseCapture::imageCaptured(QString format, Magick::Blob blob, Magick::Geometry sizeHint) {

    QString framePath = output.path() + QDir::separator()
      + leadingZeros(capturedCnt, FRAME_FILE_LEADING_ZEROS) + "_" + leadingZeros(capturedSubsequence, 2);

    if (format == "RGB") {
      if (storeRawImages) {
        // store RAW RGB data in PGM format
        QString pgmHeader = QString("P6\n%1 %2\n255\n").arg(sizeHint.width()).arg(sizeHint.height());
        std::string headerStr = pgmHeader.toStdString();
        const char *headerBytes = headerStr.c_str();
        size_t headerLen = strlen(headerBytes);

        framePath += ".ppm";
        QFile file(framePath);
        file.open(QIODevice::WriteOnly);
        file.write(headerBytes, headerLen);
        file.write((char*) blob.data(), blob.length());
        file.close();
      } else {
        // convert RGB data to JPEG
        Magick::Image capturedImage;
        capturedImage.read(blob, sizeHint, 8, "RGB");
        QDateTime now = QDateTime::currentDateTime();
        QString exifDateTime = now.toString("yyyy:MM:dd HH:mm:ss");\

        // ImageMagick don't support writing of exif data
        // TODO: setup exif timestamp correctly
        capturedImage.attribute("EXIF:DateTime", exifDateTime.toStdString());
        //capturedImage.defineValue("EXIF", "DateTime", exifDateTime.toStdString());

        capturedImage.compressType(Magick::JPEGCompression);
        capturedImage.magick("JPEG");
        framePath += ".jpeg";
        capturedImage.write(framePath.toStdString());

      }
    } else {
      // store other formats in device specific format 
      framePath += "." + format;
      QFile file(framePath);
      file.open(QIODevice::WriteOnly);
      file.write((char*) blob.data(), blob.length());
      file.close();
    }
    verboseOutput << "Captured frame saved to " << framePath << endl;

    capturedSubsequence++;
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

