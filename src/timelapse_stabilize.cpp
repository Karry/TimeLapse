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
    ErrorMessageHelper die(err.device());

    //QStringList inputArgs = parseArguments();
    parseArguments();
    QStringList inputArgs;
    inputArgs << "/media/data/tmp/timelapse/DSC_3803.JPG";
    inputArgs << "/media/data/tmp/timelapse/DSC_3804.JPG";
    inputArgs << "/media/data/tmp/timelapse/DSC_3805.JPG";
    inputArgs << "/media/data/tmp/timelapse/DSC_3806.JPG";
    inputArgs << "/media/data/tmp/timelapse/DSC_3807.JPG";
    inputArgs << "/media/data/tmp/timelapse/DSC_3808.JPG";
    inputArgs << "/media/data/tmp/timelapse/DSC_3809.JPG";
    inputArgs << "/media/data/tmp/timelapse/DSC_3810.JPG";
    inputArgs << "/media/data/tmp/timelapse/DSC_3810_2.JPG";
    inputArgs << "/media/data/tmp/timelapse/DSC_3811.JPG";
    inputArgs << "/media/data/tmp/timelapse/DSC_3812.JPG";
    inputArgs << "/media/data/tmp/timelapse/DSC_3813.JPG";
    inputArgs << "/media/data/tmp/timelapse/DSC_3814.JPG";
    inputArgs << "/media/data/tmp/timelapse/DSC_3815.JPG";
    inputArgs << "/media/data/tmp/timelapse/DSC_3816.JPG";
    inputArgs << "/media/data/tmp/timelapse/DSC_3817.JPG";

    // prepare structures configuration
    StabData _s;
    size_t size = sizeof (StabData);
    memset(&_s, 0, size);
    StabData *s = &_s;
    _s.result = (char*) "/tmp/timelapse_vidstab.trf";
    uint width = 4928; //1920; // TODO: read from image
    uint height = 3264; //1080; 


    // initialization
    VSMotionDetect* md = &(s->md);
    VSFrameInfo fi;

    vsFrameInfoInit(&fi, width, height, PF_RGB24);
    fi.planes = 1; // I don't understand vs frame info... But later is assert for planes == 1

    s->conf.algo = 1;
    s->conf.modName = "vidstabdetect";

    // see https://github.com/georgmartius/vid.stab
    s->conf.shakiness = 1;
    s->conf.accuracy = 15;
    s->conf.stepSize = 6;
    s->conf.contrastThreshold = 0.3;
    s->conf.show = 0;

    if (vsMotionDetectInit(md, &s->conf, &fi) != VS_OK) {
      err << "Initialization of Motion Detection failed, please report a BUG";
      exit(1);
    }
    vsMotionDetectGetConfig(&s->conf, md);
    verboseOutput << "Video stabilization settings (pass 1/2):" << endl;
    verboseOutput << "     shakiness = " << s->conf.shakiness << endl;
    verboseOutput << "      accuracy = " << s->conf.accuracy << endl;
    verboseOutput << "      stepsize = " << s->conf.stepSize << endl;
    verboseOutput << "   mincontrast = " << s->conf.contrastThreshold << endl;
    verboseOutput << "        tripod = " << s->conf.virtualTripod << endl;
    verboseOutput << "          show = " << s->conf.show << endl;
    verboseOutput << "        result = " << s->result << endl;

    s->f = fopen(s->result, "w");
    if (s->f == NULL) {
      die << QString("cannot open transform file %1").arg(s->result);
    } else {
      if (vsPrepareFile(md, s->f) != VS_OK) {
        die << QString("cannot write to transform file %1").arg(s->result);
      }
    }

    // process frames
    for (QString input : inputArgs) { // TODO : for each frame
      verboseOutput << "read " << input << endl;
      Magick::Image image;
      image.read(input.toStdString());
      Q_ASSERT(image.baseColumns() == width && image.baseRows() == height);
      Magick::Blob blob;
      // set raw RGBS output format & convert it into a Blob  
      image.magick("RGB");
      image.write(&blob);

      LocalMotions localmotions;
      VSFrame frame;
      int plane;

      for (plane = 0; plane < md->fi.planes; plane++) {
        Q_ASSERT(blob.length() == image.baseColumns() * image.baseRows() * 3);
        frame.data[plane] = (uint8_t*) blob.data(); // TODO: image data?
        frame.linesize[plane] = image.baseColumns() * 3; // TODO: it is correct?
      }
      if (vsMotionDetection(md, &localmotions, &frame) != VS_OK) {
        die << "motion detection failed";
      } else {
        if (vsWriteToFile(md, s->f, &localmotions) != VS_OK) {
          die << "cannot write to transform file";
        }
        vs_vector_del(&localmotions);
      }
    }
    // TODO

    // motion detect cleanup  
    if (s->f) {
      fclose(s->f);
      s->f = NULL;
    }

    vsMotionDetectionCleanup(md);

    // init transofrmation
    // TODO

    // transform frames
    // TODO

    // cleanup transformation
    // TODO

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

