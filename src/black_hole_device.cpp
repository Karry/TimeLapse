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

#include <TimeLapse/black_hole_device.h>

namespace timelapse {

  BlackHoleDevice::BlackHoleDevice() : QIODevice() {
    open(WriteOnly);
  }

  BlackHoleDevice::~BlackHoleDevice() {
  }

  qint64 BlackHoleDevice::readData([[maybe_unused]] char *data, [[maybe_unused]] qint64 maxlen) {
    return -1;
  }

  qint64 BlackHoleDevice::writeData([[maybe_unused]] const char *data, qint64 len) {
    return len;
  }
}
