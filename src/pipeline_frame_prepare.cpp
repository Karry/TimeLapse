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

#include <TimeLapse/pipeline_frame_prepare.h>

#include <TimeLapse/timelapse.h>

#include <QtCore/QTextStream>

#include <Magick++.h>
#include <exception>
#include <ImageMagick-6/Magick++/Color.h>

using namespace std;
using namespace timelapse;


namespace timelapse {

  FramePrepare::FramePrepare(QTextStream *_verboseOutput) :
  verboseOutput(_verboseOutput), prevImage(nullptr), prevInfo() {
  }

  void FramePrepare::blend(InputImageInfo info1, const Magick::Image *img1, InputImageInfo info2, [[maybe_unused]] const Magick::Image *img2) {
    for (int f = info1.frame; f < info2.frame; f++) {
      InputImageInfo i = info1;
      i.frame = f;
      emit inputImg(i, *img1);
    }
  }

  void FramePrepare::onInputImg(InputImageInfo info, Magick::Image img) {
    if (prevImage != nullptr) {
      blend(prevInfo, prevImage, info, &img);
      delete prevImage;
    }
    prevImage = new Magick::Image(img);
    prevInfo = info;
  }

  void FramePrepare::onLast() {
    if (prevImage != nullptr) {
      blend(prevInfo, prevImage, prevInfo, nullptr);
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
        
        //secondLayer.opacity(((double) QuantumRange) * opacity);
        secondLayer.opacity(Magick::Color::scaleDoubleToQuantum(opacity));
        blended.composite(secondLayer, 0, 0, Magick::DissolveCompositeOp);
      }
      //writeFrame(f, blended);      
      InputImageInfo i = info1;
      i.frame = f;
      emit inputImg(i, blended);
    }
  }

}
