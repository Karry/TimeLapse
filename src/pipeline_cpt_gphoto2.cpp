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

#include "pipeline_cpt_gphoto2.h"

#include "timelapse.h"

#include <unistd.h>

using namespace std;
using namespace timelapse;

namespace timelapse {

  /** GPhoto2 rutines */

  int _get_portinfo_list(GPPortInfoList **portinfo_list) {
    int count, result;
    GPPortInfoList *list = nullptr;

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
  void _gp_context_msg_func([[maybe_unused]] GPContext *context, const char *text, void *data) {

    *((QTextStream *) data) << "GPhoto2: " << QString::fromUtf8(text) << endl;
  }

  /**
   * \brief Logging function hook
   * 
   * This is the function frontends can use to receive logging information
   * from the libgphoto2 framework. It is set using gp_log_add_func() and 
   * removed using gp_log_remove_func() and will then receive the logging
   * messages of the level specified.
   *
   * \param level the log level of the passed message, as set by the camera driver or libgphoto2
   * \param domain the logging domain as set by the camera driver, or libgphoto2 function
   * \param str the logmessage, without linefeed
   * \param data the caller private data that was passed to gp_log_add_func()
   */
  void _gp_port_debug([[maybe_unused]] GPLogLevel level, const char *domain, const char *str, void *data) {
    *((QTextStream *) data) << "GPhoto2 - " << QString::fromUtf8(domain) << ": " << QString::fromUtf8(str) << endl;
  }

  Gphoto2Device::Gphoto2Device(GPContext *context, Camera *camera, QString port, QString model) :
  context(context), camera(camera), port(port), model(model), timer(),
  pollingScheduled(false), deviceLocked(false) {

    gp_camera_ref(camera);
    gp_context_ref(context);
  }

  Gphoto2Device::Gphoto2Device(const timelapse::Gphoto2Device& other) :
  context(other.context), camera(other.camera), port(other.port), model(other.model), timer(),
  pollingScheduled(false), deviceLocked(false) {

    if (other.deviceLocked)
      throw std::logic_error("Creating copy of locked camera is not implemented!");

    gp_camera_ref(camera);
    gp_context_ref(context);
  }

  Gphoto2Device::~Gphoto2Device() {
    unlock();

    gp_camera_unref(camera);
    gp_context_unref(context);
  }

  bool Gphoto2Device::lock() {
    if (deviceLocked) {
      return false;
    }

    int ret;
    ret = gp_camera_init(camera, context);
    if (ret < GP_OK) {
      throw std::invalid_argument(QString("Can't init camera on port %1: %2")
        .arg(port)
        .arg(gp_result_as_string(ret))
        .toStdString());
    }
    deviceLocked = true;
    return true;
  }

  bool Gphoto2Device::unlock() {
    if (!deviceLocked) {
      return false;
    }

    int ret;
    ret = gp_camera_exit(camera, context);
    if (ret < GP_OK) {
      throw std::invalid_argument(QString("Can't unlock camera on port %1: %2")
        .arg(port)
        .arg(gp_result_as_string(ret))
        .toStdString());
    }
    deviceLocked = false;
    return true;
  }

  void Gphoto2Device::findConfigWidget(QString option, CameraWidget **rootconfig, CameraWidget **child) {
    int ret;
    // get root widget
    ret = gp_camera_get_config(camera, rootconfig, context);
    if (ret != GP_OK)
      throw runtime_error("Can't get rootconfig widget");
    ret = gp_widget_get_child_by_name(*rootconfig, option.toStdString().c_str(), child);
    if (ret != GP_OK)
      ret = gp_widget_get_child_by_label(*rootconfig, option.toStdString().c_str(), child);
    if (ret != GP_OK) {
      gp_widget_free(*rootconfig);
      throw runtime_error(QString("Config widget %1 not found").arg(option).toStdString());
    }
  }

  QString Gphoto2Device::getConfigValue(QString option) {
    CameraWidget *rootconfig, *child;
    int ret;

    findConfigWidget(option, &rootconfig, &child);

    // check widget type
    CameraWidgetType type;
    ret = gp_widget_get_type(child, &type);
    if (ret != GP_OK) {
      gp_widget_free(rootconfig);
      throw runtime_error(QString("Can't get type of widget %1").arg(option).toStdString());
    }
    if (type != GP_WIDGET_MENU && type != GP_WIDGET_RADIO && type != GP_WIDGET_TEXT) {
      gp_widget_free(rootconfig);
      throw runtime_error(QString("Unexpected widget type for %1").arg(option).toStdString());
    }
    char *choiceVal;
    ret = gp_widget_get_value(child, &choiceVal);
    if (ret != GP_OK) {
      gp_widget_free(rootconfig);
      throw runtime_error(QString("Can't get value for widget %1").arg(option).toStdString());
    }
    QString val = QString::fromUtf8(choiceVal);
    gp_widget_free(rootconfig);
    return val;
  }

  bool Gphoto2Device::isConfigRw(QString option) {
    CameraWidget *rootconfig, *child;
    int ret, ro;

    findConfigWidget(option, &rootconfig, &child);

    // check rw/ro
    ret = gp_widget_get_readonly(child, &ro);
    if (ret != GP_OK) {
      gp_widget_free(rootconfig);
      throw runtime_error(QString("Can't get readonly status for widget %1").arg(option).toStdString());
    }

    gp_widget_free(rootconfig);

    return (ro != 1);
  }

  QStringList Gphoto2Device::getConfigRadioChoices(QString option) {
    CameraWidget *rootconfig, *child;
    int ret;

    findConfigWidget(option, &rootconfig, &child);

    // check widget type
    CameraWidgetType type;
    ret = gp_widget_get_type(child, &type);
    if (ret != GP_OK) {
      gp_widget_free(rootconfig);
      throw runtime_error(QString("Can't get type of widget %1").arg(option).toStdString());
    }
    if (type != GP_WIDGET_MENU && type != GP_WIDGET_RADIO) {
      gp_widget_free(rootconfig);
      throw runtime_error(QString("Unexpected widget type for %1").arg(option).toStdString());
    }

    // retrieve choice list
    QStringList choices;
    int cnt = gp_widget_count_choices(child);
    int i;
    for (i = 0; i < cnt; i++) {
      const char *choice;
      ret = gp_widget_get_choice(child, i, &choice);
      if (ret < GP_OK) {
        gp_widget_free(rootconfig);
        throw runtime_error(QString("Can't get %1. option for widget %1").arg(i).arg(option).toStdString());
      }
      QString s = QString::fromUtf8(choice);
      choices.append(s);
    }

    gp_widget_free(rootconfig);

    return choices;
  }

  void Gphoto2Device::setConfig(QString option, QString value, bool exactMatch, CameraWidgetType expectedType) {
    CameraWidget *rootconfig, *child;
    int ret, ro;

    findConfigWidget(option, &rootconfig, &child);

    // check rw/ro
    ret = gp_widget_get_readonly(child, &ro);
    if (ret != GP_OK) {
      gp_widget_free(rootconfig);
      throw runtime_error(QString("Can't get readonly status for widget %1").arg(option).toStdString());
    }
    if (ro == 1) {
      gp_widget_free(rootconfig);
      throw runtime_error(QString("Widget %1 is read only").arg(option).toStdString());
    }

    // check widget type
    CameraWidgetType type;
    ret = gp_widget_get_type(child, &type);
    if (ret != GP_OK) {
      gp_widget_free(rootconfig);
      throw runtime_error(QString("Can't get type of widget %1").arg(option).toStdString());
    }
    if (type != expectedType) {
      gp_widget_free(rootconfig);
      throw runtime_error(QString("Unexpected widget type for %1").arg(option).toStdString());
    }

    // setup value
    bool success = false;
    QString error = QString("Failed to set the value of widget %1 to %2").arg(option).arg(value);
    switch (type) {

      case GP_WIDGET_TEXT:
      { /* char *		*/
        ret = gp_widget_set_value(child, value.toStdString().c_str());
        if (ret != GP_OK) {
          error = QString("Failed to set the value of text widget %1 to %2").arg(option).arg(value);
        } else {
          success = true;
        }
        break;
      }

      case GP_WIDGET_RANGE:
      { /* float		*/
        float f, t, b, s;
        bool ok;

        ret = gp_widget_get_range(child, &b, &t, &s);
        if (ret != GP_OK)
          break;
        f = value.toFloat(&ok);
        if (!ok) {
          error = QString("The passed value %1 is not a floating point value.").arg(value);
          break;
        }
        if ((f < b) || (f > t)) {
          error = QString("The passed value %1 is not within the expected range %2 - %3.").arg(f).arg(b).arg(t);
          break;
        }
        ret = gp_widget_set_value(child, &f);
        if (ret != GP_OK)
          error = QString("Failed to set the value of range widget %1 to %2").arg(option).arg(value);
        else
          success = true;
        break;
      }
      case GP_WIDGET_TOGGLE: /* int		*/
      case GP_WIDGET_DATE: /* int			*/
      {
        // TODO: port code from gphoto2, action.c
        error = QString("Setting of TOGGLE and DATE widget is not implemented");
        break;
      }

      case GP_WIDGET_MENU:
      case GP_WIDGET_RADIO:
      {
        // setup to choice that contains value
        int cnt = gp_widget_count_choices(child);
        int i;
        for (i = 0; i < cnt; i++) {
          const char *choice;
          ret = gp_widget_get_choice(child, i, &choice);
          if (ret < GP_OK) {
            error = QString("Can't get %1. option for widget %1").arg(i).arg(option);
            break;
          }
          QString s = QString::fromUtf8(choice);
          if ((exactMatch && s == value) || ((!exactMatch) && s.contains(value))) {
            //printf("setting %s to choice %d: %s\n", name, i, choice);
            ret = gp_widget_set_value(child, choice);
            if (ret == GP_OK) {
              success = true;
              break;
            } else {
              error = QString("%1 is not property of widget %2").arg(value).arg(option);
            }
          }
        }
        if (!success) {
          /* Lets just try setting the value directly, in case we have flexible setters,
           * like PTP shutterspeed. */
          ret = gp_widget_set_value(child, value.toStdString().c_str());
          if (ret == GP_OK)
            success = true;
        }

        break;
      }

        /* ignore: */
      case GP_WIDGET_WINDOW:
      case GP_WIDGET_SECTION:
      case GP_WIDGET_BUTTON:
      {
        error = QString("The %s widget is not configurable.").arg(option);
        break;
      }

    }

    if (success) {
      ret = gp_camera_set_config(camera, rootconfig, context);
      if (ret != GP_OK) {
        success = false;
        error = QString("Failed to set new configuration value %1 for configuration entry %2.")
          .arg(value).arg(option);
      }
    }

    gp_widget_free(rootconfig);

    if (!success)
      throw runtime_error(error.toStdString());
  }

  QList<ShutterSpeedChoice> Gphoto2Device::getShutterSpeedChoices() {
    try {
      bool shouldUnlock = lock();
      QString option = SHUTTERSPEED_CONFIG;
      if (!isConfigRw(option))
        return QList<ShutterSpeedChoice>();
      QList<ShutterSpeedChoice> result;
      QStringList strList = getConfigRadioChoices(option);
      for (QString s : strList) {
        result.append(ShutterSpeedChoice(s));
      }
      if (shouldUnlock)
        unlock();
      return result;
    } catch (std::exception &e) {
      return QList<ShutterSpeedChoice>();
    }
  }

  ShutterSpeedChoice Gphoto2Device::currentShutterSpeed() {
    try {
      bool shouldUnlock = lock();
      ShutterSpeedChoice res = ShutterSpeedChoice(getConfigValue(SHUTTERSPEED_CONFIG));
      if (shouldUnlock)
        unlock();
      return res;
    } catch (std::exception &e) {
      return ShutterSpeedChoice();
    }
  }

  bool Gphoto2Device::isBusy() {
    return deviceLocked;
  }

  /**
   * This method try to setup RAM storage in camera.
   * We download captured images, so we don't want to keep it in memory card...
   */
  bool Gphoto2Device::tryToSetRamStorage() {
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

    //const char *name = "capturetarget";
    try {
      setConfig(CAPTURETARGET_CONFIG, INTERNALRAM_VALUE, false, GP_WIDGET_RADIO);
      return true;
    } catch (std::exception &e) {
      return false;
    }
  }

  void Gphoto2Device::downloadAndEmitImage(CameraFilePath *filePath) {
    int ret;

    CameraFile* file;
    ret = gp_file_new(&file);
    ret = gp_camera_file_get(camera, filePath->folder, filePath->name, GP_FILE_TYPE_NORMAL, file, context);
    if (ret < GP_OK) {
      gp_file_free(file);
      throw std::runtime_error(QString("Failed to get file from camera %1: %2")
        .arg(model)
        .arg(gp_result_as_string(ret))
        .toStdString());
    }

    const char* data;
    unsigned long int size = 0;

    gp_file_get_data_and_size(file, &data, &size);

    QFileInfo fi(filePath->name);
    Magick::Blob b(data, size);
    emit imageCaptured(fi.suffix(), b, Magick::Geometry());

    gp_file_free(file);
  }

  void Gphoto2Device::deleteImage(CameraFilePath *path) {
    int ret;
    ret = gp_camera_file_delete(camera, path->folder, path->name, context);
    if (ret < GP_OK) {
      throw std::runtime_error(QString("Failed to delete file from camera %1: %2")
        .arg(model)
        .arg(gp_result_as_string(ret))
        .toStdString());
    }
  }

  void Gphoto2Device::waitAndHandleEvent(int waitMs, CameraEventType *evtype) {
    int ret;
    void *data = nullptr;
    ret = gp_camera_wait_for_event(camera, waitMs, evtype, &data, context);
    if (ret == GP_ERROR_NOT_SUPPORTED) {
      *evtype = GP_EVENT_TIMEOUT;
      usleep(waitMs * 1000);
      return;
    }
    if (ret != GP_OK)
      throw std::runtime_error("Error while waiting for camera event");

    CameraFilePath *path = (CameraFilePath *) data;
    switch (*evtype) {
      case GP_EVENT_TIMEOUT:
      case GP_EVENT_CAPTURE_COMPLETE:
        break;
      case GP_EVENT_FOLDER_ADDED:
        //printf (_("Event FOLDER_ADDED %s/%s during wait, ignoring.\n"), path->folder, path->name);
        free(data);
        break;
      case GP_EVENT_FILE_ADDED:
        qDebug() << "New image in camera " << path->folder << "/" << path->name;
        downloadAndEmitImage(path);
        if (deleteImageAfterDownload)
          deleteImage(path);
        free(data);
        /* result will fall through to final return */
        break;
      case GP_EVENT_UNKNOWN:
        free(data);
        break;
      default:
        //	printf (_("Unknown event type %d during bulb wait, ignoring.\n"), *type);
        break;
    }
  }

  void Gphoto2Device::pollingTimeout() {
    if (!deviceLocked) {
      throw std::logic_error("Polling without camera lock!");
      //return;
    }
    pollingScheduled = false;
    CameraEventType evtype;
    int pollingInterval = 200; // TODO: configurable
    waitAndHandleEvent(pollingInterval /* ms */, &evtype);
    if (evtype != GP_EVENT_TIMEOUT) {
      timer.start();
      //printf("Got event from camera\n");
      // we got some real event, poll immediately 
      pollingScheduled = true;
      QTimer::singleShot(0, this, SLOT(pollingTimeout()));
    } else {
      if (timer.elapsed() > 1500) { // TODO: configurable
        //printf("Stop polling %d\n", timer.elapsed());
        // if we don't get any event for 3000 ms, stop polling and exit camera
        unlock();
      } else {
        pollingScheduled = true;
        QTimer::singleShot(pollingInterval, this, SLOT(pollingTimeout()));
      }
    }
  }

  void Gphoto2Device::bulbWait(int bulbLengthMs) {

    // just wait specified time...
    QElapsedTimer timer;
    timer.start();

    while ((bulbLengthMs - timer.elapsed()) > 0) {

      int waittime = bulbLengthMs - timer.elapsed();
      CameraEventType evtype;
      waitAndHandleEvent(waittime, &evtype);
    }
  }

  void Gphoto2Device::capture(QTextStream *verboseOut, ShutterSpeedChoice shutterSpeed) {
    int ret;

    // init camera
    lock();

    //printf("capture\n");

    // try to setup RAM storage for captured image
    // if it fails, we should delete images in camera storage after download
    deleteImageAfterDownload = !tryToSetRamStorage();

    try {
      QString val = getConfigValue(BATTERYLEVEL_CONFIG);
      *verboseOut << "Battery level: " << val << endl;
    } catch (std::exception &e) {
      *verboseOut << "Can't read bettery level: " << e.what() << endl;
    }

    // Capture the frame from camera
    /* Now handle the different capture methods */
    if (shutterSpeed.isBulb() && shutterSpeed.toMs() > 0) {
      /* Bulb mode is special ... we enable it, wait disable it */
      setConfig(BULB_CONFIG, ON_VALUE, false, GP_WIDGET_RADIO); // TODO: is bulb radio?

      bulbWait(shutterSpeed.toMs());

      setConfig(BULB_CONFIG, OFF_VALUE, false, GP_WIDGET_RADIO); // TODO: is bulb radio?
    } else {
      if (shutterSpeed.toMs() > 0) {
        setConfig(SHUTTERSPEED_CONFIG, shutterSpeed.toString(), true, GP_WIDGET_RADIO);
      }

      CameraFilePath filePath;
      ret = gp_camera_capture(camera, GP_CAPTURE_IMAGE, &filePath, context);
      if (ret < GP_OK) {
        unlock();
        throw std::runtime_error(QString("Failed to capture frame with camera %1: %2")
          .arg(model)
          .arg(gp_result_as_string(ret))
          .toStdString());
      }
      downloadAndEmitImage(&filePath);
      if (deleteImageAfterDownload)
        deleteImage(&filePath);
      qDebug() << "Captured frame:" << filePath.folder << filePath.name;
    }

    // start event polling...
    // 
    // in case that capture produces more images (JPEG and RAW) 
    // or we trigger bulb capture, driver send us an event 
    // that new image was stored...
    timer.start();
    if (!pollingScheduled) {
      pollingScheduled = true;
      QTimer::singleShot(0, this, SLOT(pollingTimeout()));
    }

  }

  QString Gphoto2Device::toString() {
    return QString("%1\t\"%2\"").arg(port).arg(model);
  }

  QString Gphoto2Device::toShortString() {
    return model;
  }

  // QObject* Gphoto2Device::qObject() {
  //   return this;
  // }

  Gphoto2Device Gphoto2Device::operator=(const timelapse::Gphoto2Device& o) {
    if (deviceLocked)
      throw std::logic_error("Assing to locked camera is not implemented!");
    if (o.deviceLocked)
      throw std::logic_error("Assing of locked camera is not implemented!");

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

    gp_context_set_idle_func(context, 0, nullptr);
    gp_context_set_progress_funcs(context, 0, 0, 0, nullptr);
    gp_context_set_error_func(context, _gp_context_msg_func, errOut);
    gp_context_set_status_func(context, _gp_context_msg_func, verboseOut);
    gp_context_set_question_func(context, 0, nullptr);
    gp_context_set_cancel_func(context, 0, nullptr);
    gp_context_set_message_func(context, _gp_context_msg_func, verboseOut);

#ifdef gp_log_add_func
    gp_log_add_func(GPLogLevel::GP_LOG_VERBOSE, _gp_port_debug, verboseOut);
#endif

    return context;
  }

  void Gphoto2Device::releaseContext(GPContext *context) {
    gp_context_unref(context);
  }

  Gphoto2Device Gphoto2Device::createDevice(GPContext *context, QString port, QTextStream *verboseOut) {
    int p, r;
    GPPortInfo info;

    /* Create the list of ports and load it. */
    GPPortInfoList *portinfo_list = nullptr;
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
    Camera *camera = nullptr;
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
      throw std::invalid_argument(QString("Can't init camera on port %1: %2")
        .arg(port)
        .arg(gp_result_as_string(r))
        .toStdString());
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
      case GP_PORT_IP: typestr = "IP port";
        break; /**< \brief generic IP address port. */
    }
    *verboseOut << "Create camera device " << model << " on port " << path <<
      " (" << typestr << ")" << endl;


    // exit camera, we don't want hold camera connection for long time
    r = gp_camera_exit(camera, context);
    if (r < 0) {
      gp_camera_unref(camera);
      gp_port_info_list_free(portinfo_list);
      throw std::invalid_argument(QString("Can't release camera %1: %2")
        .arg(port)
        .arg(gp_result_as_string(r))
        .toStdString());
    }

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
    const char *model = nullptr, *port = nullptr;

    GPContext *context = Gphoto2Device::initContext(verboseOut, errOut);

    GPPortInfoList *portinfo_list = nullptr;
    CR(_get_portinfo_list(&portinfo_list));
    count = gp_port_info_list_count(portinfo_list);

    CameraList *list = nullptr;
    CR(gp_list_new(&list));

    CameraAbilitiesList *abilities_list = nullptr;
    CR(gp_abilities_list_new(&abilities_list));
    gp_abilities_list_load(abilities_list, context);
    gp_abilities_list_detect(abilities_list, portinfo_list, list, context);

    CL(count = gp_list_count(list), list);

    for (x = 0; x < count; x++) {
      CL(gp_list_get_name(list, x, &model), list);
      CL(gp_list_get_value(list, x, &port), list);

      try {
        Gphoto2Device d = createDevice(context, port, verboseOut);
        devices.append(d);
      } catch (std::exception &e) {
        *verboseOut << "GPhoto2 device " << port << " / " << model << " can't be used for capturing: " << QString::fromUtf8(e.what()) << endl;
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
