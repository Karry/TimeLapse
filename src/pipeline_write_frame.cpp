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

#include <QtCore/QFileInfo>
#include <QtCore/QObject>
#include <QtCore/QDebug>
#include <QtCore/QTimer>
#include <QtCore/QTextStream>
#include <QtCore/QCoreApplication>
#include <QtCore/QFileInfo>
#include <QtCore/QTemporaryDir>
#include <QtCore/QString>
#include <QtCore/QDir>
#include <QtCore/QProcess>

#include <exception>

#include <Magick++.h>

#include "pipeline_write_frame.h"
#include "pipeline_write_frame.moc"

using namespace std;
using namespace timelapse;


namespace timelapse {

  WriteFrame::WriteFrame(QDir _outputDir, QTextStream *_verboseOutput, bool _dryRun) :
  outputDir(_outputDir), frameNumberLocale(QLocale::c()),
  verboseOutput(_verboseOutput), dryRun(_dryRun) {

    frameNumberLocale.setNumberOptions(QLocale::OmitGroupSeparator);
  }

  QString WriteFrame::leadingZeros(int i, int leadingZeros) {
    // default locale can include thousand delimiter
    QString s = frameNumberLocale.toString(i);
    if (leadingZeros <= s.length())
      return s;

    return s.prepend(QString(leadingZeros - s.length(), '0'));
  }

  void WriteFrame::onInput2(InputImageInfo info, Magick::Image img) {
    QString framePath = outputDir.path() + QDir::separator()
      + leadingZeros(info.frame, FRAME_FILE_LEADING_ZEROS) + QString(".jpeg");

    *verboseOutput << "Write frame " << framePath << endl;
    if (!dryRun) {
      img.compressType(Magick::JPEGCompression);
      img.magick( "JPEG" );
      img.write(framePath.toStdString());
    }
    // update image location & emit signal
    info.filePath = framePath.toStdString();
    emit input(info, img);
  }

}



