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

#ifndef VERSION_TIMELAPSE
#define VERSION_TIMELAPSE "0.0.0"
#endif

#ifndef TIMELAPSE_H
#define	TIMELAPSE_H

using namespace std;
using namespace timelapse;

namespace timelapse {

  class ErrorMessageHelper {
  public:

    ErrorMessageHelper(QIODevice *errDev, QCommandLineParser *parser) :
    _coloredTerm(true), _err(errDev), _parser(parser) {
    }
    
    ErrorMessageHelper(QIODevice *errDev) :
    _coloredTerm(true), _err(errDev), _parser(NULL) {
    }

    void operator<<(const QString &s) {
      //bold red text\033[0m\n";
      if (_coloredTerm)
        _err << "\033[1;31m";
      _err << endl << "!!! " << s << " !!!";
      if (_coloredTerm)
        _err << "\033[0m";
      _err << endl << endl;
      if (_parser != NULL)
        _parser->showHelp(-1);
      else
        exit(-1);
    }

  protected:
    bool _coloredTerm; // TODO: determine if terminal is color capable
    QTextStream _err;
    QCommandLineParser *_parser;
  };
  
}


#endif	/* TIMELAPSE_H */

