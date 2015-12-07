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

#include "pipeline_stab.h"
#include "pipeline_stab.moc"
#include "libvidstab.h"
#include "timelapse_stabilize.h"

using namespace std;
using namespace timelapse;


namespace timelapse {

  void stabInit() {

    VS_ERROR = 0;
    VS_OK = 1;

  }

  StabConfig::StabConfig() :
  stabStateFile(NULL), dryRun(false) {

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
    mdConf.shakiness = 1;
    mdConf.accuracy = 15;
    mdConf.stepSize = 6;
    mdConf.contrastThreshold = 0.3;
    mdConf.show = 0;
    //mdConf.numThreads = 1;

    // set values that are not initializes by the options
    // https://github.com/georgmartius/vid.stab
    tsConf.modName = "vidstabtransform";
    tsConf.verbose = 1;
    tsConf.smoothing = 0; // 0 is a special case where a static camera is simulated.
    tsConf.camPathAlgo = VSGaussian; // gauss: Gaussian kernel low-pass filter on camera motion (default). 
    tsConf.maxShift = -1; // Set maximal number of pixels to translate frames. Default value is -1, meaning: no limit.
    tsConf.maxAngle = -1; //Set maximal angle in radians (degree*PI/180) to rotate frames. Default value is -1, meaning: no limit.
    tsConf.crop = VSKeepBorder; // Keep image information from previous frame (default). 
    tsConf.simpleMotionCalculation = 0;
    tsConf.storeTransforms = 0;
    tsConf.smoothZoom = 0;
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

  void PipelineStabDetect::onInput(InputImageInfo info, Magick::Image image) {
    try {
      if (!initialized) {
        init(image);
      }
      if (image.rows() != height || image.columns() != width) {
        throw runtime_error(QString("Not uniform image size! %").arg(info.file.fileName()).toStdString());
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

      Q_ASSERT(fi.planes == 1);
      Q_ASSERT(blob.length() == image.baseColumns() * image.baseRows() * 3);

      frame.data[0] = (uint8_t*) blob.data(); // TODO: image data?
      frame.linesize[0] = image.baseColumns() * 3; // TODO: it is correct?

      if (vsMotionDetection(&md, &localmotions, &frame) != VS_OK) {
        throw runtime_error("motion detection failed");
      } else {
        if (vsWriteToFile(&md, f, &localmotions) != VS_OK) {
          throw runtime_error("cannot write to transform file");
        }
        vs_vector_del(&localmotions);
      }
      emit input(info, image);

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

  void PipelineStabTransform::onInput(InputImageInfo info, Magick::Image image) {
    try {
      if (!initialized) {
        init(image);
      }
      if (image.rows() != height || image.columns() != width) {
        throw runtime_error(QString("Not uniform image size! %").arg(info.file.fileName()).toStdString());
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
