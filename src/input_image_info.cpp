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

#include "input_image_info.h"
#include "input_image_info.moc"

InputImageInfo::InputImageInfo() :
file(), width(-1), height(-1), frame(-1), timestamp() {
}

InputImageInfo::InputImageInfo(QFileInfo &f) : file(f), width(-1), height(-1), frame(-1), timestamp() {
}

InputImageInfo& InputImageInfo::operator=(const InputImageInfo& i) {
  file = i.file;
  width = i.width;
  height = i.height;
  frame = i.frame;
  timestamp = i.timestamp;
  return *this;
}

InputImageInfo::InputImageInfo(const InputImageInfo& i) :
file(i.file), width(i.width), height(i.height), frame(i.frame), timestamp(i.timestamp) {
}

InputImageInfo::~InputImageInfo() {
}

