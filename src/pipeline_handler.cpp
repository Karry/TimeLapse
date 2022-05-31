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

#include <TimeLapse/pipeline_handler.h>

#include <TimeLapse/scope_log.h>
#include <TimeLapse/timelapse.h>

#include <ImageMagick-6/Magick++/Exception.h>

#include <QtCore/QTextStream>
#include <QtCore/QCoreApplication>
#include <QtCore/QString>

using namespace std;
using namespace timelapse;


namespace timelapse {

  void PipelineHandler::onLast() {
    //QTextStream(stdout) << "Emitting last from " << this->metaObject()->className() << endl;
    emit last();
  }

  void ImageLoader::onInputImg(InputImageInfo info, [[maybe_unused]] Magick::Image img) {
    // load image again
    onInput(info);
  }

  ImageLoader::ImageLoader(QTextStream *_verboseOutput, QTextStream *_err) :
  verboseOutput(_verboseOutput), err(_err) {
  }

  void ImageLoader::onInput(InputImageInfo info) {
    Magick::Image image;
    bool usableImage = false;
    try {
      ScopeLogger loadLogger(verboseOutput, QString("Loading %1").arg(info.fileInfo().filePath()));
      image.read(info.filePath);
      usableImage = true;
    } catch (Magick::WarningCoder &warning) {
      // Process coder warning while loading
      *err << "Coder Warning: " << QString::fromUtf8(warning.what()) << endl;
      usableImage = true;
    } catch (Magick::Warning &warning) {
      // Handle any other Magick++ warning. 
      *err << "Warning: " << QString::fromUtf8(warning.what()) << endl;
      usableImage = true;
    } catch (Magick::Error &e) {
      // Process other Magick++ 
      emit error(QString("Failed to load file as image (%1). Reason: %2")
        .arg(info.fileInfo().filePath())
        .arg(e.what()));
    }
    if (usableImage) {
      emit inputImg(info, image);
    }
  }

  void ImageTrash::onInput(InputImageInfo info) {
    emit input(info);
  }

  void ImageTrash::onInputImg(InputImageInfo info, [[maybe_unused]] Magick::Image img) {
    emit input(info);
  }

  void StageSeparator::onInput(InputImageInfo info) {
    inputs.append(info);
  }

  void StageSeparator::onLast() {
    for (InputImageInfo inf : inputs) {
      emit input(inf);
    }
    emit last();
  }

  ImageMetadataReader::ImageMetadataReader(QTextStream *_verboseOutput, QTextStream *_err) :
  verboseOutput(_verboseOutput), err(_err) {
  }

  void ImageMetadataReader::onInputImg(InputImageInfo info, Magick::Image image) {
    // read dimensions
    info.width = image.baseColumns();
    info.height = image.baseRows();

    // read timestamp
    QString exifDateTime = QString::fromStdString(image.attribute("EXIF:DateTime"));
    if (exifDateTime.length() == 0) {
      *err << "Image " << info.fileInfo().fileName() << " don't have EXIF:DateTime property. Using file modification time." << endl;
      info.timestamp = info.fileInfo().lastModified();
    } else {
      // 2015:09:15 07:00:28
      info.timestamp = QDateTime::fromString(exifDateTime, QString("yyyy:MM:dd HH:mm:ss"));
    }
    *verboseOutput << info.fileInfo().fileName() << " EXIF:DateTime : " << exifDateTime
      << " (" << info.timestamp.toString(Qt::ISODate) << ")" << endl;

    emit inputImg(info, image);
  }


}
