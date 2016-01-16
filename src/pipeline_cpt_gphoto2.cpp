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

/**
 * This code was borrowed from:
 * 
 * qtmultimedia-gphoto : 
 *    https://github.com/dept2/qtmultimedia-gphoto
 *    LGPL 2.1 Copyright © 2014 Boris Moiseev
 * 
 * gphoto2:
 *    https://github.com/gphoto/gphoto2
 *    GPL 2, various authors
 */

#include <unistd.h>
#include <bits/signum.h>
#include <signal.h>
#include <sys/time.h>

#include "pipeline_cpt_gphoto2.h"
#include "pipeline_cpt_gphoto2.moc"


using namespace std;
using namespace timelapse;

namespace timelapse {

  //GPParams gp_params;

  /** GPhoto2 rutines */

  int _get_portinfo_list(GPPortInfoList **portinfo_list) {
    int count, result;
    GPPortInfoList *list = NULL;

    if (*portinfo_list)
      return GP_OK; // list is created already 

    result = gp_port_info_list_new(&list);
    if (result < GP_OK)
      return result;

    result = gp_port_info_list_load(list);
    if (result < GP_OK) {
      gp_port_info_list_free(list);
      return result;
    }
    count = gp_port_info_list_count(list);
    if (count < 0) {
      gp_port_info_list_free(list);
      return count;
    }
    *portinfo_list = list;
    return GP_OK;
  }

  /**
   * Callback fuction for logging messages from libgphoto
   * 
   * @param context - gphoto context
   * @param text - message in utf8 
   * @param data - QTextStream pointer for printing messages
   */
  void _gp_context_msg_func(GPContext *context, const char *text, void *data) {

    *((QTextStream *) data) << "GPhoto2: " << QString::fromUtf8(text) << endl;
  }

  Gphoto2Device::Gphoto2Device(GPContext *context, Camera *camera, QString port, QString model) :
  context(context), camera(camera), port(port), model(model) {

    gp_camera_ref(camera);
    gp_context_ref(context);
  }

  Gphoto2Device::Gphoto2Device(const timelapse::Gphoto2Device& other) :
  context(other.context), camera(other.camera), port(other.port), model(other.model) {

    gp_camera_ref(camera);
    gp_context_ref(context);
  }

  Gphoto2Device::~Gphoto2Device() {
    gp_camera_unref(camera);
    gp_context_unref(context);
  }

  /**
   * This method try to setup RAM storage in camera.
   * We download captured images, so we don't want to keep it in memory card...
   */
  void Gphoto2Device::tryToSetRamStorage() {
    /*
     * $ gphoto2 --get-config capturetarget
     * Label: Capture Target
     * Type: RADIO
     * Current: Internal RAM
     * Choice: 0 Internal RAM
     * Choice: 1 Memory card
     *
     * $ gphoto2 --set-config capturetarget=0 --capture-image-and-download
     */

    CameraWidget *rootconfig, *targetWidget;
    int ret;
    const char *name = "capturetarget";

    // get root widget
    ret = gp_camera_get_config(camera, &rootconfig, context);
    if (ret != GP_OK)
      return;
    ret = gp_widget_get_child_by_name(rootconfig, name, &targetWidget);
    if (ret != GP_OK)
      ret = gp_widget_get_child_by_label(rootconfig, name, &targetWidget);
    if (ret != GP_OK)
      return;

    // check widget (it should be RADIO)
    CameraWidgetType type;
    ret = gp_widget_get_type(targetWidget, &type);
    if (ret != GP_OK)
      return;
    if (type != GP_WIDGET_RADIO)
      return;

    // setup to choice that contains "RAM"
    int cnt = gp_widget_count_choices(targetWidget);
    int i;
    for (i = 0; i < cnt; i++) {
      const char *choice;
      ret = gp_widget_get_choice(targetWidget, i, &choice);
      if (ret < GP_OK)
        return;
      QString s = QString::fromLatin1(choice);
      if (s.contains("RAM")) {
        //printf("setting %s to choice %d: %s\n", name, i, choice);
        gp_widget_set_value(targetWidget, choice);
      }
    }

  }

  Magick::Image Gphoto2Device::capture() {
    int ret;

    // init camera
    ret = gp_camera_init(camera, context);
    if (ret < 0) {
      throw std::invalid_argument(QString("Can't init camera on port %1").arg(port).toStdString());
    }

    tryToSetRamStorage();

    QByteArray result;

    // Capture the frame from camera
    CameraFilePath filePath;
    ret = gp_camera_capture(camera, GP_CAPTURE_IMAGE, &filePath, context);
    if (ret < GP_OK) {
      gp_camera_exit(camera, context);
      throw std::runtime_error(QString("Failed to capture frame with camera %1: %2").arg(model).arg(ret).toStdString());
    }

    qDebug() << "Captured frame:" << filePath.folder << filePath.name;

    // Download the file
    CameraFile* file;
    ret = gp_file_new(&file);
    ret = gp_camera_file_get(camera, filePath.folder, filePath.name, GP_FILE_TYPE_NORMAL, file, context);
    if (ret < GP_OK) {
      gp_camera_exit(camera, context);
      throw std::runtime_error(QString("Failed to get file from camera %1: %2").arg(model).arg(ret).toStdString());
    }

    const char* data;
    unsigned long int size = 0;

    gp_file_get_data_and_size(file, &data, &size);
    result = QByteArray(data, size);
    //emit imageCaptured(id, result, fileName);

    gp_file_free(file);

    while (1) {
      CameraEventType type;
      void* data;
      ret = gp_camera_wait_for_event(camera, 100, &type, &data, context);
      if (type == GP_EVENT_TIMEOUT) {
        break;
      } else if (type == GP_EVENT_CAPTURE_COMPLETE) {
        //                qDebug("Capture completed\n");
      } else if (type != GP_EVENT_UNKNOWN) {
        qWarning("Unexpected event received from camera: %d\n", (int) type);
      }
    }

    ret = gp_camera_exit(camera, context);

    return Magick::Image();
  }

  QString Gphoto2Device::toString() {
    return QString("%1\t\"%2\"").arg(port).arg(model);
  }

  Gphoto2Device Gphoto2Device::operator=(const timelapse::Gphoto2Device& o) {
    gp_camera_unref(camera);
    gp_context_unref(context);

    context = o.context;
    camera = o.camera;
    port = o.port;
    model = o.model;

    gp_camera_ref(camera);
    gp_context_ref(context);

    return *this;
  }

  GPContext * Gphoto2Device::initContext(QTextStream *verboseOut, QTextStream *errOut) {
    GPContext *context = gp_context_new();
    ALLOC_CHECK(context);

    gp_context_set_idle_func(context, 0, NULL);
    gp_context_set_progress_funcs(context, 0, 0, 0, NULL);
    gp_context_set_error_func(context, _gp_context_msg_func, errOut);
    gp_context_set_status_func(context, _gp_context_msg_func, verboseOut);
    gp_context_set_question_func(context, 0, NULL);
    gp_context_set_cancel_func(context, 0, NULL);
    gp_context_set_message_func(context, _gp_context_msg_func, verboseOut);

    return context;
  }

  void Gphoto2Device::releaseContext(GPContext *context) {
    gp_context_unref(context);
  }

  Gphoto2Device Gphoto2Device::createDevice(GPContext *context, QString port, QTextStream *verboseOut) {
    int p, r;
    GPPortInfo info;

    /* Create the list of ports and load it. */
    GPPortInfoList *portinfo_list = NULL;
    CR(_get_portinfo_list(&portinfo_list));

    /* Search our port in the list. */
    /* NOTE: This call can modify "il" for regexp matches! */

    p = gp_port_info_list_lookup_path(portinfo_list, port.toStdString().c_str());
    if (p == GP_ERROR_UNKNOWN_PORT) {
      gp_port_info_list_free(portinfo_list);
      throw std::invalid_argument(QString("Can't found port %1").arg(port).toStdString());
    }

    // Get info about our port. 
    r = gp_port_info_list_get_info(portinfo_list, p, &info);
    if (r < 0) {
      gp_port_info_list_free(portinfo_list);
      throw std::invalid_argument(QString("Can't get info about port %1").arg(port).toStdString());
    }

    // create camera
    Camera *camera = NULL;
    r = gp_camera_new(&camera);
    if (r < 0) {
      gp_port_info_list_free(portinfo_list);
      throw std::runtime_error("Can't create camera object");
    }

    /* Set the port of our camera. */
    r = gp_camera_set_port_info(camera, info);
    if (r < 0) {
      gp_camera_unref(camera);
      gp_port_info_list_free(portinfo_list);
      throw std::invalid_argument(QString("Can't set port %1 to camera").arg(port).toStdString());
    }

    //gp_port_info_get_path(info, &path);
    //gp_setting_set("gphoto2", "port", path);

    // init camera
    r = gp_camera_init(camera, context);
    if (r < 0) {
      gp_camera_unref(camera);
      gp_port_info_list_free(portinfo_list);
      throw std::invalid_argument(QString("Can't init camera on port %1").arg(port).toStdString());
    }


    CameraAbilities abilities;
    r = gp_camera_get_abilities(camera, &abilities);
    if (r < 0) {
      gp_camera_exit(camera, context);
      gp_camera_unref(camera);
      gp_port_info_list_free(portinfo_list);
      throw std::invalid_argument(QString("Can't get camera abilities (on port %1)").arg(port).toStdString());
    }

    // print informations to verbose output
    char * name, *path; // *lib;
    QString typestr;
    GPPortType type;
    gp_port_info_get_name(info, &name);
    QString model(abilities.model);
    gp_port_info_get_path(info, &path);
    //gp_port_info_get_library_filename(info, &lib);
    gp_port_info_get_type(info, &type);
    switch (type) {
      case GP_PORT_NONE: typestr = "None";
        break;
      case GP_PORT_SERIAL: typestr = "Srial port";
        break; /**< \brief Serial port. */
      case GP_PORT_USB: typestr = "USB port";
        break; /**< \brief USB port. */
      case GP_PORT_DISK: typestr = "Local mount point";
        break; /**< \brief Disk / local mountpoint port. */
      case GP_PORT_PTPIP: typestr = "PTP/IP port";
        break; /**< \brief PTP/IP port. */
      case GP_PORT_USB_DISK_DIRECT: typestr = "Direct IO USB";
        break; /**< \brief Direct IO to an usb mass storage device. */
      case GP_PORT_USB_SCSI: typestr = "SCSI USB";
        break; /**< \brief USB Mass Storage raw SCSI port. */
    }
    *verboseOut << "Create camera device " << model << " on port " << path <<
      " (" << typestr << ")" << endl;


    // exit camera, we don't want hold camera connection for long time
    r = gp_camera_exit(camera, context);

    if ((abilities.operations & GP_OPERATION_TRIGGER_CAPTURE) == 0
      || (abilities.operations & GP_OPERATION_CAPTURE_IMAGE) == 0) {
      gp_camera_unref(camera);
      gp_port_info_list_free(portinfo_list);
      throw std::invalid_argument(QString("Camera %1 don't support TRIGGER_CAPTURE").arg(model).toStdString());
    }

    Gphoto2Device d(context, camera, port, model);

    // free memory and decrement references
    gp_camera_unref(camera);
    gp_port_info_list_free(portinfo_list);

    return d;
  }

  QList<Gphoto2Device> Gphoto2Device::listDevices(QTextStream *verboseOut, QTextStream *errOut) {
    QList<Gphoto2Device> devices;

    int x, count;
    const char *model = NULL, *port = NULL;

    GPContext *context = Gphoto2Device::initContext(verboseOut, errOut);

    GPPortInfoList *portinfo_list = NULL;
    CR(_get_portinfo_list(&portinfo_list));
    count = gp_port_info_list_count(portinfo_list);

    CameraList *list = NULL;
    CR(gp_list_new(&list));

    CameraAbilitiesList *abilities_list = NULL;
    CR(gp_abilities_list_new(&abilities_list));
    gp_abilities_list_load(abilities_list, context);
    gp_abilities_list_detect(abilities_list, portinfo_list, list, context);

    CL(count = gp_list_count(list), list);

    for (x = 0; x < count; x++) {
      CL(gp_list_get_name(list, x, &model), list);
      CL(gp_list_get_value(list, x, &port), list);

      try {
        Gphoto2Device d = createDevice(context, port, verboseOut);
        d.capture();
        devices.append(d);
      } catch (std::exception &e) {
        *verboseOut << "GPhoto2 device " << port << " / " << model << " can't be used for capturing: " << e.what() << endl;
      }

      //printf("%-30s %-16s\n", model, port);
    }

    gp_list_free(list);
    gp_abilities_list_free(abilities_list);
    gp_port_info_list_free(portinfo_list);
    releaseContext(context);

    return devices;
  }
}
