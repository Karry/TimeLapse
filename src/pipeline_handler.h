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

#include "input_image_info.h"

#include <Magick++.h>

#include <QtCore/QObject>
#include <QtCore/QDebug>
#include <QtCore/QTemporaryDir>

namespace timelapse {
constexpr int FRAME_FILE_LEADING_ZEROS = 9;

  class PipelineHandler : public QObject {
    Q_OBJECT
  public:
  public slots:
    virtual void onLast();
  signals:
    void last();
    void error(QString msg);
  };

  class InputHandler : public PipelineHandler {
    Q_OBJECT
  public:

  public slots:
    virtual void onInput1(InputImageInfo info) = 0;
  signals:
    void input(InputImageInfo info);
  };

  class ImageHandler : public PipelineHandler {
    Q_OBJECT
  public:
  public slots:
    virtual void onInput2(InputImageInfo info, Magick::Image img) = 0;
  signals:
    void input(InputImageInfo info, Magick::Image img);
  };

  class ImageLoader : public ImageHandler {
    Q_OBJECT
  public:
    ImageLoader(QTextStream *verboseOutput, QTextStream *err);
  public slots:
    virtual void onInput2(InputImageInfo info, Magick::Image img) override;
    virtual void onInput1(InputImageInfo info);
  private:
    QTextStream *verboseOutput;
    QTextStream *err;
  };

  class ImageTrash : public InputHandler {
    Q_OBJECT
  public:
  public slots:
    virtual void onInput1(InputImageInfo info) override;
    virtual void onInput2(InputImageInfo info, Magick::Image img);
  signals:
    void input(InputImageInfo info);
  };

  class StageSeparator : public InputHandler {
    Q_OBJECT
  public:
  public slots:
    virtual void onInput1(InputImageInfo info) override;
    virtual void onLast() override;
  protected:
    QList<InputImageInfo> inputs;

  };

  class ImageMetadataReader : public ImageHandler {
    Q_OBJECT
  public:
    ImageMetadataReader(QTextStream *verboseOutput, QTextStream *err);
  public slots:
    virtual void onInput2(InputImageInfo info, Magick::Image img) override;
  private:
    QTextStream *verboseOutput;
    QTextStream *err;
  };

}
