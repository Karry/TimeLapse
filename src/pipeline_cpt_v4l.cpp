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


#include <fcntl.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <sys/mman.h>
#include <linux/videodev2.h>
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
  initialized(o.initialized), dev(o.dev), capability(o.capability), v4lfmt(o.v4lfmt) {
  }

  V4LDevice::~V4LDevice() {
  }

  QString V4LDevice::toString() {
    initialize();

    uint32_t v = capability.version;
    return QString("%1 \t\"%2\" @ %3 (%4 %5.%6.%7)")
      .arg(dev)
      .arg(QString::fromLatin1((char *) capability.card, sizeof (capability.card)))
      .arg(QString::fromLatin1((char *) capability.bus_info, sizeof (capability.bus_info)))
      .arg(QString::fromLatin1((char *) capability.driver, sizeof (capability.driver)))
      .arg((v >> 16) & 0xff).arg((v >> 8) & 0xff).arg(v & 0xff);
  }

  V4LDevice V4LDevice::operator=(const timelapse::V4LDevice& o) {
    initialized = o.initialized;
    dev = o.dev;
    v4lfmt = o.v4lfmt;
    capability = o.capability;
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

    int fd = open();

    try {

      CLEAR(capability);
      // get device informations
      ioctl(fd, VIDIOC_QUERYCAP, &capability);

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
    struct buffer *buffers;
    buffers = NULL;
    Magick::Image capturedImage;
    unsigned int warmupFrames = 20; // TODO: configurable

    fd = open();
    try {
      struct v4l2_format sfmt;
      memcpy(&sfmt, &v4lfmt, sizeof (struct v4l2_format));
      V4LDevice::ioctl(fd, VIDIOC_S_FMT, &sfmt);

      // check format (driver can change it if something is not supported)
      if (v4lfmt.type != sfmt.type
        || v4lfmt.fmt.pix.width != sfmt.fmt.pix.width
        || v4lfmt.fmt.pix.height != sfmt.fmt.pix.height
        || v4lfmt.fmt.pix.pixelformat != sfmt.fmt.pix.pixelformat
        || v4lfmt.fmt.pix.field != sfmt.fmt.pix.field) {

        throw runtime_error(QString("Device %1 didn't accept requiered format.")
          .arg(dev)
          .toStdString());
      }

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
          throw runtime_error("Failed to read from device");
        }

        CLEAR(buf);
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        V4LDevice::ioctl(fd, VIDIOC_DQBUF, &buf);

        if (i == warmupFrames) { // last frame, we capture it
          Magick::Blob oblob(buffers[buf.index].start, buf.bytesused);

          Magick::Geometry g(v4lfmt.fmt.pix.width, v4lfmt.fmt.pix.height);
          capturedImage.read(oblob, g, 8, "RGB");


          QDateTime now = QDateTime::currentDateTime();
          QString exifDateTime = now.toString("yyyy:MM:dd HH:mm:ss");\
          
          // ImageMagick don't support writing of exif data
          // TODO: setup exif timestamp correctly
          capturedImage.attribute("EXIF:DateTime", exifDateTime.toStdString());
          //capturedImage.defineValue("EXIF", "DateTime", exifDateTime.toStdString());

        }

        V4LDevice::ioctl(fd, VIDIOC_QBUF, &buf);
      }

      type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
      V4LDevice::ioctl(fd, VIDIOC_STREAMOFF, &type);
      for (i = 0; i < n_buffers; ++i)
        v4l2_munmap(buffers[i].start, buffers[i].length);

      delete[] buffers;
      v4l2_close(fd);
    } catch (std::exception &e) {
      // cleanup
      if (buffers != NULL)
        delete[] buffers;
      v4l2_close(fd);
      //QTextStream(stdout) << e.what() << endl;
      throw runtime_error(e.what()); // rethrow
    }

    return capturedImage;
  }

}
