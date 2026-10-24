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

#if MagickLibVersion < 0x700
using ResizeFilterType = Magick::FilterTypes;
#else
using ResizeFilterType = Magick::FilterType;
#endif


  class TIME_LAPSE_API ResizeFrame : public ImageHandler {
    Q_OBJECT
  public:
    /**
     *
     * @param verboseOutput stream to write verbose output to
     * @param w target width
     * @param h target height
     * @param adaptiveResize whether to use adaptive resize - fast interpolative resize using mesh interpolation
     *        see ImageMagick documentation for details. It works well for small resizes of less than +/- 50% of the original image size.
     * @param filterType filter type to use for resize. If adaptiveResize is true, this parameter is ignored.
     *        By default, ImageMagick decides what filter to use based on the original and target image size.
     */
    ResizeFrame(QTextStream *verboseOutput,
                int w, int h,
                bool adaptiveResize = true,
                ResizeFilterType filterType = ResizeFilterType::UndefinedFilter);

  public slots:
    virtual void onInputImg(InputImageInfo info, Magick::Image img) override;

  private:
    QTextStream *verboseOutput;
    int width;
    int height;
    bool adaptiveResize;
    ResizeFilterType filterType;
  };

}
