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
//#include <stdio.h>
//#include <stdlib.h>
//#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/ioctl.h>
//#include <sys/types.h>
#include <sys/time.h>
#include <sys/mman.h>
//#include <linux/videodev2.h>
#include <libv4l2.h>
//#include <libv4lconvert.h>


#include <QtCore/QObject>
#include <QtCore/QDebug>
#include <QtCore/QTimer>
#include <QtCore/QTextStream>
#include <QtCore/QByteArray>
#include <QtCore/QCoreApplication>
#include <QtCore/QCommandLineParser>
#include <QtCore/QProcess>

#include <QtCore/QDir>

#include "pipeline_cpt_v4l.h"
#include "pipeline_cpt_v4l.moc"



using namespace std;
using namespace timelapse;

namespace timelapse {

  V4LDevice::V4LDevice(QString dev) :
  initialized(false), dev(dev) {
  }

  V4LDevice::V4LDevice(const timelapse::V4LDevice& o) :
  initialized(o.initialized), dev(o.dev), v4lfmt(o.v4lfmt) {
  }

  V4LDevice::~V4LDevice() {
  }

  QString V4LDevice::toString() {
    return dev;
  }

  V4LDevice V4LDevice::operator=(const timelapse::V4LDevice& o) {
    initialized = o.initialized;
    dev = o.dev;
    v4lfmt = o.v4lfmt;
    return *this;
  }

  QList<V4LDevice> V4LDevice::listDevices(QTextStream *verboseOut, QDir devDir, int max) {
    QList<V4LDevice> result;

    for (int i = 0; i < max; i++) {
      QFileInfo fi(devDir, QString("video%1").arg(i));
      if (fi.exists()) {
        *verboseOut << "Probing v4l device " << fi.absoluteFilePath() << endl;
        V4LDevice d(fi.absoluteFilePath());
        try {
          d.initialize();
          result.append(d);
        } catch (std::exception &e) {
          *verboseOut << "Device " << fi.absoluteFilePath() << " can't be used for capturing." << endl;
        }
      }
    }

    return result;
  }

  void V4LDevice::ioctl(int fh, unsigned long int request, void *arg) {
    int r;
    do {
      r = v4l2_ioctl(fh, request, arg);
    } while (r == -1 && ((errno == EINTR) || (errno == EAGAIN)));

    if (r == -1) {
      throw runtime_error(QString("error %1, %2")
        .arg(errno)
        .arg(strerror(errno))
        .toStdString());
    }
  }

  int V4LDevice::open() {
    const char *dev_name = dev.toStdString().c_str();
    int fd = v4l2_open(dev_name, O_RDWR | O_NONBLOCK, 0);
    if (fd < 0) {
      throw runtime_error(QString("Cannot open device %1")
        .arg(dev)
        .toStdString());
    }
    return fd;
  }

  void V4LDevice::initialize() {
    if (initialized)
      return;

    CLEAR(v4lfmt);
    //CLEAR(dst);

    int fd = open();
    //v4lconvert_data *v4lcdt = v4lconvert_create(fd);

    try {
      // determine highest available resolution with RGB24 pixel format
      // v4l2-ctl --list-formats-ext

      struct v4l2_fmtdesc fmt;
      struct v4l2_frmsizeenum frmsize;

      CLEAR(fmt);
      CLEAR(frmsize);
      //struct v4l2_frmivalenum frmival;

      fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
      v4lfmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
      v4lfmt.fmt.pix.width = 0;
      v4lfmt.fmt.pix.height = 0;
      v4lfmt.fmt.pix.pixelformat = 0;
      //dst.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

      //dst.fmt.pix.field = V4L2_FIELD_INTERLACED;
      v4lfmt.fmt.pix.field = V4L2_FIELD_NONE;

      fmt.index = 0;
      while (v4l2_ioctl(fd, VIDIOC_ENUM_FMT, &fmt) >= 0) {
        frmsize.pixel_format = fmt.pixelformat;
        frmsize.index = 0;
        while (v4l2_ioctl(fd, VIDIOC_ENUM_FRAMESIZES, &frmsize) >= 0) {
          uint32_t w;
          uint32_t h;
          if (frmsize.type == V4L2_FRMSIZE_TYPE_DISCRETE) {
            w = frmsize.discrete.width;
            h = frmsize.discrete.height;
          } else if (frmsize.type == V4L2_FRMSIZE_TYPE_STEPWISE) {
            w = frmsize.stepwise.max_width;
            h = frmsize.stepwise.max_height;
          }

          if ((w > v4lfmt.fmt.pix.width && h > v4lfmt.fmt.pix.height && fmt.pixelformat == V4L2_PIX_FMT_RGB24)) {
            v4lfmt.fmt.pix.width = w;
            v4lfmt.fmt.pix.height = h;
            v4lfmt.fmt.pix.pixelformat = fmt.pixelformat;
          }
          frmsize.index++;
        }
        fmt.index++;
      }

      /*
      dst.fmt.pix.width = v4lfmt.fmt.pix.width;
      dst.fmt.pix.height = v4lfmt.fmt.pix.height; // try to maximum possible resolution
      dst.fmt.pix.pixelformat = V4L2_PIX_FMT_RGB24;

      // try format
      if (v4lconvert_try_format(v4lcdt, &dst, &src) != 0) {
        throw runtime_error(QString("Failed to setup format on device %s: %s")
          .arg(dev)
          .arg(v4lconvert_get_error_message(v4lcdt))
          .toStdString());
      }
       */
      // check format
      if (v4lfmt.fmt.pix.pixelformat != V4L2_PIX_FMT_RGB24) {
        throw runtime_error(QString("Device %1 didn't accept RGB24 format. Can't proceed.")
          .arg(dev)
          .toStdString());
      }

      //v4lconvert_destroy(v4lcdt);
      v4l2_close(fd);
    } catch (std::exception &e) {
      //v4lconvert_destroy(v4lcdt);
      v4l2_close(fd);
      throw e;
    }

    initialized = true;
  }

  Magick::Image V4LDevice::capture() {
    initialize();

    struct v4l2_buffer buf;
    struct v4l2_requestbuffers req;
    enum v4l2_buf_type type;
    fd_set fds;
    struct timeval tv;
    int r, fd = -1;
    unsigned int i, n_buffers;
    //char out_name[256];
    //FILE *fout;
    struct buffer *buffers;
    buffers = NULL;
    Magick::Image capturedImage;
    unsigned int warmupFrames = 20 ;// TODO: configurable

      fd = open();
    //v4lconvert_data *v4lcdt = v4lconvert_create(fd);
    try {
      //bool needsConversion = v4lconvert_needs_conversion(v4lcdt, &src, &dst) == 1;
      struct v4l2_format sfmt;
      memcpy(&sfmt, &v4lfmt, sizeof (struct v4l2_format));
      V4LDevice::ioctl(fd, VIDIOC_S_FMT, &sfmt);

      // check format (driver can change it if something is not supported)
      //if (memcmp(&sfmt, &v4lfmt, sizeof(struct v4l2_format)) != 0) {
      if (v4lfmt.type != sfmt.type
        || v4lfmt.fmt.pix.width != sfmt.fmt.pix.width
        || v4lfmt.fmt.pix.height != sfmt.fmt.pix.height
        || v4lfmt.fmt.pix.pixelformat != sfmt.fmt.pix.pixelformat
        || v4lfmt.fmt.pix.field != sfmt.fmt.pix.field) {

        throw runtime_error(QString("Device %1 didn't accept requiered format.")
          .arg(dev)
          .toStdString());
      }
      /*
              if ((fmt.fmt.pix.width != 640) || (fmt.fmt.pix.height != 480))
                      printf("Warning: driver is sending image at %dx%d\n",
                              fmt.fmt.pix.width, fmt.fmt.pix.height);
       */

      CLEAR(req);
      req.count = 2;
      req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
      req.memory = V4L2_MEMORY_MMAP;
      V4LDevice::ioctl(fd, VIDIOC_REQBUFS, &req);

      buffers = new buffer[req.count];
      for (n_buffers = 0; n_buffers < req.count; ++n_buffers) {
        CLEAR(buf);

        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index = n_buffers;

        V4LDevice::ioctl(fd, VIDIOC_QUERYBUF, &buf);

        buffers[n_buffers].length = buf.length;
        buffers[n_buffers].start = v4l2_mmap(NULL, buf.length,
          PROT_READ | PROT_WRITE, MAP_SHARED,
          fd, buf.m.offset);

        if (MAP_FAILED == buffers[n_buffers].start) {
          perror("mmap");
          exit(EXIT_FAILURE);
        }
      }

      for (i = 0; i < n_buffers; ++i) {
        CLEAR(buf);
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index = i;
        V4LDevice::ioctl(fd, VIDIOC_QBUF, &buf);
      }
      type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

      V4LDevice::ioctl(fd, VIDIOC_STREAMON, &type);
      for (i = 0; i <= warmupFrames; i++) {
        do {
          FD_ZERO(&fds);
          FD_SET(fd, &fds);

          /* Timeout. */
          tv.tv_sec = 2;
          tv.tv_usec = 0;

          r = select(fd + 1, &fds, NULL, NULL, &tv);
        } while ((r == -1 && (errno = EINTR)));
        if (r == -1) {
          perror("select");
          //cleanup(errno); 
          // TODO: exception
          return Magick::Image();
        }

        CLEAR(buf);
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        V4LDevice::ioctl(fd, VIDIOC_DQBUF, &buf);

        /*
        sprintf(out_name, "out%03d.ppm", i);
        fout = fopen(out_name, "w");
        if (!fout) {
          perror("Cannot open image");
          exit(EXIT_FAILURE);
        }
        fprintf(fout, "P6\n%d %d 255\n",
          v4lfmt.fmt.pix.width, v4lfmt.fmt.pix.height);
        fwrite(buffers[buf.index].start, buf.bytesused, 1, fout);
        fclose(fout);
         */
        if (i == warmupFrames) { // last frame, we capture it
          Magick::Blob oblob(buffers[buf.index].start, buf.bytesused);

          Magick::Geometry g(v4lfmt.fmt.pix.width, v4lfmt.fmt.pix.height);
          capturedImage.size(g);
          capturedImage.depth(8);
          capturedImage.magick("RGB");
          capturedImage.read(oblob);
        }

        V4LDevice::ioctl(fd, VIDIOC_QBUF, &buf);
      }

      type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
      V4LDevice::ioctl(fd, VIDIOC_STREAMOFF, &type);
      for (i = 0; i < n_buffers; ++i)
        v4l2_munmap(buffers[i].start, buffers[i].length);

      delete[] buffers;
      //v4lconvert_destroy(v4lcdt);
      v4l2_close(fd);
    } catch (std::exception &e) {
      // cleanup
      if (buffers != NULL)
        delete[] buffers;
      //v4lconvert_destroy(v4lcdt);
      v4l2_close(fd);
      //QTextStream(stdout) << e.what() << endl;
      throw runtime_error(e.what()); // rethrow
    }

    return capturedImage;
  }

}