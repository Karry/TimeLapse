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

#include <timelapse_deflicker.h>

#include <TimeLapse/timelapse.h>
#include <TimeLapse/timelapse_version.h>
#include <TimeLapse/error_message_helper.h>

#include <TimeLapse/pipeline_handler.h>
#include <TimeLapse/pipeline_frame_mapping.h>
#include <TimeLapse/pipeline_frame_prepare.h>
#include <TimeLapse/pipeline_video_assembly.h>
#include <TimeLapse/pipeline_write_frame.h>
#include <TimeLapse/pipeline_resize_frame.h>
#include <TimeLapse/pipeline_deflicker.h>

#include <Magick++.h>
#include <Magick++/Color.h>

#include <QtCore/QObject>
#include <QtCore/QTimer>
#include <QtCore/QTextStream>
#include <QtCore/QByteArray>
#include <QtCore/QCoreApplication>
#include <QtCore/QCommandLineParser>
#include <QtCore/QDir>

#include <vector>

using namespace std;
using namespace timelapse;

namespace timelapse {

  TimeLapseDeflicker::TimeLapseDeflicker(int &argc, char **argv) :
  QCoreApplication(argc, argv),
  out(stdout), err(stderr),
  dryRun(false), debugView(false),
  wmaCount(-1),
  verboseOutput(stdout), blackHole(nullptr),
  pipeline(nullptr), output() {

    setApplicationName("TimeLapse deflicker tool");
    setApplicationVersion(VERSION_TIMELAPSE);
  }

  TimeLapseDeflicker::~TimeLapseDeflicker() {
    if (blackHole != nullptr) {
      verboseOutput.flush();
      verboseOutput.setDevice(nullptr);
      delete blackHole;
      blackHole = nullptr;
    }
  }

  QStringList TimeLapseDeflicker::parseArguments() {
    QCommandLineParser parser;
    ErrorMessageHelper die(err.device(), &parser);

    parser.setApplicationDescription("Tool for deflicker sequence of images.");
    parser.addHelpOption();
    parser.addVersionOption();

    parser.addPositionalArgument("source(s)",
      QCoreApplication::translate("main", "Source images (images or directories with images)."));

    QCommandLineOption outputOption(QStringList() << "o" << "output",
      QCoreApplication::translate("main", "Output directory."),
      QCoreApplication::translate("main", "directory"));
    parser.addOption(outputOption);

    QCommandLineOption wmaCountOption(QStringList() << "wm-average",
      QCoreApplication::translate("main",
      "Use weighted moving average for luminance.\n"
      "Argument specified count of previous images for compute average."
      ),
      QCoreApplication::translate("main", "count"));
    parser.addOption(wmaCountOption);

    QCommandLineOption dryRunOption(QStringList() << "d" << "dryrun",
      QCoreApplication::translate("main", "Just parse arguments, check inputs and prints informations."));
    parser.addOption(dryRunOption);

    QCommandLineOption debugViewOption(QStringList() << "w" << "debug-view",
      QCoreApplication::translate("main",
      "Composite one half of output image from original and second half from updated image."
      ));
    parser.addOption(debugViewOption);

    QCommandLineOption verboseOption(QStringList() << "V" << "verbose",
      QCoreApplication::translate("main", "Verbose output."));
    parser.addOption(verboseOption);

    // Process the actual command line arguments given by the user
    parser.process(*this);

    // wma?
    if (parser.isSet(wmaCountOption)) {
      bool ok = false;
      int i = parser.value(wmaCountOption).toInt(&ok);
      if (!ok) die << "Cant parse wma count.";
      if (i < 0) die << "Wma count have to be possitive";
      wmaCount = (size_t) i;
    }

    // verbose?
    if (!parser.isSet(verboseOption)) {
      blackHole = new BlackHoleDevice();
      verboseOutput.setDevice(blackHole);
    } else {
      // verbose
      verboseOutput << "Turning on verbose output..." << endl;
      verboseOutput << applicationName() << " " << applicationVersion() << endl;
    }

    debugView = parser.isSet(debugViewOption);
    dryRun = parser.isSet(dryRunOption);

    // inputs
    QStringList inputArgs = parser.positionalArguments();
    if (inputArgs.empty())
      die << "No input given";

    // output
    if (!parser.isSet(outputOption))
      die << "Output directory is not set";
    output = QDir(parser.value(outputOption));
    if (output.exists())
      err << "Output directory exists already." << endl;
    if (!output.mkpath("."))
      die << QString("Can't create output directory %1 !").arg(output.path());

    return inputArgs;
  }

  void TimeLapseDeflicker::onError([[maybe_unused]] const QString &msg) {
    emit cleanup2(1);
  }

  void TimeLapseDeflicker::cleanup() {
    cleanup2(0);
  }

  void TimeLapseDeflicker::cleanup2(int exitCode) {
    if (pipeline != nullptr) {
      pipeline->deleteLater();
      pipeline = nullptr;
    }

    exit(exitCode);
  }

  void TimeLapseDeflicker::run() {

    QStringList inputArgs = parseArguments();

    // build processing pipeline
    pipeline = Pipeline::createWithFileSource(inputArgs, QStringList(), false, &verboseOutput, &err);

    *pipeline << new ComputeLuminance(&verboseOutput);
    *pipeline << new OneToOneFrameMapping();
    if (wmaCount > 0)
      *pipeline << new WMALuminance(&verboseOutput, wmaCount);
    else
      *pipeline << new ComputeAverageLuminance(&verboseOutput);
    *pipeline << new AdjustLuminance(&verboseOutput, debugView);
    //*pipeline << new ComputeLuminance(&verboseOutput);
    *pipeline << new WriteFrame(output, &verboseOutput, dryRun);

    connect(pipeline, &Pipeline::done, this, &TimeLapseDeflicker::cleanup);
    connect(pipeline, &Pipeline::error, this, &TimeLapseDeflicker::onError);

    // startup pipeline
    emit pipeline->process();
  }
}

/*
 * Main method
 */
int main(int argc, char** argv) {

  timelapse::registerQtMetaTypes();

  TimeLapseDeflicker app(argc, argv);
  Magick::InitializeMagick(*argv);

  // This will run the task from the application event loop.
  QTimer::singleShot(0, &app, SLOT(run()));

  int exitCode = app.exec();
  Magick::TerminateMagick();
  return exitCode;
}

