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

#include <TimeLapse/pipeline_video_assembly.h>

#include <TimeLapse/timelapse.h>

#include <QtCore/QFileInfo>
#include <QtCore/QObject>
#include <QtCore/QTextStream>
#include <QtCore/QString>
#include <QtCore/QDir>
#include <QtCore/QProcess>

#include <cassert>

using namespace std;
using namespace timelapse;


namespace timelapse {

  VideoAssembly::VideoAssembly(QDir _tempDir, QTextStream *_verboseOutput, QTextStream *_err, bool _dryRun,
    QFileInfo _output, int _width, int _height, float _fps, QString _bitrate, QString _codec, QString _builderBinary) :
  tempDir(_tempDir), verboseOutput(_verboseOutput), err(_err), dryRun(_dryRun),
  output(_output), width(_width), height(_height), fps(_fps), bitrate(_bitrate), codec(_codec),
  builderBinary(_builderBinary) {
  }

  VideoAssembly::~VideoAssembly() {
    if (builderProc != nullptr) {
      builderProc->terminate();
      if (!builderProc->waitForFinished(-1 /* no timeout */)) {
        *err << "Builder waiting failed:" << builderProc->errorString() << endl;
        emit error("Builder waiting failed:" + builderProc->errorString());
      }
    }
  }

  void VideoAssembly::onInput(InputImageInfo info) {
    emit input(info);
  }

  QString VideoAssembly::getOrDetectBuilder() {
    if (!builderBinary.isEmpty()) {
      return builderBinary;
    }
    QString cmd = "avconv";
    QProcess avconv;
    avconv.setProcessChannelMode(QProcess::MergedChannels);
    avconv.start("avconv", QStringList() << "-version");
    if (!avconv.waitForFinished() || avconv.exitCode() != 0) {
      *verboseOutput << "avconv exited with error, try to use ffmpeg" << endl;
      QProcess ffmpeg;
      ffmpeg.setProcessChannelMode(QProcess::MergedChannels);
      ffmpeg.start("ffmpeg", QStringList() << "-version");
      if (!ffmpeg.waitForFinished() || ffmpeg.exitCode() != 0) {
        *err << "Both commands (avconv, ffmpeg) fails! Try to use ffmpeg.";
      }
      cmd = "ffmpeg";
    }
    builderBinary = cmd;
    return cmd;
  }

  void VideoAssembly::onFinished(int exitCode, QProcess::ExitStatus exitStatus) {
    assert(builderProc != nullptr);

    *verboseOutput << ">> " << builderProc->readAll();
    if (exitCode != 0) {
      *err << "Video builder exited with " << builderProc->exitCode() << endl;
      emit error(QString("Video builder exited with %2").arg(builderProc->exitCode()));
    }

    builderProc->deleteLater();
    builderProc = nullptr;
    emit last();
  }

  void VideoAssembly::onStdoutReady() {
    assert(builderProc != nullptr);
    *verboseOutput << ">> " << builderProc->readAll();
  }

  void VideoAssembly::onLast() {
    assert(builderProc==nullptr);
    *verboseOutput << "Assembling video..." << endl;

    QString cmd = getOrDetectBuilder();

    // avconv -f image2 -r $fps -s $res -i morphed/%06d.jpg -b:v $bitrate -c:v libx264  video.mkv
    QStringList args = QStringList()
      << "-f" << "image2"
      << "-r" << QString("%1").arg(fps)
      << "-s" << QString("%1x%2").arg(width).arg(height)
      << "-i" << (tempDir.path() + QDir::separator() + QString("%0") + QString("%1d.jpeg").arg(FRAME_FILE_LEADING_ZEROS))
      << "-b:v" << bitrate
      << "-c:v" << codec
      << "-y" // Overwrite output file without asking
      << "-r" << QString("%1").arg(fps)
      << output.filePath();

    *verboseOutput << "Executing:" << endl << cmd << " " << args.join(' ') << endl;
    if (!dryRun) {
      builderProc = new QProcess();
      builderProc->setProcessChannelMode(QProcess::MergedChannels);

      connect(builderProc, SIGNAL(finished(int, QProcess::ExitStatus)),
              this, SLOT(onFinished(int, QProcess::ExitStatus)));
      connect(builderProc, &QProcess::readyReadStandardOutput, this, &VideoAssembly::onStdoutReady);
      connect(builderProc, &QProcess::started, this, &VideoAssembly::started);

      builderProc->start(cmd, args);
    } else {
      emit last();
    }
  }

}

