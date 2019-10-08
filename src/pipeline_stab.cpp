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

#include <QtCore/QCoreApplication>

#include "pipeline_stab.h"
#include "pipeline_stab.moc"
#include "libvidstab.h"
#include "timelapse_stabilize.h"

using namespace std;
using namespace timelapse;


namespace timelapse {

  int stabLog(int type, const char* tag, const char* format, ...) {
    // format message
    va_list valist;
    va_start(valist, format);
    QString msg = QString().vsprintf(format, valist);
    va_end(valist);

    QTextStream * out = verboseOutput;
    switch (type) {
      case STAB_LOG_ERROR:
      case STAB_LOG_WARNING:
        out = err;
    }
    *out << tag << ": " << msg; // << endl;
    return VS_OK;
  }

  void stabInit(QTextStream *_verboseOutput, QTextStream *_err) {
    // setup constants
    VS_ERROR = 0;
    VS_OK = 1;

    VS_ERROR_TYPE = STAB_LOG_ERROR;
    VS_WARN_TYPE = STAB_LOG_WARNING;
    VS_INFO_TYPE = STAB_LOG_INFO;
    VS_MSG_TYPE = STAB_LOG_VERBOSE;

    // logging
    vs_log = stabLog;
    verboseOutput = _verboseOutput;
    err = _err;
  }

  StabConfig::StabConfig() :
  stabStateFile(NULL), dryRun(false),

  threadsOption(NULL),

  shakinessOption(NULL),
  accuracyOption(NULL),
  stepSizeOption(NULL),
  minContrastOption(NULL),
  tripodOption(NULL),
  showOption(NULL),

  smoothingOption(NULL),
  camPathAlgoOption(NULL),
  maxShiftOption(NULL),
  maxAngleOption(NULL),
  cropBlackOption(NULL),
  invertOption(NULL),
  relativeOption(NULL),
  zoomOption(NULL),
  optZoomOption(NULL),
  zoomSpeedOption(NULL),
  interpolOption(NULL) {

    QTemporaryFile *tf = new QTemporaryFile();
    if (!tf->open()) { // just create temp file    
      throw runtime_error("Failed to create temp file");
    }
    tf->close();
    stabStateFile = tf;

    memset(&mdConf, 0, sizeof (VSMotionDetectConfig));
    memset(&tsConf, 0, sizeof (VSTransformConfig));

    mdConf.algo = 1;
    mdConf.modName = "vidstabdetect";

    // see https://github.com/georgmartius/vid.stab
    mdConf.shakiness = 5;
    mdConf.accuracy = 15;
    mdConf.stepSize = 6;
    mdConf.contrastThreshold = 0.3;
    mdConf.show = 0;
    //mdConf.numThreads = 1;

    // set values that are not initializes by the options
    // https://github.com/georgmartius/vid.stab
    tsConf.modName = "vidstabtransform";
    tsConf.verbose = 1;
    tsConf.smoothing = 10; // 0 is a special case where a static camera is simulated.
    tsConf.camPathAlgo = VSGaussian; // gauss: Gaussian kernel low-pass filter on camera motion (default). 
    tsConf.maxShift = -1; // Set maximal number of pixels to translate frames. Default value is -1, meaning: no limit.
    tsConf.maxAngle = -1; //Set maximal angle in radians (degree*PI/180) to rotate frames. Default value is -1, meaning: no limit.
    tsConf.crop = VSKeepBorder; // Keep image information from previous frame (default). 
    tsConf.simpleMotionCalculation = 0;
    tsConf.storeTransforms = 0;
    tsConf.smoothZoom = 0;

    // init options
    threadsOption = new QCommandLineOption(
      QStringList() << "stab-md-threads",
      QCoreApplication::translate("main", "Number of threads used for motion detection. "
      "Default is count of physical processors -1."),
      QCoreApplication::translate("main", "threads"));

    shakinessOption = new QCommandLineOption(
      QStringList() << "stab-md-shakiness",
      QCoreApplication::translate("main", "Set the shakiness of input video or quickness of camera. "
      "It accepts an integer in the range 1-10, a value of 1 means little shakiness, "
      "a value of 10 means strong shakiness. Default value is 5."),
      QCoreApplication::translate("main", "shakiness"));

    accuracyOption = new QCommandLineOption(
      QStringList() << "stab-md-accuracy",
      QCoreApplication::translate("main", "Set the accuracy of the detection process. "
      "It must be a value in the range 1-15. A value of 1 means low accuracy, "
      "a value of 15 means high accuracy. Default value is 15."),
      QCoreApplication::translate("main", "accuracy"));

    stepSizeOption = new QCommandLineOption(
      QStringList() << "stab-md-stepsize",
      QCoreApplication::translate("main", "Set stepsize of the search process. "
      "The region around minimum is scanned with 1 pixel resolution. Default value is 6."),
      QCoreApplication::translate("main", "stepsize"));

    minContrastOption = new QCommandLineOption(
      QStringList() << "stab-md-mincontrast",
      QCoreApplication::translate("main", "Set minimum contrast. Any measurement "
      "field having contrast below this value is discarded. Must be a floating "
      "point value in the range 0-1. Default value is 0.3."),
      QCoreApplication::translate("main", "mincontrast"));

    tripodOption = new QCommandLineOption(
      QStringList() << "stab-tripod",
      QCoreApplication::translate("main", "Set reference frame number for tripod mode. "
      "If enabled, the motion of the frames is compared to a reference frame "
      "in the filtered stream, identified by the specified number. The intention "
      "is to compensate all movements in a more-or-less static scene and keep "
      "the camera view absolutely still. If set to 0, it is disabled. "
      "The frames are counted starting from 1. "),
      QCoreApplication::translate("main", "tripod"));

    showOption = new QCommandLineOption(
      QStringList() << "stab-md-show",
      QCoreApplication::translate("main", "Show fields and transforms in the resulting "
      "frames for visual analysis. It accepts an integer in the range 0-2. "
      "Default value is 0, which disables any visualization."),
      QCoreApplication::translate("main", "show"));

    smoothingOption = new QCommandLineOption(
      QStringList() << "stab-tr-smoothing",
      QCoreApplication::translate("main", "Set the number of frames (value*2 + 1), used for lowpass "
      "filtering the camera movements. Default value is 10. For example, a number of 10 means that "
      "21 frames are used (10 in the past and 10 in the future) to smoothen the motion in the video. "
      "A larger value leads to a smoother video, but limits the acceleration of the camera "
      "(pan/tilt movements). 0 is a special case where a static camera is simulated."),
      QCoreApplication::translate("main", "smoothing"));

    camPathAlgoOption = new QCommandLineOption(
      QStringList() << "stab-tr-campathalgo",
      QCoreApplication::translate("main", "Set the camera path optimization algorithm. Accepted values are: \n"
      "gauss: Gaussian kernel low-pass filter on camera motion (default).\n"
      "avg: Averaging on transformations."),
      QCoreApplication::translate("main", "campathalgo"));

    maxShiftOption = new QCommandLineOption(
      QStringList() << "stab-tr-maxshift",
      QCoreApplication::translate("main", "Set maximal number of pixels to translate frames. "
      "Default value is -1, meaning: no limit."),
      QCoreApplication::translate("main", "maxshift"));

    maxAngleOption = new QCommandLineOption(
      QStringList() << "stab-tr-maxangle",
      QCoreApplication::translate("main", "Set maximal angle in radians (degree*PI/180) to rotate frames. "
      "Default value is -1, meaning: no limit."),
      QCoreApplication::translate("main", "maxangle"));

    cropBlackOption = new QCommandLineOption(
      QStringList() << "stab-tr-blackcrop",
      QCoreApplication::translate("main", "Fill empty frame borders by black color. By default is keep image from previous frame."));

    invertOption = new QCommandLineOption(
      QStringList() << "stab-tr-invert",
      QCoreApplication::translate("main", "Invert transforms."));

    relativeOption = new QCommandLineOption(
      QStringList() << "stab-tr-relative",
      QCoreApplication::translate("main", "Consider transforms as relative to previous frame. Default is absolute"));

    zoomOption = new QCommandLineOption(
      QStringList() << "stab-tr-zoom",
      QCoreApplication::translate("main", "Set percentage to zoom. A positive value will result in a zoom-in effect, "
      "a negative value in a zoom-out effect. Default value is 0 (no zoom)."),
      QCoreApplication::translate("main", "zoom"));

    optZoomOption = new QCommandLineOption(
      QStringList() << "stab-tr-optzoom",
      QCoreApplication::translate("main", "Set optimal zooming to avoid blank-borders. Accepted values are:\n"
      "0: Disabled.\n"
      "1: Optimal static zoom value is determined (only very strong movements will lead to visible borders) (default).\n"
      "2: Optimal adaptive zoom value is determined (no borders will be visible), see zoomspeed.\n"
      "Note that the value given at zoom is added to the one calculated here."),
      QCoreApplication::translate("main", "optzoom"));

    zoomSpeedOption = new QCommandLineOption(
      QStringList() << "stab-tr-zoomspeed",
      QCoreApplication::translate("main", "Set percent to zoom maximally each frame (enabled when optzoom is set to 2). "
      "Range is from 0 to 5, default value is 0.25."),
      QCoreApplication::translate("main", "zoomspeed"));

    interpolOption = new QCommandLineOption(
      QStringList() << "stab-tr-interpol",
      QCoreApplication::translate("main", "Specify type of interpolation. Available values are:\n"
      "no: No interpolation.\n"
      "linear: Linear only horizontal.\n"
      "bilinear: Linear in both directions (default).\n"
      "bicubic: Cubic in both directions (slow speed)."),
      QCoreApplication::translate("main", "interpol"));


  }

  void StabConfig::addOptions(QCommandLineParser &parser) {

    parser.addOption(*threadsOption);

    parser.addOption(*shakinessOption);
    parser.addOption(*accuracyOption);
    parser.addOption(*stepSizeOption);
    parser.addOption(*minContrastOption);
    parser.addOption(*tripodOption);

    // origin vid.stab don't support show option for packet pixel format
    // "improvements" branch from my fork https://github.com/Karry/vid.stab
    // should be used meanwhile 
    parser.addOption(*showOption);

    parser.addOption(*smoothingOption);
    parser.addOption(*camPathAlgoOption);
    parser.addOption(*maxShiftOption);
    parser.addOption(*maxAngleOption);
    parser.addOption(*cropBlackOption);
    parser.addOption(*invertOption);
    parser.addOption(*relativeOption);
    parser.addOption(*zoomOption);
    parser.addOption(*optZoomOption);
    parser.addOption(*zoomSpeedOption);
    parser.addOption(*interpolOption);

  }

  template<typename T> T StabConfig::getOpt(const QCommandLineParser &parser, ErrorMessageHelper &die,
    const QCommandLineOption &opt, const Option<T> &min, const Option<T> &max, T def,
    QString parseErrMsg, QString outOfRangeErrMsg) {

    bool ok = false;
    T i;
    if (parser.isSet(opt)) {
      if (is_same<int, T>::value)
        i = parser.value(opt).toInt(&ok);
      if (is_same<float, T>::value)
        i = parser.value(opt).toFloat(&ok);
      if (is_same<double, T>::value)
        i = parser.value(opt).toDouble(&ok);

      if (!ok) die << parseErrMsg;

      if ((min.isDefined() && i < *min) || (max.isDefined() && i > *max))
        die << outOfRangeErrMsg;

      return i;
    }
    return def;
  }

  void StabConfig::processOptions(const QCommandLineParser &parser, ErrorMessageHelper &die, QTextStream *err) {

    mdConf.numThreads = getOpt(parser, die, *threadsOption, SomeInt(0), NoneInt, mdConf.numThreads,
      QCoreApplication::translate("main", "Cant parse thread count."),
      QCoreApplication::translate("main", "Thread count have to be possitive."));

    mdConf.shakiness = getOpt(parser, die, *shakinessOption, SomeInt(1), SomeInt(10), mdConf.shakiness,
      QCoreApplication::translate("main", "Cant parse shakiness."),
      QCoreApplication::translate("main", "Shakiness can be in range 1-10."));

    mdConf.accuracy = getOpt(parser, die, *accuracyOption, SomeInt(1), SomeInt(15), mdConf.accuracy,
      QCoreApplication::translate("main", "Cant parse accuracy."),
      QCoreApplication::translate("main", "Acccuracy can be in range 1-15."));

    mdConf.stepSize = getOpt(parser, die, *stepSizeOption, SomeInt(1), SomeInt(1000), mdConf.stepSize,
      QCoreApplication::translate("main", "Cant parse step size."),
      QCoreApplication::translate("main", "Step size can be in range 1-1000."));

    mdConf.contrastThreshold = getOpt(parser, die, *minContrastOption, SomeDouble(0), SomeDouble(1), mdConf.contrastThreshold,
      QCoreApplication::translate("main", "Cant parse contrast threshold."),
      QCoreApplication::translate("main", "Contrast threshold can be in range 0-1."));

    mdConf.show = getOpt(parser, die, *showOption, SomeInt(0), SomeInt(2), mdConf.show,
      QCoreApplication::translate("main", "Cant parse show option."),
      QCoreApplication::translate("main", "Show option can be in range 0-2."));

    tsConf.smoothing = getOpt(parser, die, *smoothingOption, SomeInt(0), NoneInt, tsConf.smoothing,
      QCoreApplication::translate("main", "Cant parse smoothing option."),
      QCoreApplication::translate("main", "Smoothing have to be possitive."));


    if (parser.isSet(*camPathAlgoOption)) {
      QString algoStr = parser.value(*camPathAlgoOption);
      if (algoStr.compare("gauss", Qt::CaseInsensitive) == 0) {
        tsConf.camPathAlgo = VSGaussian;
      } else if (algoStr.compare("avg", Qt::CaseInsensitive) == 0) {
        tsConf.camPathAlgo = VSAvg;
      } else {
        die << QCoreApplication::translate("main", "Invalid cam path algorithm \"%1\".").arg(algoStr);
      }
    }

    tsConf.maxShift = getOpt(parser, die, *maxShiftOption, SomeInt(-1), NoneInt, tsConf.maxShift,
      QCoreApplication::translate("main", "Cant parse max shift option."),
      QCoreApplication::translate("main", "Max. shift can't be smaller than -1."));

    tsConf.maxAngle = getOpt(parser, die, *maxAngleOption, SomeDouble(-1), NoneDouble, tsConf.maxAngle,
      QCoreApplication::translate("main", "Cant parse max angle option."),
      QCoreApplication::translate("main", "Max. angle can't be smaller than -1."));

    tsConf.crop = parser.isSet(*cropBlackOption) ? VSCropBorder : VSKeepBorder;
    tsConf.invert = parser.isSet(*invertOption) ? 1 : 0;
    tsConf.relative = parser.isSet(*relativeOption) ? 1 : 0;

    tsConf.zoom = getOpt(parser, die, *zoomOption, NoneDouble, NoneDouble, tsConf.zoom,
      QCoreApplication::translate("main", "Cant parse zoom option."),
      "");

    tsConf.optZoom = getOpt(parser, die, *optZoomOption, SomeInt(0), SomeInt(2), tsConf.optZoom,
      QCoreApplication::translate("main", "Cant parse opt-zoom option."),
      QCoreApplication::translate("main", "Opt zoom can be 0, 1 or 2."));

    tsConf.zoomSpeed = getOpt(parser, die, *zoomSpeedOption, SomeDouble(0), SomeDouble(5), tsConf.zoomSpeed,
      QCoreApplication::translate("main", "Cant parse zoom speed option."),
      QCoreApplication::translate("main", "Zoom speed can be in range 0-5."));

    if (parser.isSet(*interpolOption)) {
      QString interpolStr = parser.value(*interpolOption);
      if (interpolStr.compare("no", Qt::CaseInsensitive) == 0) {
        tsConf.interpolType = VS_Zero;
      } else if (interpolStr.compare("linear", Qt::CaseInsensitive) == 0) {
        tsConf.interpolType = VS_Linear;
      } else if (interpolStr.compare("bilinear", Qt::CaseInsensitive) == 0) {
        tsConf.interpolType = VS_BiLinear;
      } else if (interpolStr.compare("bicubic", Qt::CaseInsensitive) == 0) {
        tsConf.interpolType = VS_BiCubic;
      } else {
        die << QCoreApplication::translate("main", "Invalid interpolation algorithm \"%1\".").arg(interpolStr);
      }
    }

    mdConf.virtualTripod = getOpt(parser, die, *tripodOption, SomeInt(0), NoneInt, mdConf.virtualTripod,
      QCoreApplication::translate("main", "Cant parse tripod option."),
      QCoreApplication::translate("main", "Tripod option have to be possitive."));

    if (mdConf.virtualTripod > 0) {
      if (tsConf.relative != 0)
        *err << QCoreApplication::translate("main", "Relative transformation is disabled with tripod.") << endl;
      if (tsConf.smoothing != 0)
        *err << QCoreApplication::translate("main", "Smoothing is disabled with tripod.") << endl;
      tsConf.relative = 0;
      tsConf.smoothing = 0;
    }

  }

  FILE* StabConfig::openStabStateFile(const char *mode) {
    std::string name = stabStateFile->fileName().toStdString();
    FILE *result = fopen(name.c_str(), mode);
    if (result == NULL) {
      throw runtime_error(QString("Cannot open transform file %1").arg(stabStateFile->fileName()).toStdString());
    }

    return result;
  }

  StabConfig::~StabConfig() {
    if (stabStateFile != NULL) {
      delete stabStateFile;
      stabStateFile = NULL;
    }

    delete threadsOption;

    delete shakinessOption;
    delete accuracyOption;
    delete stepSizeOption;
    delete minContrastOption;
    delete tripodOption;
    delete showOption;

    delete smoothingOption;
    delete camPathAlgoOption;
    delete maxShiftOption;
    delete maxAngleOption;
    delete cropBlackOption;
    delete invertOption;
    delete relativeOption;
    delete zoomOption;
    delete optZoomOption;
    delete zoomSpeedOption;
    delete interpolOption;

  }

  PipelineStabDetect::PipelineStabDetect(StabConfig *stabConf, QTextStream *verboseOutput, QTextStream *err) :
  stabConf(stabConf),
  initialized(false), width(-1), height(-1), f(NULL),
  verboseOutput(verboseOutput), err(err) {

    memset(&md, 0, sizeof (VSMotionDetect));
    memset(&fi, 0, sizeof (VSFrameInfo));

  }

  PipelineStabDetect::~PipelineStabDetect() {
  }

  void PipelineStabDetect::onLast() {
    if (initialized) {
      *verboseOutput << "closing " << stabConf->stabStateFile->fileName() << endl;
      fclose(f);
      f = NULL;
      vsMotionDetectionCleanup(&md);
    }
    emit last();
  }

  void PipelineStabDetect::onInput2(InputImageInfo info, Magick::Image image) {
    try {
      if (!initialized) {
        init(image);
      }
      if (image.rows() != height || image.columns() != width) {
        throw runtime_error(QString("Not uniform image size! %").arg(info.fileInfo().fileName()).toStdString());
      }


      Magick::Blob blob;
      // set raw RGBS output format & convert it into a Blob  
      if (image.depth() > 8)
        *err << "Warning: we lost some information by converting to 8bit depth (now " << image.depth() << ")" << endl;
      image.depth(8);
      image.magick("RGB");
      image.write(&blob);

      LocalMotions localmotions;
      VSFrame frame;
      size_t dataLen = blob.length();

      Q_ASSERT(fi.planes == 1);
      Q_ASSERT(dataLen == image.baseColumns() * image.baseRows() * 3);

      if (stabConf->mdConf.show > 0) { // create copy of blob
        frame.data[0] = new uint8_t[dataLen];
        memcpy(frame.data[0], blob.data(), dataLen);
      } else {
        frame.data[0] = (uint8_t*) blob.data();
      }
      frame.linesize[0] = image.baseColumns() * 3;

      if (vsMotionDetection(&md, &localmotions, &frame) != VS_OK) {
        throw runtime_error("motion detection failed");
      } else {
        if (vsWriteToFile(&md, f, &localmotions) != VS_OK) {
          throw runtime_error("cannot write to transform file");
        }
        vs_vector_del(&localmotions);
      }

      if (stabConf->mdConf.show > 0) {
        // if we want to store transformations, we have to create new image...
        Magick::Geometry g(width, height);
        Magick::Blob oblob(frame.data[0], dataLen);

        Magick::Image oimage;
        oimage.read(oblob, g, 8, "RGB");
        delete[] frame.data[0];
        emit input(info, oimage);
      } else {
        emit input(info, image);
      }

    } catch (exception &e) {
      emit error(e.what());
    }
  }

  void PipelineStabDetect::init(Magick::Image img) {
    width = img.columns();
    height = img.rows();

    if (!vsFrameInfoInit(&fi, width, height, PF_RGB24)) {
      throw runtime_error("Failed to initialize frame info");
    }
    fi.planes = 1; // I don't understand vs frame info... But later is assert for planes == 1

    if (vsMotionDetectInit(&md, &stabConf->mdConf, &fi) != VS_OK) {
      throw runtime_error("Initialization of Motion Detection failed, please report a BUG");
    }
    vsMotionDetectGetConfig(&stabConf->mdConf, &md);

    *verboseOutput << "Video stabilization settings (pass 1/2):" << endl;
    *verboseOutput << "     shakiness = " << stabConf->mdConf.shakiness << endl;
    *verboseOutput << "      accuracy = " << stabConf->mdConf.accuracy << endl;
    *verboseOutput << "      stepsize = " << stabConf->mdConf.stepSize << endl;
    *verboseOutput << "   mincontrast = " << stabConf->mdConf.contrastThreshold << endl;
    *verboseOutput << "        tripod = " << stabConf->mdConf.virtualTripod << endl;
    *verboseOutput << "          show = " << stabConf->mdConf.show << endl;
    *verboseOutput << "        result = " << stabConf->stabStateFile->fileName() << endl;


    //f = fopen(stabStateFile->fileName().toStdString(), "w");
    f = stabConf->openStabStateFile("w");
    if (vsPrepareFile(&md, f) != VS_OK) {
      throw runtime_error(QString("cannot write to transform file %1").arg(stabConf->stabStateFile->fileName()).toStdString());
    }

    initialized = true;
  }

  PipelineStabTransform::PipelineStabTransform(StabConfig *stabConf, QTextStream *verboseOutput, QTextStream *err) :
  stabConf(stabConf),
  initialized(false), width(-1), height(-1), f(NULL),
  verboseOutput(verboseOutput), err(err) {


    memset(&fi, 0, sizeof (VSFrameInfo));
    memset(&td, 0, sizeof (VSTransformData));
    memset(&trans, 0, sizeof (VSTransformations));


  }

  PipelineStabTransform::~PipelineStabTransform() {
  }

  void PipelineStabTransform::onLast() {
    if (initialized) {
      // cleanup transformation
      vsTransformDataCleanup(&td);
      vsTransformationsCleanup(&trans);
    }
    emit last();
  }

  void PipelineStabTransform::onInput2(InputImageInfo info, Magick::Image image) {
    try {
      if (!initialized) {
        init(image);
      }
      if (image.rows() != height || image.columns() != width) {
        throw runtime_error(QString("Not uniform image size! %").arg(info.fileInfo().fileName()).toStdString());
      }

      Q_ASSERT(image.baseColumns() == width && image.baseRows() == height);
      Magick::Blob blob;
      // set raw RGBS output format & convert it into a Blob 
      if (image.depth() > 8)
        *err << "Warning: we lost some information by converting to 8bit depth (now " << image.depth() << ")" << endl;
      image.depth(8);
      image.magick("RGB");
      image.write(&blob);


      Q_ASSERT(blob.length() == image.baseColumns() * image.baseRows() * 3);

      // inframe
      VSFrame inframe;
      size_t dataLen = blob.length();
      inframe.data[0] = (uint8_t*) blob.data();
      inframe.linesize[0] = image.baseColumns() * 3; // TODO: it is correct?

      // outframe
      uint8_t* data = new uint8_t[dataLen];
      //memcpy(data, blob.data(), dataLen);
      VSFrame outframe;
      outframe.data[0] = data;
      outframe.linesize[0] = image.baseColumns() * 3; // TODO: it is correct?

      if (vsTransformPrepare(&td, &inframe, &outframe) != VS_OK) {
        throw runtime_error("Failed to prepare transform");
      }
      Q_ASSERT(vsTransformGetSrcFrameInfo(&td)->planes == 1);

      vsDoTransform(&td, vsGetNextTransform(&td, &trans));

      vsTransformFinish(&td);

      Magick::Geometry g(width, height);
      Magick::Blob oblob(data, dataLen);

      Magick::Image oimage;
      oimage.size(g);
      oimage.depth(8);
      oimage.magick("RGB");
      oimage.read(oblob);

      delete[] data;

      info.luminance = -1;
      emit input(info, oimage);

    } catch (exception &e) {
      emit error(e.what());
    }
  }

  void PipelineStabTransform::init(Magick::Image img) {
    width = img.columns();
    height = img.rows();


    if (!vsFrameInfoInit(&fi, width, height, PF_RGB24)) {
      throw runtime_error("Failed to initialize frame format");
    }
    fi.planes = 1; // I don't understand vs frame info... But later is assert for planes == 1

    if (vsTransformDataInit(&td, &stabConf->tsConf, &fi, &fi) != VS_OK) {
      throw runtime_error("initialization of vid.stab transform failed, please report a BUG");
    }
    vsTransformGetConfig(&stabConf->tsConf, &td);

    *verboseOutput << "Video transformation/stabilization settings (pass 2/2):" << endl;
    *verboseOutput << "    input     = " << stabConf->stabStateFile->fileName() << endl;
    *verboseOutput << "    smoothing = " << stabConf->tsConf.smoothing << endl;
    *verboseOutput << "    optalgo   = " <<
      (stabConf->tsConf.camPathAlgo == VSOptimalL1 ? "opt" :
      (stabConf->tsConf.camPathAlgo == VSGaussian ? "gauss" : "avg")) << endl;
    *verboseOutput << "    maxshift  = " << stabConf->tsConf.maxShift << endl;
    *verboseOutput << "    maxangle  = " << stabConf->tsConf.maxAngle << endl;
    *verboseOutput << "    crop      = " << (stabConf->tsConf.crop ? "Black" : "Keep") << endl;
    *verboseOutput << "    relative  = " << (stabConf->tsConf.relative ? "True" : "False") << endl;
    *verboseOutput << "    invert    = " << (stabConf->tsConf.invert ? "True" : "False") << endl;
    *verboseOutput << "    zoom      = " << (stabConf->tsConf.zoom) << endl;
    *verboseOutput << "    optzoom   = " << (
      stabConf->tsConf.optZoom == 1 ? "Static (1)" : (stabConf->tsConf.optZoom == 2 ? "Dynamic (2)" : "Off (0)")) << endl;
    if (stabConf->tsConf.optZoom == 2)
      *verboseOutput << "    zoomspeed = " << stabConf->tsConf.zoomSpeed << endl;
    *verboseOutput << "    interpol  = " << getInterpolationTypeName(stabConf->tsConf.interpolType) << endl;

    //f = fopen(stabStateFile->fileName().toStdString(), "r");
    f = stabConf->openStabStateFile("r");
    VSManyLocalMotions mlms;
    if (vsReadLocalMotionsFile(f, &mlms) == VS_OK) {
      // calculate the actual transforms from the local motions
      if (vsLocalmotions2Transforms(&td, &mlms, &trans) != VS_OK) {
        throw runtime_error("calculating transformations failed");
      }
    } else { // try to read old format
      if (!vsReadOldTransforms(&td, f, &trans)) { /* read input file */
        throw runtime_error(QString("error parsing input file %1").arg(stabConf->stabStateFile->fileName()).toStdString());
      }
    }

    fclose(f);
    f = NULL;

    if (vsPreprocessTransforms(&td, &trans) != VS_OK) {
      throw runtime_error("error while preprocessing transforms");
    }
    initialized = true;
  }
}
