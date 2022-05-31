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

#include "timelapse.h"
#include "pipeline.h"
#include "pipeline_handler.h"
#include "pipeline_cpt.h"

#include <exception>

namespace timelapse {

  Pipeline* Pipeline::createWithCaptureSource(QSharedPointer<CaptureDevice> dev, int64_t interval, int32_t cnt,
    QTextStream *verboseOutput, QTextStream *err) {

    PipelineCaptureSource *src = new PipelineCaptureSource(dev, interval, cnt, verboseOutput, err);
    return new Pipeline(src, src, verboseOutput, err);
  }

  Pipeline* Pipeline::createWithFileSource(QStringList inputArguments, bool recursive,
    QTextStream *verboseOutput, QTextStream *err) {

    PipelineFileSource *src = new PipelineFileSource(inputArguments, recursive, verboseOutput, err);
    return new Pipeline(src, src, verboseOutput, err);
  }

  Pipeline::Pipeline(PipelineSource *src, InputHandler *firstInputHandler,
    QTextStream *verboseOutput, QTextStream *err) :
  verboseOutput(verboseOutput), err(err),
  elements(), src(src), lastInputHandler(firstInputHandler), lastImageHandler(nullptr) {

    append(firstInputHandler);
  }

  Pipeline::Pipeline(PipelineSource *src, ImageHandler *firstImageHandler,
    QTextStream *verboseOutput, QTextStream *err) :
  verboseOutput(verboseOutput), err(err),
  elements(), src(src), lastInputHandler(nullptr), lastImageHandler(firstImageHandler) {

    append(firstImageHandler);
  }

  Pipeline::~Pipeline() {
    for (QObject *el : elements) {
      delete el;
    }
  }

  void Pipeline::handlerFinished() {
    QObject *sender = QObject::sender();
    if (sender != nullptr)
      *verboseOutput << "Pipeline handler " << sender->metaObject()->className() << " finished" << endl;
    else
      *verboseOutput << "handlerFinished called directly!" << endl;
  }

  void Pipeline::onError(QString msg) {
    QObject *sender = QObject::sender();
    if (sender != nullptr) {
      *verboseOutput << "Error in pipeline handler " << sender->metaObject()->className() << " :" << msg << endl;
    } else {
      *verboseOutput << "onError slot called directly!" << endl;
    }
    *err << msg << endl;
  }

  void Pipeline::append(PipelineHandler *handler) {
    *verboseOutput << "Pipeline append " << handler->metaObject()->className() << "" << endl;
    connect(handler, &PipelineHandler::last, this, &Pipeline::handlerFinished);
    connect(handler, &PipelineHandler::error, this, &Pipeline::onError);
    connect(handler, &PipelineHandler::error, this, &Pipeline::error);
    elements.append(handler);
  }

  void Pipeline::operator<<(ImageHandler *handler) {

    if (lastInputHandler != nullptr) {
      ImageLoader *loader = new ImageLoader(verboseOutput, err);
      connect(lastInputHandler, &InputHandler::input, loader, &ImageLoader::onInput);
      connect(lastInputHandler, &InputHandler::last, loader, &ImageLoader::onLast);
      append(loader);

      lastInputHandler = nullptr;
      lastImageHandler = loader;
    }

    if (lastImageHandler != nullptr) {

      connect(lastImageHandler, &ImageHandler::inputImg, handler, &ImageHandler::onInputImg);
      connect(lastImageHandler, &InputHandler::last, handler, &ImageHandler::onLast);
      //*verboseOutput << "Connect " << lastImageHandler->metaObject()->className()
      //  << " to " << handler->metaObject()->className() << endl;

    } else {
      throw runtime_error("Weird pipeline state");
    }
    lastInputHandler = nullptr;
    lastImageHandler = handler;

    append(handler);
  }

  void Pipeline::operator<<(InputHandler *handler) {
    if (lastImageHandler != nullptr) {
      ImageTrash *trash = new ImageTrash();
      connect(lastImageHandler, &ImageHandler::inputImg, trash, &ImageTrash::onInputImg);
      connect(lastImageHandler, &ImageHandler::last, trash, &ImageTrash::onLast);
      append(trash);

      lastImageHandler = nullptr;
      lastInputHandler = trash;
    }

    if (lastInputHandler != nullptr) {
      connect(lastInputHandler, &InputHandler::input, handler, &InputHandler::onInput);
      connect(lastInputHandler, &InputHandler::last, handler, &InputHandler::onLast);

    } else {
      throw runtime_error("Weird pipeline state");
    }
    lastInputHandler = handler;
    lastImageHandler = nullptr;

    append(handler);
  }

  void Pipeline::process() {
    // listen when last element finish their job
    if (lastInputHandler != nullptr) {
      connect(lastInputHandler, &InputHandler::last, this, &Pipeline::done);
    } else if (lastImageHandler != nullptr) {
      connect(lastImageHandler, &ImageHandler::last, this, &Pipeline::done);
    } else {
      throw logic_error("No handler in pipeline");
    }

    emit src->process();
  }
}
