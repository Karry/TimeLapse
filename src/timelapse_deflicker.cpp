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

#include "timelapse_deflicker.h"
#include "timelapse_deflicker.moc"

#include "timelapse.h"

using namespace std;
using namespace timelapse;

namespace timelapse {

  TimeLapseDeflicker::TimeLapseDeflicker(int &argc, char **argv) :
  QCoreApplication(argc, argv),
  out(stdout), err(stderr), dryRun(false), verboseOutput(stdout), blackHole(NULL),
  inputs(),
  pipeline(NULL) {

    setApplicationName("TimeLapse deflicker tool");
    setApplicationVersion(VERSION_TIMELAPSE);
  }

  TimeLapseDeflicker::~TimeLapseDeflicker() {
    if (blackHole != NULL) {
      verboseOutput.flush();
      verboseOutput.setDevice(NULL);
      delete blackHole;
      blackHole = NULL;
    }
  }

  QList<InputImageInfo> TimeLapseDeflicker::parseArguments() {
    QCommandLineParser parser;
    ErrorMessageHelper die(err.device(), &parser);

    parser.setApplicationDescription("Tool for assembly time-lapse video from sequence of images.");
    parser.addHelpOption();
    parser.addVersionOption();

    // Process the actual command line arguments given by the user
    parser.process(*this);

    QCommandLineOption dryRunOption(QStringList() << "d" << "dryrun",
      QCoreApplication::translate("main", "Just parse arguments, check inputs and prints informations."));
    parser.addOption(dryRunOption);

    QCommandLineOption verboseOption(QStringList() << "V" << "verbose",
      QCoreApplication::translate("main", "Verbose output."));
    parser.addOption(verboseOption);

    // verbose?
    if (!parser.isSet(verboseOption)) {
      blackHole = new BlackHoleDevice();
      verboseOutput.setDevice(blackHole);
      //_err << "no verbose"<< endl;
    } else {
      // verbose
      verboseOutput << "Turning on verbose output..." << endl;
      verboseOutput << applicationName() << " " << applicationVersion() << endl;
    }


    dryRun = parser.isSet(dryRunOption);

    return inputs;
  }

  void TimeLapseDeflicker::onError(QString msg) {
    emit cleanup(1);
  }

  void TimeLapseDeflicker::cleanup(int exitCode) {
    if (pipeline != NULL) {
      delete pipeline;
      pipeline = NULL;
    }

    exit(exitCode);
  }

  void TimeLapseDeflicker::run() {

    QList<InputImageInfo> inputs = parseArguments();

    // build processing pipeline
    pipeline = new Pipeline(inputs, &verboseOutput, &err);
    // startup pipeline
    emit pipeline->process();
  }
}

/*
 * Main method
 */
int main(int argc, char** argv) {

  TimeLapseDeflicker app(argc, argv);
  Magick::InitializeMagick(*argv);

  // This will run the task from the application event loop.
  QTimer::singleShot(0, &app, SLOT(run()));

  return app.exec();
}
