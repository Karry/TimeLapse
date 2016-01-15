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

  void _get_portinfo_list(GPPortInfoList **portinfo_list) {
    int count, result;
    GPPortInfoList *list = NULL;

    if (*portinfo_list)
      return;

    if (gp_port_info_list_new(&list) < GP_OK)
      return;
    result = gp_port_info_list_load(list);
    if (result < 0) {
      gp_port_info_list_free(list);
      return;
    }
    count = gp_port_info_list_count(list);
    if (count < 0) {
      gp_port_info_list_free(list);
      return;
    }
    *portinfo_list = list;
    return;
  }

  Gphoto2Device::Gphoto2Device(GPContext *context, QString port, QString model) :
  context(context), port(port), model(model) {

    gp_context_ref(context);
  }

  Gphoto2Device::Gphoto2Device(const timelapse::Gphoto2Device& other) :
  context(other.context), port(other.port), model(other.model) {

    gp_context_ref(context);
  }

  Gphoto2Device::~Gphoto2Device() {

    gp_context_unref(context);
  }

  Magick::Image Gphoto2Device::capture() {
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


    //capture_generic(GP_CAPTURE_IMAGE, "/tmp/gphoto2.out", 1);
    return Magick::Image();
  }

  QString Gphoto2Device::toString() {
    return QString("%1\t\"%2\"").arg(port).arg(model);
  }

  Gphoto2Device Gphoto2Device::operator=(const timelapse::Gphoto2Device& o) {
    gp_context_unref(context);

    context = o.context;
    port = o.port;
    model = o.model;

    gp_context_ref(context);

    return *this;
  }

  GPContext * Gphoto2Device::initContext() {
    GPContext *context = gp_context_new();
    ALLOC_CHECK(context);

    gp_context_set_idle_func(context, 0, NULL);
    gp_context_set_progress_funcs(context, 0, 0, 0, NULL);
    gp_context_set_error_func(context, 0, NULL);
    gp_context_set_status_func(context, 0, NULL);
    gp_context_set_question_func(context, 0, NULL);
    gp_context_set_cancel_func(context, 0, NULL);
    gp_context_set_message_func(context, 0, NULL);

    return context;
  }

  QList<Gphoto2Device> Gphoto2Device::listDevices() {
    QList<Gphoto2Device> devices;

    int x, count;
    const char *model = NULL, *port = NULL;

    GPContext *context = Gphoto2Device::initContext();

    GPPortInfoList *portinfo_list = NULL;
    _get_portinfo_list(&portinfo_list);
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


      /* Search our port in the list. */
      /* NOTE: This call can modify "il" for regexp matches! */
      /*
  p = gp_port_info_list_lookup_path (params->portinfo_list, verified_port);

  switch (p) {
  case GP_ERROR_UNKNOWN_PORT:
    fprintf (stderr, _("The port you specified "
      "('%s') can not be found. Please "
      "specify one of the ports found by "
      "'gphoto2 --list-ports' and make "
      "sure the spelling is correct "
      "(i.e. with prefix 'serial:' or 'usb:')."),
        verified_port);
    break;
  default:
    break;
  }

  // Get info about our port. 
  r = gp_port_info_list_get_info (params->portinfo_list, p, &info);
  if (r < 0)
    return (r);
      
      //GPContext *m_context;
      Camera _camera;
      CLEAR(_camera);
      Camera *camera = &_camera;
      
      int ret = gp_camera_new(&camera);
      camera->port
      ret = gp_camera_init(camera, p->context);
       */


      Gphoto2Device d(context, port, model);
      devices.append(d);
      d.capture();
      //printf("%-30s %-16s\n", model, port);
    }

    gp_list_free(list);
    gp_abilities_list_free(abilities_list);
    gp_context_unref(context);

    return devices;
  }
}
