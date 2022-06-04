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

#include <TimeLapse/timelapse.h>
#include <TimeLapse/input_image_info.h>
#include <TimeLapse/pipeline_handler.h>

#include <Magick++.h>

#include <QtCore/QObject>
#include <QtCore/QDebug>
#include <QtCore/QTemporaryDir>

namespace timelapse {

  class TIME_LAPSE_API FramePrepare : public ImageHandler {
    Q_OBJECT

  public:
    explicit FramePrepare(QTextStream *verboseOutput);
    virtual void blend(InputImageInfo info1, const Magick::Image *img1, InputImageInfo info2, const Magick::Image *img2);

  public slots:
    void onInputImg(InputImageInfo info, Magick::Image img) override;
    void onLast() override;

  protected:
    QTextStream * verboseOutput;

  private:
    Magick::Image * prevImage;
    InputImageInfo prevInfo;
  };

  class TIME_LAPSE_API BlendFramePrepare : public FramePrepare {
    Q_OBJECT
  public:
    explicit BlendFramePrepare(QTextStream *verboseOutput);
    virtual void blend(InputImageInfo info1, const Magick::Image *img1, InputImageInfo info2, const Magick::Image *img2) override;
  };

}