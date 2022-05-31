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

#include <QtCore/QObject>
#include <QtCore/QDebug>
#include <QtCore/QTemporaryDir>

#include <Magick++.h>

namespace timelapse {

  class TIME_LAPSE_API OneToOneFrameMapping : public InputHandler {
    Q_OBJECT
  public:
    OneToOneFrameMapping();
  public slots:
    virtual void onInput(InputImageInfo info) override;
  private:
    int frame;
  };

  class ConstIntervalFrameMapping : public InputHandler {
    Q_OBJECT
  public:
    ConstIntervalFrameMapping(QTextStream *verboseOutput, QTextStream *err, float length, float fps);
  public slots:
    virtual void onInput(InputImageInfo info) override;
    virtual void onLast() override;
  protected:
    int frame;
    QTextStream *verboseOutput;
    QTextStream *err;
    float length;
    float fps;
    int frameCount;
    QList<InputImageInfo> inputs;
  };

  class VariableIntervalFrameMapping : public ConstIntervalFrameMapping {
    Q_OBJECT
  public:
    VariableIntervalFrameMapping(QTextStream *verboseOutput, QTextStream *err, float length, float fps);
  public slots:
    virtual void onLast();
  };


}
