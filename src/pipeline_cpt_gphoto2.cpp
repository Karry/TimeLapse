/*
 *   Copyright (C) 2016 Lukáš Karas <lukas.karas@centrum.cz>
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

#include "pipeline_cpt_gphoto2.h"
#include "pipeline_cpt_gphoto2.moc"


using namespace std;
using namespace timelapse;

namespace timelapse {

  Gphoto2Device::Gphoto2Device(QString dev) {
  }

  Gphoto2Device::Gphoto2Device(const timelapse::Gphoto2Device& other) {
  }

  Gphoto2Device::~Gphoto2Device() {
  }

  Magick::Image Gphoto2Device::capture() {
    return Magick::Image();
  }

  QString Gphoto2Device::toString() {
    return "";
  }

  Gphoto2Device Gphoto2Device::operator=(const timelapse::Gphoto2Device&) {
    return Gphoto2Device();
  }

}
