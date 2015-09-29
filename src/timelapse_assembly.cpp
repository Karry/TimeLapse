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

#include "timelapse_assembly.h"
#include "timelapse_assembly.moc"
#include "black_hole_device.h"

#include <QtCore/QObject>
#include <QtCore/QDebug>
#include <QtCore/QTimer>
#include <QtCore/QTextStream>
#include <QtCore/QByteArray>
#include <QtCore/QCoreApplication>
#include <QtCore/QCommandLineParser>
#include <QtCore/QProcess>

#include <QtCore/QDir>


using namespace std;
using namespace timelapse;

namespace timelapse {

  TimeLapseAssembly::TimeLapseAssembly(int &argc, char **argv) :
  QCoreApplication(argc, argv),
  _out(stdout), _err(stderr), _dryRun(false), _verboseOutput(stdout), _blackHole(NULL),
  _forceOverride(false), _inputs(),
  _tmpBaseDir(QDir::tempPath()),
  _tempDir(NULL),
  _output("timelapse.mkv"),
  _width(1920), _height(1080),
  _fps(25), _length(-1), _frameCount(-1), _bitrate("40000k"), _codec("libx264"),
  _noStrictInterval(false), _blendFrames(false),
  _frameNumberLocale(QLocale::c()) {

    _frameNumberLocale.setNumberOptions(QLocale::OmitGroupSeparator);

    setApplicationName("TimeLapse assembly tool");
    setApplicationVersion(VERSION_TIMELAPSE);
  }

  /*
  QString TimeLapseAssembly::getApplicationVersion(){
    return VERSION_TIMELAPSE;
  }
   */

  TimeLapseAssembly::~TimeLapseAssembly() {
    if (_blackHole != NULL) {
      _verboseOutput.flush();
      _verboseOutput.setDevice(NULL);
      delete _blackHole;
      _blackHole = NULL;
    }
  }

  class ErrorMessageHelper {
  public:

    ErrorMessageHelper(QIODevice *errDev, QCommandLineParser *parser) :
    _coloredTerm(true), _err(errDev), _parser(parser) {
    }

    void operator<<(const QString &s) {
      //bold red text\033[0m\n";
      if (_coloredTerm)
        _err << "\033[1;31m";
      _err << endl << "!!! " << s << " !!!";
      if (_coloredTerm)
        _err << "\033[0m";
      _err << endl << endl;
      _parser->showHelp(-1);
    }

  protected:
    bool _coloredTerm; // TODO: determine if terminal is color capable
    QTextStream _err;
    QCommandLineParser *_parser;
  };

  void TimeLapseAssembly::parseArguments() {
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

    // check output
    //if (!parser.isSet(outputOption))
    //  die << "No output specified";

    if (parser.isSet(outputOption))
      _output = QFileInfo(parser.value(outputOption));
    //_err << endl << "output: " << _output.filePath() << endl << endl;
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

    _verboseOutput << "inputs: " << inputArgs.join(", ") << endl;
    for (QString inputArg : inputArgs) {
      QFileInfo i(inputArg);
      if (i.isFile() || i.isSymLink()) {
        _verboseOutput << "Input file: " << inputArg << endl;
        _inputs.append(InputImageInfo(i));
      } else if (i.isDir()) {
        _verboseOutput << "Dive into directory: " << inputArg << endl;
        // list files in directory (no recursive)
        QDir d(i.filePath());
        QFileInfoList l = d.entryInfoList(QDir::Files, QDir::Name);
        _verboseOutput << "...found " << l.size() << " entries" << endl;
        for (QFileInfo i2 : l) {
          if (i2.isFile() || i2.isSymLink()) {
            _verboseOutput << "Input file: " << i2.filePath() << endl;
            _inputs.append(InputImageInfo(i2));
          } else {
            die << (QString("Can't find input ") + i2.filePath());
          }
        }
      } else {
        die << (QString("Can't find input ") + inputArg);
      }
    }
    if (_inputs.empty())
      die << "No input images found!";

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

  }

  /**
   *  determine input image to output frame mapping
   */
  void TimeLapseAssembly::setupOneToOneFrameMapping() {
    int frame = 0;
    QList<InputImageInfo> inputList;
    for (InputImageInfo input : _inputs) {
      input.frame = frame;
      //_verboseOutput << input.file.fileName() << " frame " << input.frame << endl;
      inputList.append(input);
      frame++;
    }
    _inputs = inputList;
    _frameCount = frame;
    emit inputsMappedToFrames();
  }

  /**
   * Determine input image to output frame mapping
   */
  void TimeLapseAssembly::setupFrameMapping() {
    _frameCount = _length * _fps;
    if (_frameCount <= 0) {
      _err << "Frame count have to be possitive!" << endl;
      exit(-1);
      return;
    }
    if (_frameCount % _inputs.size() != 0) {
      _err << "Count of video frames is not integer multiple of count of input images." << endl;
    }
    if (_frameCount < _inputs.size()) {
      _err << "Count of video frames (" << _frameCount << ") is less than input images "
        "(" << _inputs.size() << "). Some images will be skipped!" << endl;
    }

    double frameStep = ((double) _frameCount) / ((double) _inputs.size());
    if (frameStep <= 0) {
      _err << "Frame step between images is too low!" << endl;
      exit(-1);
      return;
    }
    double currentFrame = 0;
    int lastFrame = -1;
    QList<InputImageInfo> inputList;
    for (InputImageInfo input : _inputs) {
      input.frame = currentFrame;
      if (input.frame > lastFrame) {
        _verboseOutput << "Mapping " << input.file.fileName() << " to frame " << input.frame
          << " (step " << (lastFrame < 0 ? 0 : input.frame - lastFrame) << ")" << endl;
        inputList.append(input);
      } else {
        _verboseOutput << "Skip " << input.file.fileName() << endl;
      }
      lastFrame = input.frame;
      currentFrame += frameStep;
    }
    _inputs = inputList;
    if (_inputs.size() <= 0) {
      _err << "No images after mapping!" << endl;
      exit(-1);
      return;
    }
    _verboseOutput << "Sum of frames: " << _frameCount << endl;

    emit inputsMappedToFrames();
  }

  /**
   *  determine input image to output frame mapping
   */
  void TimeLapseAssembly::setupVariableIntervalFrameMapping() {
    _frameCount = _length * _fps;
    if (_frameCount <= 0) {
      _err << "Frame count have to be possitive!" << endl;
      exit(-1);
      return;
    }
    if (_frameCount < _inputs.size()) {
      _err << "Count of video frames (" << _frameCount << ") is less than input images "
        "(" << _inputs.size() << "). Some images will be skipped!" << endl;
    }

    // read date-time from image exif or use file modification time
    QDateTime startTimestamp;
    QDateTime stopTimestamp;
    QDateTime lastTimestamp;
    bool first = true;
    QList<InputImageInfo> inputList;
    for (InputImageInfo input : _inputs) {
      Magick::Image image(input.file.filePath().toStdString());

      QString exifDateTime = QString::fromStdString(image.attribute("EXIF:DateTime"));
      if (exifDateTime.length() == 0) {
        _err << "Image " << input.file.fileName() << " don't have EXIF:DateTime property. Using file modification time." << endl;
        input.timestamp = input.file.lastModified();
      } else {
        // 2015:09:15 07:00:28
        input.timestamp = QDateTime::fromString(exifDateTime, QString("yyyy:MM:dd HH:mm:ss"));
      }
      _verboseOutput << input.file.fileName() << " EXIF:DateTime : " << exifDateTime
        << " (" << input.timestamp.toString(Qt::ISODate) << ")" << endl;
      if (!first && lastTimestamp > input.timestamp) {
        _err << "Images are not sorted by its time! Skip " << input.file.fileName() << endl;
      } else {
        inputList.append(input);
        if (first || input.timestamp < startTimestamp)
          startTimestamp = input.timestamp;
        if (first || input.timestamp > stopTimestamp)
          stopTimestamp = input.timestamp;
        lastTimestamp = input.timestamp;
      }
      first = false;
    }
    _inputs = inputList;

    qint64 realTimeLapsDurationMs = startTimestamp.msecsTo(stopTimestamp);
    if (realTimeLapsDurationMs <= 0) {
      _err << "All images has equal timestamp!" << endl;
      exit(-2);
      return;
    }

    _verboseOutput << "Real time-laps duration: " << realTimeLapsDurationMs << " ms" << endl;
    _verboseOutput << "Sum of frames: " << _frameCount << endl;

    inputList = QList<InputImageInfo>();
    int lastFrame = -1;
    int maxStep = 0;
    for (InputImageInfo input : _inputs) {
      input.frame = (
        ((double) startTimestamp.msecsTo(input.timestamp)) / ((double) realTimeLapsDurationMs)
        * ((double) _frameCount));

      if (input.frame >= _frameCount) // last frame
        input.frame = _frameCount - 1;

      int step = (lastFrame < 0 ? 0 : input.frame - lastFrame);
      _verboseOutput << input.file.fileName() << " " << input.timestamp.toString(Qt::ISODate)
        << " > frame " << input.frame << " (step " << step << ")" << endl;
      if (step > maxStep)
        maxStep = step;

      if (lastFrame >= input.frame) {
        _err << "Skip image " << input.file.fileName() << ", it is too early after previous." << endl
          << "You can try to increase output length or fps." << endl;
      } else {
        inputList.append(input);
        lastFrame = input.frame;
      }
    }
    _inputs = inputList;
    _verboseOutput << "Maximum step is " << maxStep << " frames." << endl;

    if (_inputs.isEmpty()) {
      _err << "Input list is empty!" << endl;
      exit(-2);
      return;
    }
    emit inputsMappedToFrames();
  }

  QString TimeLapseAssembly::leadingZeros(int i, int leadingZeros) {
    // default locale can include thousand delimiter
    QString s = _frameNumberLocale.toString(i);
    if (leadingZeros <= s.length())
      return s;

    return s.prepend(QString(leadingZeros - s.length(), '0'));
  }

  Magick::Image TimeLapseAssembly::cropAndResize(int f, const Magick::Image &i) {
    Magick::Image copy = i;
    Magick::Geometry g(_width, _height);
    g.aspect(true);
    copy.resize(g);
    return copy;
  }

  void TimeLapseAssembly::writeFrame(int f, const Magick::Image &i) {
    QString framePath = _tempDir->path() + QDir::separator()
      + leadingZeros(f, FRAME_FILE_LEADING_ZEROS) + QString(".jpeg");

    _verboseOutput << "Write frame " << framePath << endl;
    if (!_dryRun) {
      cropAndResize(f, i).write(framePath.toStdString());
    }

  }

  void TimeLapseAssembly::blendFrameTransition(int f1, const Magick::Image * i1, int f2, const Magick::Image *i2) {
    if (_blendFrames && i2 != NULL) {
      _verboseOutput << "Blending images for frames " << f1 << " ... " << f2 << endl;
      for (int f = f1; f < f2; f++) {
        Magick::Image blended = *i1;

        double opacity = 1.0 - ((double) (f - f1) / ((double) (f2 - f1)));
        if (f - f1 > 0) { // for 100 % transparency, we don't have to composite
          Magick::Image secondLayer = *i2;
          _verboseOutput << "Blend with next image with " << (opacity * 100) << " % transparency" << endl;
          secondLayer.opacity(((double) QuantumRange) * opacity);
          blended.composite(secondLayer, 0, 0, Magick::DissolveCompositeOp);
        }
        writeFrame(f, blended);
      }
    } else {
      _verboseOutput << "Duplicate image for frames " << f1 << " ... " << f2 << endl;
      for (int f = f1; f < f2; f++) {
        writeFrame(f, *i1);
      }
    }
  }

  void TimeLapseAssembly::prepareFrames() {
    _verboseOutput << "Preparing frames..." << endl;

    int prevFrame = 0;
    Magick::Image * prevImage = NULL;
    for (InputImageInfo input : _inputs) {
      _verboseOutput << "Loading " << input.file.filePath() << endl;
      Magick::Image * inputImage = new Magick::Image(input.file.filePath().toStdString());

      if (prevImage != NULL) {
        blendFrameTransition(prevFrame, prevImage, input.frame, inputImage);
        delete prevImage;
      }
      prevImage = inputImage;
      prevFrame = input.frame;
    }
    if (prevImage != NULL) {
      blendFrameTransition(prevFrame, prevImage, _frameCount, NULL);
      delete prevImage;
    }

    emit framesPrepared();
  }

  void TimeLapseAssembly::assemblyVideo() {
    _verboseOutput << "Assembling video..." << endl;

    QString cmd = "avconv";
    QProcess avconv;
    avconv.setProcessChannelMode(QProcess::MergedChannels);
    avconv.start("avconv", QStringList() << "-version");
    if (!avconv.waitForFinished()) {
      _verboseOutput << "avconv exited with error, try to use ffmpeg" << endl;
      QProcess ffmpeg;
      ffmpeg.setProcessChannelMode(QProcess::MergedChannels);
      ffmpeg.start("ffmpeg", QStringList() << "-version");
      if (!ffmpeg.waitForFinished()) {
        _err << "Both commands (avconv, ffmpeg) fails! Try to use ffmpeg.";
      }
      cmd = "ffmpeg";
    }

    // avconv -f image2 -r $fps -s $res -i morphed/%06d.jpg -b:v $bitrate -c:v libx264  video.mkv
    QStringList args = QStringList()
      << "-f" << "image2"
      << "-r" << QString("%1").arg(_fps)
      << "-s" << QString("%1x%2").arg(_width).arg(_height)
      << "-i" << (_tempDir->path() + QDir::separator() + QString("%0") + QString("%1d.jpeg").arg(FRAME_FILE_LEADING_ZEROS))
      << "-b:v" << _bitrate
      << "-c:v" << _codec
      << "-y" // Overwrite output file without asking
      << "-r" << QString("%1").arg(_fps)
      << _output.filePath();

    _verboseOutput << "Executing:" << endl << cmd << " " << args.join(' ') << endl;
    if (!_dryRun) {
      QProcess proc;
      proc.setProcessChannelMode(QProcess::MergedChannels);
      proc.start(cmd, args);
      if (!proc.waitForFinished(-1 /* no timeout */)) {
        _err << cmd << " failed:" << proc.errorString() << endl;
        exit(-2);
        return;
      }
      _verboseOutput << cmd << " output:\n" << proc.readAll();
    }

    emit videoAssembled();
  }

  void TimeLapseAssembly::cleanup() {
    if (_tempDir != NULL) {
      delete _tempDir;
      _tempDir = NULL;
    }
    emit done();
  }

  void TimeLapseAssembly::run() {

    parseArguments();

    // build workflow
    // in future can be workflow graph more sophisticated...
    if (_length < 0) {
      connect(this, SIGNAL(startAssembly()), this, SLOT(setupOneToOneFrameMapping()));
    } else if (_noStrictInterval) {
      connect(this, SIGNAL(startAssembly()), this, SLOT(setupVariableIntervalFrameMapping()));
    } else {
      connect(this, SIGNAL(startAssembly()), this, SLOT(setupFrameMapping()));
    }

    connect(this, SIGNAL(inputsMappedToFrames()), this, SLOT(prepareFrames()));
    connect(this, SIGNAL(framesPrepared()), this, SLOT(assemblyVideo()));
    connect(this, SIGNAL(videoAssembled()), this, SLOT(cleanup()));
    connect(this, SIGNAL(done()), this, SLOT(quit()));

    emit startAssembly();
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

