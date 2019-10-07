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

#pragma once

#include <QtCore/QObject>
#include <QtCore/QDateTime>
#include <QtCore/QFileInfo>

#include <Magick++.h>
#include <vector>

struct InputImageInfo {
public:
  InputImageInfo() = default;
  InputImageInfo(const InputImageInfo &) = default;
  InputImageInfo(InputImageInfo &&) = default;
  InputImageInfo &operator=(const InputImageInfo &) = default;
  InputImageInfo &operator=(InputImageInfo &&) = default;

  explicit InputImageInfo(const QFileInfo &f);

  QFileInfo fileInfo() const;

public:
  std::string filePath;
  int width{-1};
  int height{-1};
  int frame{-1};
  QDateTime timestamp;
  double luminance{-1};
  double luminanceChange{0};
};
