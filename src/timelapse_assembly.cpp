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

#include "timelapse_assembly.h"
#include "timelapse_assembly.moc"

#include "timelapse.h"
#include "black_hole_device.h"

#include "pipeline.h"
#include "pipeline_handler.h"
#include "pipeline_frame_mapping.h"
#include "pipeline_frame_prepare.h"
#include "pipeline_video_assembly.h"
#include "pipeline_write_frame.h"
#include "pipeline_resize_frame.h"
#include "pipeline_deflicker.h"

using namespace std;
using namespace timelapse;

namespace timelapse {

  TimeLapseAssembly::TimeLapseAssembly(int &argc, char **argv) :
  QCoreApplication(argc, argv),
  _out(stdout), _err(stderr),
  _dryRun(false), deflickerAvg(false), deflickerDebugView(false), wmaCount(-1),
  _verboseOutput(stdout), _blackHole(NULL),
  _forceOverride(false),
  _tmpBaseDir(QDir::tempPath()),
  _tempDir(NULL),
  _output("timelapse.mkv"),
  _width(1920), _height(1080),
  _fps(25), _length(-1), _frameCount(-1), _bitrate("40000k"), _codec("libx264"),
  _noStrictInterval(false), _blendFrames(false),
  pipeline(NULL) {

    setApplicationName("TimeLapse assembly tool");
    setApplicationVersion(VERSION_TIMELAPSE);
  }

  TimeLapseAssembly::~TimeLapseAssembly() {
    if (_blackHole != NULL) {
      _verboseOutput.flush();
      _verboseOutput.setDevice(NULL);
      delete _blackHole;
      _blackHole = NULL;
    }
  }

  QStringList TimeLapseAssembly::parseArguments() {
    QCommandLineParser parser;
    ErrorMessageHelper die(_err.device(), &parser);

    parser.setApplicationDescription("Tool for assembly time-lapse video from sequence of images.");
    parser.addHelpOption();
    parser.addVersionOption();

    parser.addPositionalArgument("source(s)",
      QCoreApplication::translate("main", "Source images (images or directories with images)."));

    QCommandLineOption outputOption(QStringList() << "o" << "output",
      QCoreApplication::translate("main", "Output video file (default is timelapse.mkv)."),
      QCoreApplication::translate("main", "output"));
    parser.addOption(outputOption);

    QCommandLineOption widthOption(QStringList() << "width",
      QCoreApplication::translate("main", "Output video width. Default is 1920."),
      QCoreApplication::translate("main", "width"));
    parser.addOption(widthOption);

    QCommandLineOption heightOption(QStringList() << "height",
      QCoreApplication::translate("main", "Output video height. Default is 1080."),
      QCoreApplication::translate("main", "height"));
    parser.addOption(heightOption);

    QCommandLineOption fpsOption(QStringList() << "fps",
      QCoreApplication::translate("main", "Output video fps (frames per second). Default is 25."),
      QCoreApplication::translate("main", "fps"));
    parser.addOption(fpsOption);

    QCommandLineOption lengthOption(QStringList() << "length",
      QCoreApplication::translate("main", "Output video length (in seconds). \n"
      "If this option is not defined, then length will be computed from number of inputs images / fps."),
      QCoreApplication::translate("main", "length"));
    parser.addOption(lengthOption);

    QCommandLineOption bitrateOption(QStringList() << "b" << "bitrate",
      QCoreApplication::translate("main", "Output video bitrate. Default %1.").arg(_bitrate),
      QCoreApplication::translate("main", "bitrate"));
    parser.addOption(bitrateOption);

    QCommandLineOption codecOption(QStringList() << "codec",
      QCoreApplication::translate("main", "Output video codec. Default %1.").arg(_codec),
      QCoreApplication::translate("main", "codec"));
    parser.addOption(codecOption);

    QCommandLineOption noStrictIntervalOption(QStringList() << "no-strict-interval",
      QCoreApplication::translate("main", "Don't map input images to output video frames with strict interval. "
      "Input image to output video frame mapping will be computed from image "
      "timestamp (EXIF metadata will be used or file modification time)."));
    parser.addOption(noStrictIntervalOption);

    QCommandLineOption blendFramesOption(QStringList() << "blend-frames",
      QCoreApplication::translate("main", "Blend frame transition."));
    parser.addOption(blendFramesOption);

    QCommandLineOption deflickerAvgOption(QStringList() << "deflicker-average",
      QCoreApplication::translate("main", "Deflicker images by average luminance."));
    parser.addOption(deflickerAvgOption);

    QCommandLineOption wmaCountOption(QStringList() << "deflicker-wm-average",
      QCoreApplication::translate("main", "Deflicker images using weighted moving average of luminance."
      "Argument specified count of previous images for compute weighted average."),
      QCoreApplication::translate("main", "count"));
    parser.addOption(wmaCountOption);

    QCommandLineOption deflickerDebugViewOption(QStringList() << "deflicker-debug-view",
      QCoreApplication::translate("main", "Composite one half of output image from original "
      "and second half from image with corrected luminance."));
    parser.addOption(deflickerDebugViewOption);

    QCommandLineOption verboseOption(QStringList() << "V" << "verbose",
      QCoreApplication::translate("main", "Verbose output."));
    parser.addOption(verboseOption);

    QCommandLineOption dryRunOption(QStringList() << "d" << "dryrun",
      QCoreApplication::translate("main", "Just parse arguments, check inputs and prints informations."));
    parser.addOption(dryRunOption);

    QCommandLineOption forceOption(QStringList() << "f" << "force",
      QCoreApplication::translate("main", "Overwrite existing output."));
    parser.addOption(forceOption);

    QCommandLineOption tmpOption(QStringList() << "tmp",
      QCoreApplication::translate("main", "Temp directory. Default is %1.").arg(_tmpBaseDir),
      QCoreApplication::translate("main", "tmp"));
    parser.addOption(tmpOption);

    QCommandLineOption keepTempOption(QStringList() << "k" << "keep-temp",
      QCoreApplication::translate("main", "Keep temporary files (Program will cleanup temporary files at the end by default)."));
    parser.addOption(keepTempOption);

    // Process the actual command line arguments given by the user
    parser.process(*this);

    // verbose?
    if (!parser.isSet(verboseOption)) {
      _blackHole = new BlackHoleDevice();
      _verboseOutput.setDevice(_blackHole);
      //_err << "no verbose"<< endl;
    } else {
      // verbose
      _verboseOutput << "Turning on verbose output..." << endl;
      _verboseOutput << applicationName() << " " << applicationVersion() << endl;
    }

    _forceOverride = parser.isSet(forceOption);
    _dryRun = parser.isSet(dryRunOption);
    deflickerAvg = parser.isSet(deflickerAvgOption);
    deflickerDebugView = parser.isSet(deflickerDebugViewOption);

    // wma?
    if (parser.isSet(wmaCountOption)) {
      if (deflickerAvg)
        _err << "Ignore average luminance option. Weighted moving average will be used." << endl;
      deflickerAvg = true;
      bool ok = false;
      int i = parser.value(wmaCountOption).toInt(&ok);
      if (!ok) die << "Cant parse wma count.";
      if (i < 0) die << "Wma count have to be possitive";
      wmaCount = (size_t) i;
    }

    if (parser.isSet(outputOption))
      _output = QFileInfo(parser.value(outputOption));

    if (_output.exists()) {
      if (_output.isDir())
        die << "Output is directory";
      if (!_forceOverride)
        die << "Output already exists. Consider \"force\" option.";
    }

    // inputs
    QStringList inputArgs = parser.positionalArguments();
    if (inputArgs.empty())
      die << "No input given";

    bool ok = false;
    if (parser.isSet(widthOption)) {
      _width = parser.value(widthOption).toInt(&ok);
      if (!ok) die << "Can't parse width";
    }
    if (parser.isSet(heightOption)) {
      _height = parser.value(heightOption).toInt(&ok);
      if (!ok) die << "Can't parse height";
    }
    if (parser.isSet(fpsOption)) {
      _fps = parser.value(fpsOption).toFloat(&ok);
      if (!ok) die << "Can't parse fps";
      if (_fps <= 0) die << "FPS have to be possitive!";
    }
    if (parser.isSet(lengthOption)) {
      _length = parser.value(lengthOption).toFloat(&ok);
      if (!ok) die << "Can't parse length";
      if (_length <= 0) die << "Length have to be possitive!";
    }
    if (parser.isSet(bitrateOption))
      _bitrate = parser.value(bitrateOption);
    if (parser.isSet(codecOption))
      _codec = parser.value(codecOption);

    _noStrictInterval = parser.isSet(noStrictIntervalOption);
    if (_noStrictInterval && _length < 0) {
      _err << "Video length is not setup, ignore \"no-strict-interval\"." << endl;
    }

    _blendFrames = parser.isSet(blendFramesOption);
    if (_blendFrames && _length < 0) {
      _err << "Video length is not setup, ignore \"blend-frame\" option." << endl;
    }

    if (parser.isSet(tmpOption))
      _tmpBaseDir = parser.value(tmpOption);
    // check temp dir
    _tempDir = new QTemporaryDir(_tmpBaseDir + QDir::separator() + "timelapse_");
    if (!_tempDir->isValid())
      die << "Can't create temp directory";

    _verboseOutput << "Tmp dir: " << QDir::tempPath() << endl;
    _tempDir->setAutoRemove(!parser.isSet(keepTempOption));
    _verboseOutput << "Using temp directory " << _tempDir->path() << endl;

    return inputArgs;
  }

  void TimeLapseAssembly::onError(QString msg) {
    emit cleanup(1);
  }

  void TimeLapseAssembly::cleanup(int exitCode) {
    if (pipeline != NULL) {
      delete pipeline;
      pipeline = NULL;
    }
    if (_tempDir != NULL) {
      delete _tempDir;
      _tempDir = NULL;
    }

    exit(exitCode);
  }

  void TimeLapseAssembly::run() {

    QStringList inputArguments = parseArguments();

    // build processing pipeline
    pipeline = new Pipeline(inputArguments, false, &_verboseOutput, &_err);

    if (deflickerAvg) {
      *pipeline << new ComputeLuminance(&_verboseOutput);
    }

    if (_length < 0) {
      *pipeline << new OneToOneFrameMapping();
    } else if (_noStrictInterval) {
      *pipeline << new ImageMetadataReader(&_verboseOutput, &_err);
      *pipeline << new VariableIntervalFrameMapping(&_verboseOutput, &_err, _length, _fps);
    } else {
      *pipeline << new ConstIntervalFrameMapping(&_verboseOutput, &_err, _length, _fps);
    }

    if (deflickerAvg) {
      if (wmaCount > 0)
        *pipeline << new WMALuminance(&_verboseOutput, wmaCount);
      else
        *pipeline << new ComputeAverageLuminance(&_verboseOutput);
      *pipeline << new AdjustLuminance(&_verboseOutput, deflickerDebugView);
    }

    if (_blendFrames) {
      *pipeline << new BlendFramePrepare(&_verboseOutput);
    } else {
      *pipeline << new FramePrepare(&_verboseOutput);
    }
    *pipeline << new ResizeFrame(_width, _height);
    *pipeline << new WriteFrame(QDir(_tempDir->path()), &_verboseOutput, _dryRun);

    * pipeline << new VideoAssembly(QDir(_tempDir->path()), &_verboseOutput, &_err, _dryRun,
      _output, _width, _height, _fps, _bitrate, _codec);

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

  TimeLapseAssembly app(argc, argv);
  Magick::InitializeMagick(*argv);

  // This will run the task from the application event loop.
  QTimer::singleShot(0, &app, SLOT(run()));

  return app.exec();
}

