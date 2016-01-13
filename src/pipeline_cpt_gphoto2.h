/*
 *   Copyright (C) 2016 Lukáš Karas <lukas.karas@centrum.cz>
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


#ifndef PIPELINECPTGPHOTO2_H
#define	PIPELINECPTGPHOTO2_H

#include <gphoto2/gphoto2-file.h>
#include <gphoto2/gphoto2-camera.h>

#include <QtCore/QObject>
#include <QtCore/QDebug>
#include <QtCore/QTimer>
#include <QtCore/QTextStream>
#include <QtCore/QCoreApplication>
#include <QtCore/QFileInfo>
#include <QtCore/QTemporaryDir>

#include <Magick++.h>

#include "pipeline_cpt.h"

namespace timelapse {

  class Gphoto2Device : public QObject, public CaptureDevice {
    Q_OBJECT
  public:
    Gphoto2Device(QString dev = "");
    Gphoto2Device(const timelapse::Gphoto2Device& other);
    virtual ~Gphoto2Device();

    virtual Magick::Image capture();

    virtual QString toString();
    Gphoto2Device operator=(const timelapse::Gphoto2Device&);
  };
}

#endif	/* PIPELINECPTGPHOTO2_H */

