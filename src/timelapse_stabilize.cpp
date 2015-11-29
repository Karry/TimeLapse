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

#include <QtCore/QObject>
#include <QtCore/QDebug>
#include <QtCore/QTimer>
#include <QtCore/QTextStream>
#include <QtCore/QByteArray>
#include <QtCore/QCoreApplication>
#include <QtCore/QCommandLineParser>
#include <QtCore/QProcess>

#include <QtCore/QDir>

#include "timelapse_stabilize.h"
#include "timelapse_stabilize.moc"

#include "timelapse.h"
#include "black_hole_device.h"


using namespace std;
using namespace timelapse;

namespace timelapse {

  TimeLapseStabilize::TimeLapseStabilize(int &argc, char **argv) :
  QCoreApplication(argc, argv),
  out(stdout), err(stderr),
  verboseOutput(stdout), blackHole(NULL),
  pipeline(NULL), output(),
  dryRun(false) {

    setApplicationName("TimeLapse stabilize tool");
    setApplicationVersion(VERSION_TIMELAPSE);
  }

  TimeLapseStabilize::~TimeLapseStabilize() {
    if (blackHole != NULL) {
      verboseOutput.flush();
      verboseOutput.setDevice(NULL);
      delete blackHole;
      blackHole = NULL;
    }
  }

  QStringList TimeLapseStabilize::parseArguments() {
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

    QCommandLineOption dryRunOption(QStringList() << "d" << "dryrun",
      QCoreApplication::translate("main", "Just parse arguments, check inputs and prints informations."));
    parser.addOption(dryRunOption);

    QCommandLineOption verboseOption(QStringList() << "V" << "verbose",
      QCoreApplication::translate("main", "Verbose output."));
    parser.addOption(verboseOption);

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
    if (!output.mkpath(output.path()))
      die << QString("Can't create output directory %1 !").arg(output.path());

    return inputArgs;
  }

  void TimeLapseStabilize::onError(QString msg) {
    emit cleanup(1);
  }

  void TimeLapseStabilize::cleanup(int exitCode) {
    if (pipeline != NULL) {
      delete pipeline;
      pipeline = NULL;
    }

    exit(exitCode);
  }

  void TimeLapseStabilize::run() {

    QStringList inputArgs = parseArguments();

    exit(0);
  }
}

/*
 * Main method
 */
int main(int argc, char** argv) {
  TimeLapseStabilize app(argc, argv);
  Magick::InitializeMagick(*argv);

  // This will run the task from the application event loop.
  QTimer::singleShot(0, &app, SLOT(run()));

  return app.exec();
}

