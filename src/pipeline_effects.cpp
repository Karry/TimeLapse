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

#include "pipeline_effects.h"
#include "pipeline_effects.moc"

using namespace std;
using namespace timelapse;

namespace timelapse {

  PipelineEdgeEffect::PipelineEdgeEffect(double radius) : radius(radius) {
  }

  PipelineEdgeEffect::~PipelineEdgeEffect() {
  }

  void PipelineEdgeEffect::onInput(InputImageInfo info, Magick::Image img) {
    img.edge(radius);
    //img.cannyEdge(radius);
    info.luminance = -1;
    emit input(info, img);
  }

}
