# TimeLapse

Goal of this project is to create set of tools for creating timelapse videos.
From capturing series of images by some camera (V4L or gphoto2), 
process them and assembly "time-lapse" video from them.

## Build

This project is written in C++ with usage of QT framework and some other libraries 
(ImageMagic, VidStab, V4L, GPHOTO2). 
I invest effort to create tools multiplatform, but I tested it on Ubuntu Linux only. 
Moreover, V4L is Linux-only api.
It uses avconv or ffmpeg to build final videos.

#### Ubuntu

Install build tools and libraries:

```
sudo apt-get install cmake cmake-data build-essential libmagick++-dev qtbase5-dev \
  libav-tools git libv4l-dev
```

build & install libvidstab*:
```
git clone -b improvements https://github.com/Karry/vid.stab.git vidstab
cd vidstab
cmake .
make -j `nproc`
sudo make install
sudo ldconfig
cd ..
```
* This fork of vid.stab contains some fixes for processing RGB image that are not merged to 
to upstream yet ( https://github.com/georgmartius/vid.stab )

build & install TimeLapse tools:

```
git clone https://github.com/Karry/TimeLapse.git timelapse-tools
cd timelapse-tools
cmake . 
make -j `nproc`
sudo make install
sudo ldconfig
```

If you want help me with bug fixing, recompile tools with `cmake -DCMAKE_BUILD_TYPE=DebugFull`, 
reproduce bug inside `gdb` and post stacktrace to github issue...

## TimeLapse Tools

 - *Capture* - 
    Tool for capture sequence of images from digital camera (V4L or GPhoto2 API).
    This tool support automatic shutter speed configuration with some gphoto2 cameras.
    It is useful when light conditions are changing during timelapse capturing.

 - *Deflickering* - 
    Tool for "average" luminance of series of images.

 - *Stabilize* - 
    Tool for stabilize movements in sequence of images.
    It uses vid.stab library for stabilizing. Results are not perfect, 
    but it depens on conditions. Just try it :-)

 - *Assembly* - 
    Tool for build timelapse video from series of images. It support deflickering 
    as part of processing pipeline. Final video assembly is processed by 
    [avconv](https://libav.org/avconv.html) or [ffmpeg](https://www.ffmpeg.org/ffmpeg.html) tool.

## External tools

 - *timelapse-deflicker* - 
    Simple script to deflicker images taken for timelapses 
    https://github.com/cyberang3l/timelapse-deflicker

 - *gphoto2* -
    The gphoto2 commandline tool for accessing and controlling digital cameras. 
    https://github.com/gphoto/gphoto2
