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

#include <QtCore/QFileInfo>
#include <QtCore/QObject>
#include <QtCore/QDebug>
#include <QtCore/QTimer>
#include <QtCore/QTextStream>
#include <QtCore/QCoreApplication>
#include <QtCore/QFileInfo>
#include <QtCore/QTemporaryDir>
#include <QtCore/QString>
#include <QtCore/QDir>
#include <QtCore/QProcess>

#include <exception>

#include "pipeline_frame_prepare.h"
#include "pipeline_frame_prepare.moc"

using namespace std;
using namespace timelapse;


namespace timelapse {

  FramePrepare::FramePrepare(QTextStream *_verboseOutput) :
  verboseOutput(_verboseOutput), prevImage(NULL), prevInfo() {
  }

  void FramePrepare::blend(InputImageInfo info1, const Magick::Image *img1, InputImageInfo info2, const Magick::Image *img2) {
    for (int f = info1.frame; f < info2.frame; f++) {
      InputImageInfo i = info1;
      i.frame = f;
      emit input(i, *img1);
    }
  }

  void FramePrepare::onInput(InputImageInfo info, Magick::Image img) {
    if (prevImage != NULL) {
      blend(prevInfo, prevImage, info, &img);
      delete prevImage;
    }
    prevImage = new Magick::Image(img);
    prevInfo = info;
  }

  void FramePrepare::onLast() {
    if (prevImage != NULL) {
      blend(prevInfo, prevImage, prevInfo, NULL);
      delete prevImage;
    }
    emit last();
  }

  BlendFramePrepare::BlendFramePrepare(QTextStream *verboseOutput) :
  FramePrepare(verboseOutput) {

  }

  void BlendFramePrepare::blend(InputImageInfo info1, const Magick::Image *img1, InputImageInfo info2, const Magick::Image *img2) {
    int f1 = info1.frame;
    int f2 = info2.frame;
    *verboseOutput << "Blending images for frames " << f1 << " ... " << f2 << endl;
    for (int f = f1; f < f2; f++) {
      Magick::Image blended = *img1;

      double opacity = 1.0 - ((double) (f - f1) / ((double) (f2 - f1)));
      if (f - f1 > 0) { // for 100 % transparency, we don't have to composite
        Magick::Image secondLayer = *img2;
        *verboseOutput << "Blend with next image with " << (opacity * 100) << " % transparency" << endl;
        secondLayer.opacity(((double) QuantumRange) * opacity);
        blended.composite(secondLayer, 0, 0, Magick::DissolveCompositeOp);
      }
      //writeFrame(f, blended);      
      InputImageInfo i = info1;
      i.frame = f;
      emit input(i, blended);
    }
  }

}
