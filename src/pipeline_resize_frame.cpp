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

#include <TimeLapse/pipeline_resize_frame.h>

#include <TimeLapse/scope_log.h>

using namespace std;
using namespace timelapse;

namespace timelapse {

  ResizeFrame::ResizeFrame(QTextStream *verboseOutput, int w, int h, bool adaptiveResize) :
    verboseOutput{verboseOutput}, width(w), height(h), adaptiveResize(adaptiveResize) {
  }

  void ResizeFrame::onInputImg(InputImageInfo info, Magick::Image img) {
    Magick::Image resized = img;
    {
      ScopeLogger resizeLogger(verboseOutput, QString("Resizing image %1 x %2 to %3 x %4")
                                 .arg(resized.columns()).arg(resized.rows())
                                 .arg(width).arg(height));
      Magick::Geometry g(width, height);
      g.aspect(true);

      if (adaptiveResize) {
        resized.adaptiveResize(g);
      } else {
        resized.resize(g);
      }
    }
    emit inputImg(info, resized);
  }

}
