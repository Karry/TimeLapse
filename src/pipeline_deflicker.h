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
#include <QtCore/QDebug>
#include <QtCore/QTemporaryDir>

#include <Magick++.h>

#include "input_image_info.h"
#include "pipeline_handler.h"

namespace timelapse {

  class ComputeLuminance : public ImageHandler {
    Q_OBJECT
  public:
    ComputeLuminance(QTextStream *verboseOutput);

    static double computeLuminance(
            const std::vector<std::pair<Magick::Color, size_t>> &histogram,
            double gamma = 1.0);

  public slots:
    virtual void onInput2(InputImageInfo info, Magick::Image img);
  private:
    QTextStream *verboseOutput;
  };

  /**
   * Compute target luminance by average from all images
   */
  class ComputeAverageLuminance : public InputHandler {
    Q_OBJECT
  public:
    ComputeAverageLuminance(QTextStream *verboseOutput);
  public slots:
    virtual void onInput1(InputImageInfo info) override;
    void onLast() override;
  private:
    QTextStream *verboseOutput;
    QList<InputImageInfo> inputs;
  };

  /**
   * Compute target luminance by weighted moving average
   */
  class WMALuminance : public InputHandler {
    Q_OBJECT
  public:
    WMALuminance(QTextStream *verboseOutput, size_t count);
  public slots:
    virtual void onInput1(InputImageInfo info) override;
    void onLast() override;
  private:
    size_t count;
    QTextStream *verboseOutput;
    QList<InputImageInfo> inputs;
  };

  class AdjustLuminance : public ImageHandler {
    Q_OBJECT
  public:
    AdjustLuminance(QTextStream *verboseOutput, bool debugView);
  public slots:
    virtual void onInput2(InputImageInfo info, Magick::Image img);
  private:
    QTextStream *verboseOutput;
    bool debugView;
  };

}
