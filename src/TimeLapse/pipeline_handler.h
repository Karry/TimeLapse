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

#include <Magick++.h>

#include <QtCore/QObject>
#include <QtCore/QDebug>
#include <QtCore/QTemporaryDir>

namespace timelapse {

constexpr int FRAME_FILE_LEADING_ZEROS = 9;

  class TIME_LAPSE_API PipelineHandler : public QObject {
    Q_OBJECT
  public:
  public slots:
    virtual void onLast();
  signals:
    void last();
    void error(QString msg);
  };

  class TIME_LAPSE_API InputHandler : public PipelineHandler {
    Q_OBJECT
  public:

  public slots:
    virtual void onInput(InputImageInfo info) = 0;
  signals:
    void input(InputImageInfo info);
  };

  class TIME_LAPSE_API ImageHandler : public PipelineHandler {
    Q_OBJECT
  public:
  public slots:
    virtual void onInputImg(InputImageInfo info, Magick::Image img) = 0;
  signals:
    void inputImg(InputImageInfo info, Magick::Image img);
  };

  class TIME_LAPSE_API ImageLoader : public ImageHandler {
    Q_OBJECT
  signals:
    void onImageLoaded(int stage, int cnt);
  public:
    ImageLoader(QTextStream *verboseOutput, QTextStream *err, int stage);
  public slots:
    virtual void onInputImg(InputImageInfo info, Magick::Image img) override;
    virtual void onInput(InputImageInfo info);
  private:
    QTextStream *verboseOutput;
    QTextStream *err;
    int stage;
    int cnt=0;
  };

  class TIME_LAPSE_API ImageTrash : public InputHandler {
    Q_OBJECT
  public:
  public slots:
    virtual void onInput(InputImageInfo info) override;
    virtual void onInputImg(InputImageInfo info, Magick::Image img);
  };

  class TIME_LAPSE_API StageSeparator : public InputHandler {
    Q_OBJECT
  public:
  public slots:
    virtual void onInput(InputImageInfo info) override;
    virtual void onLast() override;
  protected:
    QList<InputImageInfo> inputs;

  };

  class TIME_LAPSE_API ImageMetadataReader : public ImageHandler {
    Q_OBJECT
  public:
    ImageMetadataReader(QTextStream *verboseOutput, QTextStream *err);
  public slots:
    virtual void onInputImg(InputImageInfo info, Magick::Image img) override;
  private:
    QTextStream *verboseOutput;
    QTextStream *err;
  };

}
