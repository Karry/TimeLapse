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

#include <QtCore/QCommandLineParser>
#include <QtCore/QIODevice>
#include <QtCore/QTextStream>

using namespace std;

namespace timelapse {

#if QT_VERSION >= QT_VERSION_CHECK(5,14,0)
  // for compatibility with Qt < 5.14 we are not using Qt::endl directly
  inline QTextStream &endl(QTextStream &s) {
    return Qt::endl(s);
  }
#endif

#if QT_VERSION >= QT_VERSION_CHECK(5,15,0)
  constexpr auto SkipEmptyParts = Qt::SkipEmptyParts;
#else
  constexpr auto SkipEmptyParts = QString::SkipEmptyParts;
#endif


  class ErrorMessageHelper {
  public:

    ErrorMessageHelper(QIODevice *errDev, QCommandLineParser *parser) :
    _coloredTerm(true), _err(errDev), _parser(parser) {
    }

    explicit ErrorMessageHelper(QIODevice *errDev) :
    _coloredTerm(true), _err(errDev), _parser(nullptr) {
    }

    void operator<<(const QString &s) {
      //bold red text\033[0m\n";
      if (_coloredTerm)
        _err << "\033[1;31m";
      _err << endl << "!!! " << s << " !!!";
      if (_coloredTerm)
        _err << "\033[0m";
      _err << endl << endl;
      if (_parser != nullptr)
        _parser->showHelp(-1);
      else
        exit(-1);
    }

  protected:
    bool _coloredTerm; // TODO: determine if terminal is color capable
    QTextStream _err;
    QCommandLineParser *_parser;
  };

  inline void registerQtMetaTypes() {
    qRegisterMetaType<QList<InputImageInfo>>("QList<InputImageInfo>");
  }

}
