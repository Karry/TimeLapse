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

#include <TimeLapse/pipeline_frame_mapping.h>

#include <TimeLapse/timelapse.h>

#include <QtCore/QObject>
#include <QtCore/QTextStream>
#include <QtCore/QString>

using namespace std;
using namespace timelapse;


namespace timelapse {

  OneToOneFrameMapping::OneToOneFrameMapping() : frame(0) {

  }

  void OneToOneFrameMapping::onInput(InputImageInfo info) {
    info.frame = frame;
    emit input(info);
    frame++;
  }

  ConstIntervalFrameMapping::ConstIntervalFrameMapping(
    QTextStream *_verboseOutput, QTextStream *_err, float _length, float _fps) :
    verboseOutput(_verboseOutput), err(_err), length(_length), fps(_fps) {

    frameCount = length * fps;
    if (frameCount <= 0) {
      throw invalid_argument("Frame count have to be positive!");
    }
  }

  void ConstIntervalFrameMapping::onInput(InputImageInfo info) {
    inputs.append(info);
  }

  void ConstIntervalFrameMapping::onLast() {
    if (frameCount % inputs.size() != 0) {
      *err << "Count of video frames is not integer multiple of count of input images." << endl;
    }
    if (frameCount < inputs.size()) {
      *err << "Count of video frames (" << frameCount << ") is less than input images "
        "(" << inputs.size() << "). Some images will be skipped!" << endl;
    }

    double frameStep = ((double) frameCount) / ((double) inputs.size());
    if (frameStep <= 0) {
      throw invalid_argument("Frame step between images is too low!");
    }
    double currentFrame = 0;
    int lastFrame = -1;
    int emited = 0;
    for (InputImageInfo inf : inputs) {
      inf.frame = currentFrame;
      if (inf.frame > lastFrame) {
        *verboseOutput << "Mapping " << inf.fileInfo().fileName() << " to frame " << inf.frame
          << " (step " << (lastFrame < 0 ? 0 : inf.frame - lastFrame) << ")" << endl;
        //inputList.append(input);
        emit input(inf);
        emited++;
      } else {
        *verboseOutput << "Skip " << inf.fileInfo().fileName() << endl;
      }
      lastFrame = inf.frame;
      currentFrame += frameStep;
    }
    if (emited <= 0) {
      emit error("No images after mapping!"); 
    }
    *verboseOutput << "Sum of frames: " << frameCount << endl;

    emit last();
  }

  VariableIntervalFrameMapping::VariableIntervalFrameMapping(
    QTextStream *verboseOutput, QTextStream *err, float length, float fps) :
  ConstIntervalFrameMapping(verboseOutput, err, length, fps) {
  }

  void VariableIntervalFrameMapping::onLast() {
    if (frameCount < inputs.size()) {
      *err << "Count of video frames (" << frameCount << ") is less than input images "
        "(" << inputs.size() << "). Some images will be skipped!" << endl;
    }

    QDateTime startTimestamp;
    QDateTime stopTimestamp;
    QDateTime lastTimestamp;
    bool first = true;
    QList<InputImageInfo> inputList;
    for (InputImageInfo inf : inputs) {
      //Magick::Image image(input.file.filePath().toStdString());

      if (!first && lastTimestamp > inf.timestamp) {
        *err << "Images are not sorted by its time! Skip " << inf.fileInfo().fileName() << endl;
      } else {
        inputList.append(inf);
        if (first || inf.timestamp < startTimestamp)
          startTimestamp = inf.timestamp;
        if (first || inf.timestamp > stopTimestamp)
          stopTimestamp = inf.timestamp;
        lastTimestamp = inf.timestamp;
      }
      first = false;
    }
    inputs = inputList;

    qint64 realTimeLapsDurationMs = startTimestamp.msecsTo(stopTimestamp);
    if (realTimeLapsDurationMs <= 0) {
      emit error("All images has equal timestamp!");
      return;
    }

    *verboseOutput << "Real time-laps duration: " << realTimeLapsDurationMs << " ms" << endl;
    *verboseOutput << "Sum of frames: " << frameCount << endl;

    //inputList = QList<InputImageInfo>();
    int emited = 0;
    int lastFrame = -1;
    int maxStep = 0;
    for (InputImageInfo inf : inputs) {
      inf.frame = (
        ((double) startTimestamp.msecsTo(inf.timestamp)) / ((double) realTimeLapsDurationMs)
        * ((double) frameCount));

      if (inf.frame >= frameCount) // last frame
        inf.frame = frameCount - 1;

      int step = (lastFrame < 0 ? 0 : inf.frame - lastFrame);
      *verboseOutput << inf.fileInfo().fileName() << " " << inf.timestamp.toString(Qt::ISODate)
        << " > frame " << inf.frame << " (step " << step << ")" << endl;
      if (step > maxStep)
        maxStep = step;

      if (lastFrame >= inf.frame) {
        *err << "Skip image " << inf.fileInfo().fileName() << ", it is too early after previous." << endl
          << "You can try to increase output length or fps." << endl;
      } else {
        //inputList.append(input);
        emit input(inf);
        emited++;
        lastFrame = inf.frame;
      }
    }
    inputs = inputList;
    *verboseOutput << "Maximum step is " << maxStep << " frames." << endl;

    if (emited <= 0) {
      emit error( "No images after frame mapping!");
      return;
    }
    emit last();
  }


}


