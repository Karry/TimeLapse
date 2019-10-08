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

#include "pipeline_resize_frame.h"
#include "pipeline_resize_frame.moc"

#include <QtCore/QCoreApplication>

using namespace std;
using namespace timelapse;


namespace timelapse {

  ResizeFrame::ResizeFrame(int w, int h) : width(w), height(h) {
  }

  void ResizeFrame::onInput2(InputImageInfo info, Magick::Image img) {
    Magick::Image resized = img;
    Magick::Geometry g(width, height);
    g.aspect(true);
    resized.resize(g);
    emit input(info, resized);
  }

}
