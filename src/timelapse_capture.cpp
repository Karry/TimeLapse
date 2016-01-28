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

#include <exception>
#include <queue>
#include <vector>
#include <utility>
#include <Magick++.h>
#include <ImageMagick-6/Magick++/Color.h>
#include <ImageMagick-6/magick/exception.h>

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

  AdaptiveShutterSpeedAlg::AdaptiveShutterSpeedAlg(
    QList<ShutterSpeedChoice> shutterSpeedChoices,
    ShutterSpeedChoice currentShutterSpeed,
    ShutterSpeedChoice minShutterSpeed,
    ShutterSpeedChoice maxShutterSpeed,
    QTextStream *err,
    QTextStream *verboseOutput) :

  shutterSpeedChoices(shutterSpeedChoices),
  currentShutterSpeed(currentShutterSpeed),
  minShutterSpeed(minShutterSpeed),
  maxShutterSpeed(maxShutterSpeed),
  err(err), verboseOutput(verboseOutput) {
  }

  AdaptiveShutterSpeedAlg::~AdaptiveShutterSpeedAlg() {
  }

  MatrixMeteringAlg::MatrixMeteringAlg(
    QList<ShutterSpeedChoice> shutterSpeedChoices,
    ShutterSpeedChoice currentShutterSpeed,
    ShutterSpeedChoice minShutterSpeed,
    ShutterSpeedChoice maxShutterSpeed,
    QTextStream *err,
    QTextStream *verboseOutput,
    int changeThreshold,
    int step) :

  AdaptiveShutterSpeedAlg(shutterSpeedChoices, currentShutterSpeed,
  minShutterSpeed, maxShutterSpeed, err, verboseOutput),
  changeThreshold(changeThreshold), greyHistograms(),
  step(step) {
  }

  void MatrixMeteringAlg::clearHistograms() {
    for (uint32_t *hist : greyHistograms) {
      delete[] hist;
    }
    greyHistograms.clear();
  }

  MatrixMeteringAlg::~MatrixMeteringAlg() {
    clearHistograms();
  }

#define GREY_HISTOGRAM_RESOLUTION 256
#define UNDEREXPOSURE_GREY_LIMIT 13
#define OVEREXPOSURE_GREY_LIMIT 242
#define UNDEREXPOSURE_RATIO_LIMIT .05
#define OVEREXPOSURE_RATIO_LIMIT .05
#define BULB_CHANGE_s 5

  void MatrixMeteringAlg::update(Magick::Image img) {
    // remove old histograms, keep last (changeThreshold) histograms
    while (greyHistograms.size() >= changeThreshold) {
      QList<uint32_t *>::Iterator it = greyHistograms.begin();
      delete[] * it;
      greyHistograms.removeFirst();
    }

    uint32_t *greyHistogram = new uint32_t[GREY_HISTOGRAM_RESOLUTION]();

    // get img histogram
    std::vector<std::pair < Magick::Color, size_t>> histogram;
    Magick::colorHistogram(&histogram, img);

    // compute grey-scale histogram
    for (std::pair<Magick::Color, size_t> p : histogram) {
      double r = Magick::Color::scaleQuantumToDouble(p.first.redQuantum());
      double g = Magick::Color::scaleQuantumToDouble(p.first.greenQuantum());
      double b = Magick::Color::scaleQuantumToDouble(p.first.blueQuantum());
      double grey = 0.299 * r + 0.587 * g + 0.114 * b;
      int i = std::max(
        std::min(GREY_HISTOGRAM_RESOLUTION, (int) (grey * ((double) GREY_HISTOGRAM_RESOLUTION))),
        0);
      greyHistogram[i] += p.second;
    }

    greyHistograms.append(greyHistogram);
  }

  ShutterSpeedChoice MatrixMeteringAlg::adjustShutterSpeed() {
    if (greyHistograms.size() < changeThreshold) {
      return currentShutterSpeed;
    }

    // iterate over last grey histograms and compute how many images was 
    // under exposured (possitive score) and how many over exposured (negative score)
    // then, change shutter speed if abs(score) > (changeThreshold / 3)
    int changeScore = 0;

    for (uint32_t *histo : greyHistograms) {
      uint32_t pixCnt = 0;
      uint32_t underExposureCnt = 0;
      uint32_t overExposureCnt = 0;
      for (int ii = 0; ii < GREY_HISTOGRAM_RESOLUTION; ii++) {
        pixCnt += histo[ii];
        if (ii < UNDEREXPOSURE_GREY_LIMIT)
          underExposureCnt += histo[ii];
        if (ii > OVEREXPOSURE_GREY_LIMIT)
          overExposureCnt += histo[ii];
      }

      double underExposure = (double) underExposureCnt / (double) pixCnt;
      double overExposure = (double) overExposureCnt / (double) pixCnt;
      if (underExposure > UNDEREXPOSURE_RATIO_LIMIT || overExposure > OVEREXPOSURE_RATIO_LIMIT) {
        if (underExposure > overExposure)
          changeScore += 1;
        else
          changeScore -= 1;
      }
    }

    if (std::abs(changeScore) <= changeThreshold / 3) { // ignore low score
      return currentShutterSpeed;
    }
    if ((changeScore > 0 && currentShutterSpeed.toMicrosecond() >= maxShutterSpeed.toMicrosecond())
      || (changeScore < 0 && currentShutterSpeed.toMicrosecond() <= minShutterSpeed.toMicrosecond())) {
      return currentShutterSpeed;
    }

    // adjust ShutterSpeed
    ShutterSpeedChoice prev = currentShutterSpeed;
    if (currentShutterSpeed.isBulb()) {
      if (changeScore > 0) {
        currentShutterSpeed = ShutterSpeedChoice(
          true,
          std::min(currentShutterSpeed.toSecond() + (BULB_CHANGE_s * step), maxShutterSpeed.toSecond()),
          1);
      } else {
        currentShutterSpeed = ShutterSpeedChoice(
          true,
          std::min(currentShutterSpeed.toSecond() - (BULB_CHANGE_s * step), maxShutterSpeed.toSecond()),
          1);

        // if current bulb time is less than some regular speed, use it
        for (ShutterSpeedChoice ch : shutterSpeedChoices) {
          if ((!ch.isBulb()) && ch.toMicrosecond() >= currentShutterSpeed.toMicrosecond()) {
            currentShutterSpeed = ch;
            break;
          }
        }
      }
    } else {
      int currentStep = 0;
      if (changeScore > 0) {
        for (ShutterSpeedChoice ch : shutterSpeedChoices) {
          if ((!ch.isBulb()) && ch.toMicrosecond() > currentShutterSpeed.toMicrosecond()) {
            currentShutterSpeed = ch;
            currentStep += 1;
            if (currentShutterSpeed.toMicrosecond() >= maxShutterSpeed.toMicrosecond())
              break;
            if (currentStep >= step)
              break;
          }
        }
        if (prev.toMicrosecond() == currentShutterSpeed.toMicrosecond() && maxShutterSpeed.isBulb()) {
          currentShutterSpeed = ShutterSpeedChoice(
            true,
            std::min(currentShutterSpeed.toSecond() + (BULB_CHANGE_s * step), maxShutterSpeed.toSecond()),
            1);
        }
      } else {
        QList<ShutterSpeedChoice> reverse = shutterSpeedChoices;
        std::reverse(reverse.begin(), reverse.end());
        for (ShutterSpeedChoice ch : reverse) {
          if ((!ch.isBulb()) && ch.toMicrosecond() < currentShutterSpeed.toMicrosecond()) {
            currentShutterSpeed = ch;
            currentStep += 1;
            if (currentShutterSpeed.toMicrosecond() <= minShutterSpeed.toMicrosecond())
              break;
            if (currentStep >= step)
              break;
          }
        }
      }
    }

    // print verbose informations
    *verboseOutput << "Some of previous frames was "
      << (changeScore > 0 ? "underexposured" : "overexposured")
      << " (score " << changeScore << "). Changing shutter speed from "
      << prev.toString() << " to " << currentShutterSpeed.toString()
      << endl;

    clearHistograms();

    return currentShutterSpeed;
  }

  TimeLapseCapture::TimeLapseCapture(int &argc, char **argv) :
  QCoreApplication(argc, argv),
  out(stdout), err(stderr),
  verboseOutput(stdout), blackHole(NULL),
  output(), timer(), frameNumberLocale(QLocale::c()), capturedCnt(0), capturedSubsequence(0),
  shutterSpdAlg(NULL),
  interval(10000), cnt(-1) {

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
    if (shutterSpdAlg != NULL) {
      delete shutterSpdAlg;
      shutterSpdAlg = NULL;
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

  ShutterSpeedChoice TimeLapseCapture::getShutterSpeed(QString optStr, QList<ShutterSpeedChoice> choices, ErrorMessageHelper *die) {
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
        *die << QString("Camera don't support requested shutterspeed \"%1\".").arg(optStr);
      return requested;
    } catch (std::exception &e) {
      *die << QString("Can't parse shutterspeed option \"%1\": %2").arg(optStr).arg(e.what());
    }

    throw std::logic_error("Unreachable statement - should not happen!");
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

    QCommandLineOption getShutterSpeedOption(QStringList() << "s" << "shutterspeed-options",
      QCoreApplication::translate("main", "Prints available shutterspeed setting choices and exits."));
    parser.addOption(getShutterSpeedOption);

    QCommandLineOption adaptiveShutterSpeedOption(QStringList() << "a" << "adaptive-shutterspeed",
      QCoreApplication::translate("main",
      "Camera shutterspeed will be adaptively changed after exposure metering.\n"
      "This option setup how many images should be used for exposure metering. \n"
      "Default value is 0 - it means that shutterspeed will not be changed by capture tool."
      ),
      QCoreApplication::translate("main", "count"));
    parser.addOption(adaptiveShutterSpeedOption);

    QCommandLineOption shutterSpeedStepOption(QStringList() << "shutterspeed-step",
      QCoreApplication::translate("main",
      "How large should be step when changing shutterspeed. \n"
      "Default value is 1."
      ),
      QCoreApplication::translate("main", "step"));
    parser.addOption(shutterSpeedStepOption);

    QCommandLineOption minShutterSpeedOption(QStringList() << "min-shutterspeed",
      QCoreApplication::translate("main", "Minimum shutterspeed (fastest shutter) used by adaptive shutterspeed"),
      QCoreApplication::translate("main", "shutterspeed"));
    parser.addOption(minShutterSpeedOption);

    QCommandLineOption maxShutterSpeedOption(QStringList() << "max-shutterspeed",
      QCoreApplication::translate("main",
      "Maximum shutterspeed (slowest shutter) used by adaptive shutterspeed.\n"
      "If camera supports BULB shutterspeed, it can be defined as \"BULB:XX\" here (it means bulb with XX s exposure)."
      ),
      QCoreApplication::translate("main", "shutterspeed"));
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
    storeRawImages = parser.isSet(rowOption);

    // interval
    if (parser.isSet(intervalOption)) {
      bool ok = false;
      long i = parser.value(intervalOption).toLong(&ok);
      if (!ok) die << "Cant parse interval.";
      if (i <= 0) die << "Interval have to be possitive";
      interval = i;
    }

    // count
    if (parser.isSet(cntOption)) {
      bool ok = false;
      int i = parser.value(cntOption).toInt(&ok);
      if (!ok) die << "Cant parse count.";
      cnt = i;
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
      if (!assigned)
        die << "No supported device.";
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
        err << "Device " << dev->toShortString() << " don't support shutterspeed setting" << endl;
      } else {
        out << "Device " << dev->toShortString() << " shutterspeed choices:" << endl;
        for (ShutterSpeedChoice ch : choices) {
          out << "  " << ch.toString() << endl;
        }
        out << "Current shutter speed: " << dev->currentShutterSpeed().toString() << endl;
      }
      std::exit(0);
    }

    // automatic chutter speed
    // this functionality is experimental!
    if (parser.isSet(adaptiveShutterSpeedOption)) {
      bool ok = false;
      int changeThreshold = parser.value(adaptiveShutterSpeedOption).toInt(&ok);
      if (!ok) die << "Cant parse adaptive shutterspeed option.";

      if (changeThreshold > 0) {

        if (choices.isEmpty()) {
          die << "Camera don't support shutterspeed setting.";
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
            if (!ok) die << "Cant parse shutterspeed step.";
            if (shutterSpeedStep < 1) die << "Shutterspeed step can be less than one.";
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
          out << "  min shutter speed:     " << minShutterSpeed.toString() << endl;
          out << "  max shutter speed:     " << maxShutterSpeed.toString() << endl;
          out << "  change threshold:      " << changeThreshold << endl;
          out << "  change step:           " << shutterSpeedStep << endl;

          if (minShutterSpeed.toMicrosecond() <= 0
            || maxShutterSpeed.toMicrosecond() <= 0
            || maxShutterSpeed.toMicrosecond() < minShutterSpeed.toMicrosecond())
            die << "Invalid shutter speed configurarion";

          if (maxShutterSpeed.toMs() > interval) {
            err << QString("Warning: Maximum shutter speed (%1 ms) is greater than capture interval (%2 ms)!")
              .arg(maxShutterSpeed.toMs())
              .arg(interval) << endl;
          }
          shutterSpdAlg = new MatrixMeteringAlg(choices, currentShutterSpeed, minShutterSpeed, maxShutterSpeed,
            &err, &verboseOutput, changeThreshold, shutterSpeedStep);
        }
      }
    }

    // output
    if (!parser.isSet(outputOption))
      die << "Output directory is not set";
    output = QDir(parser.value(outputOption));
    if (output.exists())
      err << "Output directory exists already." << endl;
    if (!output.mkpath("."))
      die << QString("Can't create output directory %1 !").arg(output.path());

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
    //timer.start(interval);
    capture();
  }

#define BUSY_CAPTURE_POSTPONE_MS 100

  void TimeLapseCapture::capture() {
    if (cnt >= 0 && capturedCnt >= cnt) {
      timer.stop();
      done();
      return;
    }

    if (dev->isBusy()) {
      verboseOutput << "Camera is busy, postpone capturing by " << BUSY_CAPTURE_POSTPONE_MS << " ms" << endl;
      timer.stop();
      QTimer::singleShot(BUSY_CAPTURE_POSTPONE_MS, this, SLOT(capture()));
      return;
    }
    if (!timer.isActive()) {
      timer.start(interval);
    }

    try {
      capturedSubsequence = 0;
      capturedCnt++;
      ShutterSpeedChoice shutterSpeed;
      if (shutterSpdAlg != NULL)
        shutterSpeed = shutterSpdAlg->adjustShutterSpeed();
      dev->capture(shutterSpeed);
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
    bool readRawFromFile = false;

    QString framePath = output.path() + QDir::separator()
      + leadingZeros(capturedCnt, FRAME_FILE_LEADING_ZEROS) + "_" + leadingZeros(capturedSubsequence, 2);

    if (format == "RGB") {
      if (storeRawImages) {
        // store RAW RGB data in PPM format
        QString pgmHeader = QString("P6\n%1 %2\n255\n").arg(sizeHint.width()).arg(sizeHint.height());
        std::string headerStr = pgmHeader.toStdString();
        const char *headerBytes = headerStr.c_str();
        size_t headerLen = strlen(headerBytes);

        if (shutterSpdAlg != NULL && capturedSubsequence == 0) {
          Magick::Image capturedImage;
          capturedImage.read(blob, sizeHint, 8, "RGB");
          shutterSpdAlg->update(capturedImage);
        }

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

        if (shutterSpdAlg != NULL && capturedSubsequence == 0) {
          shutterSpdAlg->update(capturedImage);
        }

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

      if (shutterSpdAlg != NULL && capturedSubsequence == 0) {
        try {
          Magick::Image capturedImage;
          capturedImage.read(blob, format.toStdString());
          shutterSpdAlg->update(capturedImage);
        } catch (const std::exception &e) {
          err << "Failed to decode captured image (" << format << "): " << e.what() << endl;
          readRawFromFile = true;
        }
      }

      // store other formats in device specific format 
      framePath += "." + format;
      QFile file(framePath);
      file.open(QIODevice::WriteOnly);
      file.write((char*) blob.data(), blob.length());
      file.close();

      if (readRawFromFile && shutterSpdAlg != NULL && capturedSubsequence == 0) {
        /* I don't understand ImageMagick correctly, but it fails with reading RAW files
         * from memory blob, but reading from file works (sometimes). 
         * Maybe, it don't support delegating (dcraw, ufraw...) with memory data...
         */
        try {
          Magick::Image capturedImage;
          capturedImage.read(framePath.toStdString());
          shutterSpdAlg->update(capturedImage);
        } catch (const std::exception &e) {
          err << "Failed to decode captured image (" << framePath << "): " << e.what() << endl;
        }
      }
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

