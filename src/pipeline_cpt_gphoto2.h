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

  typedef enum {
    FLAGS_RECURSE = 1 << 0,
    FLAGS_REVERSE = 1 << 1,
    FLAGS_QUIET = 1 << 2,
    FLAGS_FORCE_OVERWRITE = 1 << 3,
    FLAGS_STDOUT = 1 << 4,
    FLAGS_STDOUT_SIZE = 1 << 5,
    FLAGS_NEW = 1 << 6,
    FLAGS_RESET_CAPTURE_INTERVAL = 1 << 7,
    FLAGS_KEEP = 1 << 8,
    FLAGS_KEEP_RAW = 1 << 9,
    FLAGS_SKIP_EXISTING = 1 << 10
  } Flags;

  typedef enum {
    MULTI_UPLOAD,
    MULTI_UPLOAD_META,
    MULTI_DOWNLOAD,
    MULTI_DELETE
  } MultiType;


  typedef struct _GPParams GPParams;

  struct _GPParams {
    Camera *camera;
    GPContext *context;
    char *folder;
    char *filename;

    unsigned int cols;

    Flags flags;

    /** This field is supposed to be private. Usually, you use the
     * gp_camera_abilities_list() function to access it.
     */
    CameraAbilitiesList *_abilities_list;

    GPPortInfoList *portinfo_list;
    int debug_func_id;

    MultiType multi_type;
    CameraFileType download_type; /* for multi download */

    char *hook_script; /* If non-NULL, hook script to run */
    char **envp; /* envp from the main() function */
  };

  struct privstr {
    int fd;
  };

  //static CameraFileHandler xhandler = { x_size, x_read, x_write };
#define CR(result)       {int __r=(result); if (__r<0) throw std::runtime_error("Gphoto2 internal error");}
#define CRU(result,file) {int __r=(result); if (__r<0) {gp_file_unref(file);throw std::runtime_error("Gphoto2 internal error");}}
#define CL(result,list)  {int __r=(result); if (__r<0) {gp_list_free(list); throw std::runtime_error("Gphoto2 internal error");}}

#define SHUTTERSPEED_CONFIG "shutterspeed2"
#define CAPTURETARGET_CONFIG "capturetarget"
#define INTERNALRAM_VALUE "Internal RAM"
#define BULB_CONFIG "bulb"
#define ON_VALUE "1"
#define OFF_VALUE "0"

  class Gphoto2Device : public QObject, public CaptureDevice {
    Q_OBJECT
  public:
    Gphoto2Device(GPContext *context, Camera *camera, QString port, QString model);
    Gphoto2Device(const timelapse::Gphoto2Device& other);
    virtual ~Gphoto2Device();

    virtual void capture(ShutterSpeedChoice shutterSpeed = ShutterSpeedChoice());

    virtual QString toString();
    virtual QString toShortString();

    Gphoto2Device operator=(const timelapse::Gphoto2Device&);

    virtual QObject* qObject();

    static Gphoto2Device createDevice(GPContext *context, QString port, QTextStream *verboseOut);
    static QList<Gphoto2Device> listDevices(QTextStream *verboseOut, QTextStream *errOut);
    static GPContext *initContext(QTextStream *verboseOut, QTextStream *errOut);
    static void releaseContext(GPContext *context);
    virtual QList<ShutterSpeedChoice> getShutterSpeedChoices();
    virtual ShutterSpeedChoice currentShutterSpeed();

  signals:
    void imageCaptured(QString type, Magick::Blob blob, Magick::Geometry sizeHint);

  protected slots:
    void pollingTimeout();

  protected:
    void findConfigWidget(QString option, CameraWidget **rootconfig, CameraWidget **child);
    QString getConfigRadio(QString option);
    bool isConfigRw(QString option);
    QStringList getConfigRadioChoices(QString option);
    void setConfig(QString option, QString value, CameraWidgetType expectedType);

    void downloadAndEmitImage(CameraFilePath *path);
    void deleteImage(CameraFilePath *path);
    void waitAndHandleEvent(int waitMs, CameraEventType *type);
    void bulbWait(int bulbLengthMs);
    bool tryToSetRamStorage();

    GPContext *context;
    Camera *camera;
    QString port;
    QString model;
    QTime timer;
    bool pollingScheduled;
    bool deleteImageAfterDownload;
    bool deviceLocked;
  };
}

#endif	/* PIPELINECPTGPHOTO2_H */

