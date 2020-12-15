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

#pragma once

#include "pipeline_cpt.h"

#include <Magick++.h>

#include <gphoto2/gphoto2-file.h>
#include <gphoto2/gphoto2-camera.h>
#include <gphoto2/gphoto2-port-log.h>

#include <QtCore/QObject>
#include <QtCore/QDebug>
#include <QtCore/QTimer>
#include <QtCore/QTextStream>
#include <QtCore/QCoreApplication>
#include <QtCore/QFileInfo>
#include <QtCore/QTemporaryDir>
#include <QElapsedTimer>

namespace timelapse {
  
#define CR(result)       {int __r=(result); if (__r<0) throw std::runtime_error("Gphoto2 internal error");}
#define CRU(result,file) {int __r=(result); if (__r<0) {gp_file_unref(file);throw std::runtime_error("Gphoto2 internal error");}}
#define CL(result,list)  {int __r=(result); if (__r<0) {gp_list_free(list); throw std::runtime_error("Gphoto2 internal error");}}

#define SHUTTERSPEED_CONFIG "shutterspeed2"
#define CAPTURETARGET_CONFIG "capturetarget"
#define BATTERYLEVEL_CONFIG "batterylevel"
#define INTERNALRAM_VALUE "Internal RAM"
#define BULB_CONFIG "bulb"
#define ON_VALUE "1"
#define OFF_VALUE "0"

  class Gphoto2Device : public CaptureDevice {
    Q_OBJECT
  public:
    Gphoto2Device(GPContext *context, Camera *camera, QString port, QString model);
    Gphoto2Device(const timelapse::Gphoto2Device& other);
    virtual ~Gphoto2Device();

    virtual void capture(QTextStream *verboseOut, ShutterSpeedChoice shutterSpeed = ShutterSpeedChoice());

    virtual QString toString();
    virtual QString toShortString();

    Gphoto2Device operator=(const timelapse::Gphoto2Device&);

    //virtual QObject* qObject();

    static Gphoto2Device createDevice(GPContext *context, QString port, QTextStream *verboseOut);
    static QList<Gphoto2Device> listDevices(QTextStream *verboseOut, QTextStream *errOut);
    static GPContext *initContext(QTextStream *verboseOut, QTextStream *errOut);
    static void releaseContext(GPContext *context);
    virtual QList<ShutterSpeedChoice> getShutterSpeedChoices();
    virtual ShutterSpeedChoice currentShutterSpeed();
    virtual bool isBusy() ;

  protected slots:
    void pollingTimeout();

  protected:
    bool lock();
    bool unlock();
    void findConfigWidget(QString option, CameraWidget **rootconfig, CameraWidget **child);
    QString getConfigValue(QString option);
    bool isConfigRw(QString option);
    QStringList getConfigRadioChoices(QString option);
    void setConfig(QString option, QString value, bool exactMatch, CameraWidgetType expectedType);

    void downloadAndEmitImage(CameraFilePath *path);
    void deleteImage(CameraFilePath *path);
    void waitAndHandleEvent(int waitMs, CameraEventType *type);
    void bulbWait(int bulbLengthMs);
    bool tryToSetRamStorage();

    GPContext *context;
    Camera *camera;
    QString port;
    QString model;
    QElapsedTimer timer;
    bool pollingScheduled;
    bool deleteImageAfterDownload;
    bool deviceLocked;
  };
}
