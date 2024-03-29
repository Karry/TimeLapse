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

#include <TimeLapse/pipeline_source.h>

#include <TimeLapse/timelapse.h>

#include <QtCore/QFileInfo>
#include <QtCore/QTextStream>
#include <QtCore/QString>
#include <QtCore/QDir>

using namespace std;
using namespace timelapse;


namespace timelapse {

  PipelineFileSource::PipelineFileSource(const QStringList &_inputArguments, const QStringList &_fileSuffixes, bool _recursive,
    QTextStream *_verboseOutput, QTextStream *_err) :
  inputArguments(_inputArguments), fileSuffixes(_fileSuffixes), recursive(_recursive),
  verboseOutput(_verboseOutput), err(_err) {

    connect(this, &PipelineFileSource::processNext, this, &PipelineFileSource::takeNext, Qt::QueuedConnection);
  }

  QList<InputImageInfo> PipelineFileSource::listDirectory(QDir d) {
    *verboseOutput << "Dive into directory: " << d.path() << endl;
    QList<InputImageInfo> inputs;
    QFileInfoList l = d.entryInfoList(QDir::Files, QDir::Name);
    *verboseOutput << "...found " << l.size() << " entries" << endl;
    for (QFileInfo i2 : l) {
      if (i2.isFile() || i2.isSymLink()) {
        if (fileSuffixes.isEmpty() || fileSuffixes.contains(i2.completeSuffix(), Qt::CaseInsensitive)) {
          *verboseOutput << "Input file: " << i2.filePath() << endl;
          inputs.append(InputImageInfo(i2));
        } else {
          *verboseOutput << "Skip file: " << i2.filePath() << endl;
        }
      } else if (i2.isDir()) {
        if (recursive) {
          inputs.append(listDirectory(QDir(i2.filePath())));
        } else {
          *verboseOutput << "Ignore directory " << i2.filePath() << endl;
        }
      } else {
        emit error(QString("Can't add input ") + i2.filePath());
      }
    }
    return inputs;
  }

  QList<InputImageInfo> PipelineFileSource::parseArguments() {
    QList<InputImageInfo> inputs;
    *verboseOutput << "inputs: " << inputArguments.join(", ") << endl;
    for (QString inputArg : inputArguments) {
      QFileInfo i(inputArg);
      if (i.isFile() || i.isSymLink()) {
        *verboseOutput << "Input file: " << inputArg << endl;
        inputs.append(InputImageInfo(i));
      } else if (i.isDir()) {
        inputs.append(listDirectory(QDir(i.filePath())));
      } else {
        emit error(QString("Can't find input ") + inputArg);
      }
    }
    if (inputs.empty())
      emit error("No input images found!");

    // check suffixes
    QString suffix;
    bool first = true;
    for (InputImageInfo in : inputs) {
      if (first) {
        suffix = in.fileInfo().completeSuffix();
        first = false;
      } else {
        if (suffix != in.fileInfo().completeSuffix()) {
          *err << "Input files with multiple suffixes. Are you sure that this is one sequence?" << endl;
          break;
        }
      }
    }

    return inputs;
  }

  void PipelineFileSource::onInput([[maybe_unused]] InputImageInfo info) {
    // just ignore, we are the source
  }

  void PipelineFileSource::takeNext(QList<InputImageInfo> inputs) {
    if (inputs.empty()) {
      emit last();
    } else {
      emit input(inputs.takeFirst());
      emit processNext(inputs);
    }
  }

  void PipelineFileSource::process() {
    emit processNext(parseArguments());
  }


}
