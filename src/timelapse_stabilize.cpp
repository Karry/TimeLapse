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

#include "timelapse_stabilize.h"
#include "timelapse_stabilize.moc"

#include "timelapse.h"
#include "black_hole_device.h"
#include "pipeline_stab.h"
#include "pipeline_write_frame.h"
#include "pipeline_handler.h"
#include "pipeline_frame_mapping.h"

#include <QtCore/QObject>
#include <QtCore/QTimer>
#include <QtCore/QTextStream>
#include <QtCore/QByteArray>
#include <QtCore/QCoreApplication>
#include <QtCore/QCommandLineParser>
#include <QtCore/QDir>

using namespace std;
using namespace timelapse;

namespace timelapse {

  TimeLapseStabilize::TimeLapseStabilize(int &argc, char **argv) :
  QCoreApplication(argc, argv),
  out(stdout), err(stderr),
  verboseOutput(stdout), blackHole(nullptr),
  pipeline(nullptr), stabConf(nullptr), output(),
  dryRun(false),
  tempDir(nullptr) {

    setApplicationName("TimeLapse stabilize tool");
    setApplicationVersion(VERSION_TIMELAPSE);
  }

  TimeLapseStabilize::~TimeLapseStabilize() {
    if (blackHole != nullptr) {
      verboseOutput.flush();
      verboseOutput.setDevice(nullptr);
      delete blackHole;
      blackHole = nullptr;
    }

    if (tempDir != nullptr) {
      delete tempDir;
      tempDir = nullptr;
    }
  }

  QStringList TimeLapseStabilize::parseArguments() {
    QCommandLineParser parser;
    ErrorMessageHelper die(err.device(), &parser);

    parser.setApplicationDescription("Tool for stabilize movements in sequence of images.");
    parser.addHelpOption();
    parser.addVersionOption();

    parser.addPositionalArgument("source(s)",
      QCoreApplication::translate("main", "Source images (images or directories with images)."));

    QCommandLineOption outputOption(QStringList() << "o" << "output",
      QCoreApplication::translate("main", "Output directory."),
      QCoreApplication::translate("main", "directory"));
    parser.addOption(outputOption);

    QCommandLineOption dryRunOption(QStringList() << "d" << "dryrun",
      QCoreApplication::translate("main", "Just parse arguments, check inputs and prints information."));
    parser.addOption(dryRunOption);

    QCommandLineOption verboseOption(QStringList() << "V" << "verbose",
      QCoreApplication::translate("main", "Verbose output."));
    parser.addOption(verboseOption);

    if (stabConf != nullptr) {
      delete stabConf;
    }
    stabConf = new StabConfig();

    stabConf->addOptions(parser);

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

    dryRun = parser.isSet(dryRunOption);

    stabConf->processOptions(parser, die, &err);

    // inputs
    QStringList inputArgs = parser.positionalArguments();
    if (inputArgs.empty()) {
      die << "No input given";
    }

    // output
    if (!parser.isSet(outputOption)) {
      die << "Output directory is not set";
    }
    output = QDir(parser.value(outputOption));
    if (output.exists()) {
      err << "Output directory already exists." << endl;
    }
    if (!output.mkpath(".")) {
      die << QString("Can't create output directory %1 !").arg(output.path());
    }

    if (stabConf->mdConf.show > 0) {
      QString tmpBaseDir = QDir::tempPath();
      tempDir = new QTemporaryDir(tmpBaseDir + QDir::separator() + "timelapse_");
      if (!tempDir->isValid()) {
        die << "Can't create temp directory";
      }
    }
    return inputArgs;
  }

  void TimeLapseStabilize::onError([[maybe_unused]] const QString &msg) {
    emit cleanup2(1);
  }

  void TimeLapseStabilize::cleanup() {
    cleanup2(0);
  }

  void TimeLapseStabilize::cleanup2(int exitCode) {
    if (pipeline != nullptr) {
      delete pipeline;
      pipeline = nullptr;
    }
    if (stabConf != nullptr) {
      delete stabConf;
      stabConf = nullptr;
    }

    exit(exitCode);
  }

  void TimeLapseStabilize::run() {

    QStringList inputArgs = parseArguments();

    // build processing pipeline
    stabInit(&verboseOutput, &err);


    pipeline = Pipeline::createWithFileSource(inputArgs, false, &verboseOutput, &err);
    *pipeline << new OneToOneFrameMapping();
    *pipeline << new PipelineStabDetect(stabConf, &verboseOutput, &err);
    if (stabConf->mdConf.show > 0) {
      *pipeline << new WriteFrame(QDir(tempDir->path()), &verboseOutput, dryRun);
    }
    
    *pipeline << new StageSeparator();

    *pipeline << new PipelineStabTransform(stabConf, &verboseOutput, &err);
    *pipeline << new WriteFrame(output, &verboseOutput, dryRun);

    connect(pipeline, &Pipeline::done, this, &TimeLapseStabilize::cleanup);
    connect(pipeline, &Pipeline::error, this, &TimeLapseStabilize::onError);

    // startup pipeline
    emit pipeline->process();
  }
}

/*
 * Main method
 */
int main(int argc, char** argv) {

  timelapse::registerQtMetaTypes();

  TimeLapseStabilize app(argc, argv);
  Magick::InitializeMagick(*argv);

  // This will run the task from the application event loop.
  QTimer::singleShot(0, &app, SLOT(run()));

  return app.exec();
}

