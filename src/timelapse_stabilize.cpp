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
      err << "Output directory already exists." << endl;
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
    StabData *s = &_s;
    memset(s, 0, sizeof (StabData));
    char * statFile = (char*) "/tmp/timelapse_vidstab.trf";
    _s.result = statFile;
    uint width = 4928; //1920; // TODO: read from image
    uint height = 3264; //1080; 

    VS_ERROR = 0;
    VS_OK = 1;


    // initialization
    VSMotionDetect* md = &(s->md);
    VSFrameInfo fi;

    if (!vsFrameInfoInit(&fi, width, height, PF_RGB24)) {
      die << "Failed to initialize frame info";
    }
    fi.planes = 1; // I don't understand vs frame info... But later is assert for planes == 1

    s->conf.algo = 1;
    s->conf.modName = "vidstabdetect";

    // see https://github.com/georgmartius/vid.stab
    s->conf.shakiness = 1;
    s->conf.accuracy = 15;
    s->conf.stepSize = 6;
    s->conf.contrastThreshold = 0.3;
    s->conf.show = 0;
    //s->conf.numThreads = 1;

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

    // motion detect cleanup  
    if (s->f) {
      fclose(s->f);
      s->f = NULL;
    }

    vsMotionDetectionCleanup(md);


    verboseOutput << "Stage 2" << endl;

    // init transformation
    TransformContext _tc;
    TransformContext *tc = &_tc;
    memset(tc, 0, sizeof (TransformContext));

    VSTransformData *td = &(tc->td);

    tc->input = statFile;
    if (!vsFrameInfoInit(&fi, width, height, PF_RGB24)) {
      die << "Failed to initialize frame format";
    }
    fi.planes = 1; // I don't understand vs frame info... But later is assert for planes == 1
    

    // set values that are not initializes by the options
    tc->conf.modName = "vidstabtransform";
    tc->conf.verbose = 1 + tc->debug;
    if (tc->tripod) {
      verboseOutput << "Virtual tripod mode: relative=0, smoothing=0" << endl;
      tc->conf.relative = 0;
      tc->conf.smoothing = 0;
    }
    tc->conf.simpleMotionCalculation = 0;
    tc->conf.storeTransforms = tc->debug;
    tc->conf.smoothZoom = 0;

    if (vsTransformDataInit(td, &tc->conf, &fi, &fi) != VS_OK) {
      die << "initialization of vid.stab transform failed, please report a BUG";
    }

    vsTransformGetConfig(&tc->conf, td);
    verboseOutput << "Video transformation/stabilization settings (pass 2/2):" << endl;
    verboseOutput << "    input     = " << tc->input << endl;
    verboseOutput << "    smoothing = " << tc->conf.smoothing << endl;
    verboseOutput << "    optalgo   = " <<
      (tc->conf.camPathAlgo == VSOptimalL1 ? "opt" :
      (tc->conf.camPathAlgo == VSGaussian ? "gauss" : "avg")) << endl;
    verboseOutput << "    maxshift  = " << tc->conf.maxShift << endl;
    verboseOutput << "    maxangle  = " << tc->conf.maxAngle << endl;
    verboseOutput << "    crop      = " << (tc->conf.crop ? "Black" : "Keep") << endl;
    verboseOutput << "    relative  = " << (tc->conf.relative ? "True" : "False") << endl;
    verboseOutput << "    invert    = " << (tc->conf.invert ? "True" : "False") << endl;
    verboseOutput << "    zoom      = " << (tc->conf.zoom) << endl;
    verboseOutput << "    optzoom   = " << (
      tc->conf.optZoom == 1 ? "Static (1)" : (tc->conf.optZoom == 2 ? "Dynamic (2)" : "Off (0)")) << endl;
    if (tc->conf.optZoom == 2)
      verboseOutput << "    zoomspeed = " << tc->conf.zoomSpeed << endl;
    verboseOutput << "    interpol  = " << getInterpolationTypeName(tc->conf.interpolType) << endl;


    FILE *f = fopen(tc->input, "r");
    if (!f) {
      die << QString("cannot open input file %1, errno %2").arg(tc->input).arg(errno);
    } else {
      VSManyLocalMotions mlms;
      if (vsReadLocalMotionsFile(f, &mlms) == VS_OK) {
        // calculate the actual transforms from the local motions
        if (vsLocalmotions2Transforms(td, &mlms, &tc->trans) != VS_OK) {
          die << "calculating transformations failed";
        }
      } else { // try to read old format
        if (!vsReadOldTransforms(td, f, &tc->trans)) { /* read input file */
          die << QString("error parsing input file %1").arg(tc->input);
        }
      }
    }
    fclose(f);

    if (vsPreprocessTransforms(td, &tc->trans) != VS_OK) {
      die << "error while preprocessing transforms";
    }

    // transform frames
    for (QString input : inputArgs) { // for each frame
      verboseOutput << "read " << input << endl;
      Magick::Image image;
      image.read(input.toStdString());
      Q_ASSERT(image.baseColumns() == width && image.baseRows() == height);
      Magick::Blob blob;
      // set raw RGBS output format & convert it into a Blob  
      image.magick("RGB");
      image.write(&blob);

      VSFrame inframe;
      int plane;

      for (plane = 0; plane < vsTransformGetSrcFrameInfo(td)->planes; plane++) {
        inframe.data[plane] = (uint8_t*) blob.data(); // TODO: image data?
        inframe.linesize[plane] = image.baseColumns() * 3; // TODO: it is correct?
      }

      vsTransformPrepare(td, &inframe, &inframe); // direct

      vsDoTransform(td, vsGetNextTransform(td, &tc->trans));

      vsTransformFinish(td);

      QString framePath = output.path() + QDir::separator()+(QFile(input)).fileName();
      Magick::Geometry g(width, height);
      Magick::Image oimage(blob, g, "RGB");

      verboseOutput << "Write frame " << framePath << endl;
      if (!dryRun) {
        oimage.write(framePath.toStdString());
      }
    }


    // cleanup transformation
    vsTransformDataCleanup(&tc->td);
    vsTransformationsCleanup(&tc->trans);

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

