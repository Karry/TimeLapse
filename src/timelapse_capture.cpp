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
 * V4L implementation was inspired by http://linuxtv.org/downloads/v4l-dvb-apis/v4l2grab-example.html
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

#include <timelapse_capture.h>

#include <TimeLapse/timelapse_version.h>
#include <TimeLapse/timelapse.h>
#include <TimeLapse/black_hole_device.h>
#include <TimeLapse/pipeline_cpt.h>
#include <TimeLapse/pipeline_cpt_v4l.h>
#include <TimeLapse/pipeline_cpt_gphoto2.h>
#include <TimeLapse/pipeline_cpt_qcamera.h>

#include <Magick++.h>

#include <QtCore/QObject>
#include <QtCore/QDebug>
#include <QtCore/QTimer>
#include <QtCore/QTextStream>
#include <QtCore/QByteArray>
#include <QtCore/QCoreApplication>
#include <QtCore/QCommandLineParser>
#include <QtCore/QDir>

#include <exception>

using namespace std;
using namespace timelapse;

namespace timelapse {

  TimeLapseCaptureCli::TimeLapseCaptureCli(int &argc, char **argv) :
    QCoreApplication(argc, argv),
    out(stdout), err(stderr),
    verboseOutput(stdout), blackHole(nullptr),
    capture(&err, &verboseOutput)
  {
    setApplicationName("TimeLapse capture tool");
    setApplicationVersion(VERSION_TIMELAPSE);

    connect(&capture, &TimeLapseCapture::done, this, &TimeLapseCaptureCli::onDone);
    connect(&capture, &TimeLapseCapture::error, this, &TimeLapseCaptureCli::onError);
  }

  TimeLapseCaptureCli::~TimeLapseCaptureCli() {
    if (blackHole != nullptr) {
      verboseOutput.flush();
      verboseOutput.setDevice(nullptr);
      delete blackHole;
      blackHole = nullptr;
    }
  }

  QList<QSharedPointer<CaptureDevice>> TimeLapseCaptureCli::listDevices() {
    QList<QSharedPointer < CaptureDevice>> result;

    QList<V4LDevice> v4lDevices = V4LDevice::listDevices(&verboseOutput);
    for (const V4LDevice &v4lDev : v4lDevices) {
      result.push_back(QSharedPointer<CaptureDevice>(new V4LDevice(v4lDev)));
    }

    try {
      QList<Gphoto2Device> gp2devices = Gphoto2Device::listDevices(&verboseOutput, &err);
      for (const Gphoto2Device &gp2Dev : gp2devices) {
        result.push_back(QSharedPointer<Gphoto2Device>(new Gphoto2Device(gp2Dev)));
      }
    } catch (std::exception &e) {
      err << "Can't get Gphoto2 devices. " << QString::fromUtf8(e.what()) << endl;
    }

    QList<QCameraDevice> qCamDevices = QCameraDevice::listDevices(&verboseOutput);
    for (const QCameraDevice &qCamDev : qCamDevices) {
      result.push_back(QSharedPointer<QCameraDevice>(new QCameraDevice(qCamDev)));
    }

    return result;
  }

  ShutterSpeedChoice TimeLapseCaptureCli::getShutterSpeed(QString optStr, QList<ShutterSpeedChoice> choices, ErrorMessageHelper *die) {
    try {
      ShutterSpeedChoice requested(optStr);
      // check if this option is available
      bool found = false;
      for (ShutterSpeedChoice ch : choices) {
        if ((ch.toString() == requested.toString()) || (ch.isBulb() && requested.isBulb())) {
          found = true;
          break;
        }
      }
      if (!found)
        *die << QString("Camera doesn't support requested shutter speed \"%1\".").arg(optStr);
      return requested;
    } catch (std::exception &e) {
      *die << QString("Can't parse shutterspeed option \"%1\": %2").arg(optStr).arg(e.what());
    }

    throw std::logic_error("Unreachable statement - should not happen!");
  }

  QSharedPointer<CaptureDevice> TimeLapseCaptureCli::parseArguments() {
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

    QCommandLineOption rawOption(QStringList() << "r" << "raw",
                                 QCoreApplication::translate("main", "Store all captured images in raw."));
    parser.addOption(rawOption);

    QCommandLineOption getShutterSpeedOption(QStringList() << "s" << "shutter-speed-options",
                                             QCoreApplication::translate("main", "Prints available shutter speed setting choices and exits."));
    parser.addOption(getShutterSpeedOption);

    QCommandLineOption adaptiveShutterSpeedOption(QStringList() << "a" << "adaptive-shutter-speed",
      QCoreApplication::translate("main",
      "Camera shutter speed will be adaptively changed after exposure metering.\n"
      "This option setup how many images should be used for exposure metering. \n"
      "Default value is 0 - it means that shutter speed will not be changed by capture tool."
      ),
      QCoreApplication::translate("main", "count"));
    parser.addOption(adaptiveShutterSpeedOption);

    QCommandLineOption shutterSpeedStepOption(QStringList() << "shutter-speed-step",
      QCoreApplication::translate("main",
      "How large should be step when changing shutter speed. \n"
      "Default value is 1."
      ),
      QCoreApplication::translate("main", "step"));
    parser.addOption(shutterSpeedStepOption);

    QCommandLineOption minShutterSpeedOption(QStringList() << "min-shutter-speed",
      QCoreApplication::translate("main", "Minimum shutter speed (fastest shutter) used by adaptive shutter speed"),
      QCoreApplication::translate("main", "shutter speed"));
    parser.addOption(minShutterSpeedOption);

    QCommandLineOption maxShutterSpeedOption(QStringList() << "max-shutter-speed",
      QCoreApplication::translate("main",
      "Maximum shutter speed (slowest shutter) used by adaptive shutter speed.\n"
      "If camera supports BULB shutter speed, it can be defined as \"BULB:XX\" here (it means bulb with XX s exposure)."
      ),
      QCoreApplication::translate("main", "shutter speed"));
    parser.addOption(maxShutterSpeedOption);

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
    capture.setStoreRawImages(parser.isSet(rawOption));

    // interval
    if (parser.isSet(intervalOption)) {
      bool ok = false;
      long i = parser.value(intervalOption).toLong(&ok);
      if (!ok) die << "Cant parse interval.";
      if (i <= 0) die << "Interval have to be positive";
      capture.setInterval(i);
    }

    // count
    if (parser.isSet(cntOption)) {
      bool ok = false;
      int i = parser.value(cntOption).toInt(&ok);
      if (!ok) die << "Cant parse count.";
      capture.setCount(i);
    }

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
      if (!assigned) {
        die << "No supported device.";
      }
    } else {
      QString devVal = parser.value(deviceOption);
      for (QSharedPointer<CaptureDevice> d : devices) {
        if (d->toString().contains(devVal, Qt::CaseInsensitive)) {
          assigned = true;
          dev = d;
          break;
        }
      }
      if (!assigned) {
        die << QString("No device matching \"%1\" found.").arg(devVal);
      }
    }
    out << "Using device " << dev->toString() << endl;

    // getShutterSpeedOption ?
    QList<ShutterSpeedChoice> choices = dev->getShutterSpeedChoices();
    if (parser.isSet(getShutterSpeedOption)) {
      if (choices.isEmpty()) {
        err << "Device " << dev->toShortString() << " doesn't support shutter speed setting" << endl;
      } else {
        out << "Device " << dev->toShortString() << " shutter speed choices:" << endl;
        for (ShutterSpeedChoice ch : choices) {
          out << "  " << ch.toString() << endl;
        }
        out << "Current shutter speed: " << dev->currentShutterSpeed().toString() << endl;
      }
      std::exit(0);
    }

    // automatic shutter speed
    // this functionality is experimental!
    if (parser.isSet(adaptiveShutterSpeedOption)) {
      bool ok = false;
      int changeThreshold = parser.value(adaptiveShutterSpeedOption).toInt(&ok);
      if (!ok) {
        die << "Cant parse adaptive shutter speed option.";
      }

      if (changeThreshold > 0) {

        if (choices.isEmpty()) {
          die << "Camera doesn't support shutter speed setting.";
        } else {

          ShutterSpeedChoice currentShutterSpeed;
          ShutterSpeedChoice minShutterSpeed;
          ShutterSpeedChoice maxShutterSpeed;

          currentShutterSpeed = dev->currentShutterSpeed();
          minShutterSpeed = choices.first();
          for (ShutterSpeedChoice ch : choices) {
            if ((!ch.isBulb()) && ch.toMicrosecond() > maxShutterSpeed.toMicrosecond())
              maxShutterSpeed = ch;
          }

          int shutterSpeedStep = 1;
          if (parser.isSet(shutterSpeedStepOption)) {
            bool ok = false;
            shutterSpeedStep = parser.value(shutterSpeedStepOption).toInt(&ok);
            if (!ok) die << "Cant parse shutter speed step.";
            if (shutterSpeedStep < 1) {
              die << "Shutter speed step can be less than one.";
            }
          }
          if (parser.isSet(minShutterSpeedOption)) {
            QString optStr = parser.value(minShutterSpeedOption);
            minShutterSpeed = getShutterSpeed(optStr, choices, &die);
          }
          if (parser.isSet(maxShutterSpeedOption)) {
            QString optStr = parser.value(maxShutterSpeedOption);
            maxShutterSpeed = getShutterSpeed(optStr, choices, &die);
          }

          out << "Using automatic shutter speed:" << endl;
          out << "  current shutter speed: " << currentShutterSpeed.toString() << endl;
          out << "  min. shutter speed:    " << minShutterSpeed.toString() << endl;
          out << "  max. shutter speed:    " << maxShutterSpeed.toString() << endl;
          out << "  change threshold:      " << changeThreshold << endl;
          out << "  change step:           " << shutterSpeedStep << endl;

          if (minShutterSpeed.toMicrosecond() <= 0
            || maxShutterSpeed.toMicrosecond() <= 0
            || maxShutterSpeed.toMicrosecond() < minShutterSpeed.toMicrosecond())
            die << "Invalid shutter speed configuration";

          if (maxShutterSpeed.toMs() > capture.getInterval()) {
            err << QString("Warning: Maximum shutter speed (%1 ms) is greater than capture interval (%2 ms)!")
              .arg(maxShutterSpeed.toMs())
              .arg(capture.getInterval()) << endl;
          }
          capture.setMinShutterSpeed(minShutterSpeed);
          capture.setMaxShutterSpeed(maxShutterSpeed);
          capture.setShutterSpeedChangeThreshold(changeThreshold);
          capture.setShutterSpeedStep(shutterSpeedStep);
        }
      }
    }

    // output
    if (!parser.isSet(outputOption)) {
      die << "Output directory is not set";
    }
    QDir output = QDir(parser.value(outputOption));
    if (output.exists()) {
      err << "Output directory exists already." << endl;
    }
    if (!output.mkpath(".")) {
      die << QString("Can't create output directory %1 !").arg(output.path());
    }

    capture.setOutput(output);
    capture.setDevice(dev);
    return dev;
  }

  void TimeLapseCaptureCli::onDone() {
    emit cleanup(0);
  }

  void TimeLapseCaptureCli::onError([[maybe_unused]] const QString &msg) {
    emit cleanup(1);
  }

  void TimeLapseCaptureCli::cleanup(int exitCode) {
    exit(exitCode);
  }

  void TimeLapseCaptureCli::run() {
    parseArguments();
    capture.start();
  }
}

/*
 * Main method
 */
int main(int argc, char** argv) {

  timelapse::registerQtMetaTypes();

  TimeLapseCaptureCli app(argc, argv);
  Magick::InitializeMagick(*argv);

  // This will run the task from the application event loop.
  QTimer::singleShot(0, &app, SLOT(run()));

  int exitCode = app.exec();
  Magick::TerminateMagick();
  return exitCode;
}

