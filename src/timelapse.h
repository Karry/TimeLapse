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

#include <QtCore/QCommandLineParser>
#include <QtCore/QIODevice>
#include <QtCore/QTextStream>

using namespace std;

// Shared library support
#  if __GNUC__ >= 4
#    define TIME_LAPSE_API __attribute__ ((visibility ("default")))
#  else
#    define TIME_LAPSE_API
#  endif

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

  void registerQtMetaTypes();
}
