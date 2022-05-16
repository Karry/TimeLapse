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

  // for compatibility with Qt < 5.14
  inline QTextStream &endl(QTextStream &s) {
#if QT_VERSION >= QT_VERSION_CHECK(5,14,0)
    return Qt::endl(s);
#else
    return QTextStreamFunctions::endl(s);
#endif
  }

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

  template<typename T> class Option {
  public:
    virtual bool isDefined() const = 0;

    virtual bool isEmpty() const {
      return !isDefined();
    };
    virtual const T operator*() const = 0;
    virtual const T getOrElse(const T def) const = 0;
  };

  template<typename T> class None : public Option<T> {
  public:

    virtual bool isDefined() const override {
      return false;
    };

    virtual const T operator*() const override {
      throw runtime_error("Not defined");
    };

    virtual const T getOrElse(const T def) const {
      return def;
    };
  };

  template<typename T> class Some : public Option<T> {
  public:

    explicit Some(const T &v) : val(v) {
    }

    virtual bool isDefined() const override  {
      return true;
    }

    virtual const T operator*() const override {
      return val;
    };

    virtual const T getOrElse([[maybe_unused]] const T def) const override {
      return val;
    };
  private:
    T val;
  };

  typedef Option<int> OptionInt;
  typedef Some<int> SomeInt;
#define NoneInt None<int>() 
  typedef Option<float> OptionFloat;
  typedef Some<float> SomeFloat;
#define NoneFloat None<float>()
  typedef Option<double> OptionDouble;
  typedef Some<double> SomeDouble;
#define NoneDouble None<double>()
}
