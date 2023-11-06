
## TimeLapse Tools

Goal of this project is to create set of tools for creating timelapse videos.
From capturing series of images by some camera (V4L, GPhoto2 or Qt), 
process them and assembly "time-lapse" video from them.

 - *[Capture](https://github.com/Karry/TimeLapse/wiki/Capture)* - 
    Tool for capture sequence of images from digital camera (V4L, GPhoto2 or Qt Camera API).
    This tool support automatic shutter speed configuration with some gphoto2 cameras.
    It is useful when light conditions are changing during timelapse capturing.

 - *[Deflickering](https://github.com/Karry/TimeLapse/wiki/Deflickering)* - 
    Tool for "average" luminance of series of images.

 - *[Stabilize](https://github.com/Karry/TimeLapse/wiki/Stabilize)* - 
    Tool for stabilize movements in sequence of images.
    It is using [vid.stab](https://github.com/georgmartius/vid.stab) library for stabilizing. 
    Results are not perfect, but it depends on conditions. Just try it :-)

 - *[Assembly](https://github.com/Karry/TimeLapse/wiki/Assembly)* - 
    Tool for build timelapse video from series of images. It support deflickering 
    as part of processing pipeline. Final video assembly is processed by 
    [avconv](https://libav.org/avconv.html) or [ffmpeg](https://www.ffmpeg.org/ffmpeg.html) tool.

## Result examples

All captured with Nikon D5100, raw processed by Rawtherapee, then assembled by TimeLapse.

[![Aurora behind clouds](https://img.youtube.com/vi/XsykUYhzCsE/0.jpg)](https://www.youtube.com/watch?v=XsykUYhzCsE)
[![Night timelapse](https://img.youtube.com/vi/mv7ci8BZZr8/0.jpg)](https://www.youtube.com/watch?v=mv7ci8BZZr8)
[![Grant Canyon sunset](https://img.youtube.com/vi/ugQ-LHx41fg/0.jpg)](https://www.youtube.com/watch?v=ugQ-LHx41fg)

## Build

This project is written in C++ with usage of QT framework and some other libraries 
(ImageMagic, VidStab, V4L, GPHOTO2, exif). 
I invest effort to create tools multiplatform, but I tested it on Ubuntu Linux only. 
Moreover, V4L is Linux-only api.
It uses avconv or ffmpeg to build final videos.

#### Ubuntu

Install build tools and libraries:

```
sudo apt-get install cmake cmake-data build-essential libmagick++-dev qtbase5-dev qtmultimedia5-dev \
  ffmpeg git libv4l-dev libgphoto2-dev
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
mkdir -p build && cd build
cmake ..
make -j $(nproc)
sudo make install
sudo ldconfig
```

If you want help me with bug fixing, recompile tools with `cmake -DCMAKE_BUILD_TYPE=DebugFull`, 
reproduce bug inside `gdb` and post stacktrace to github issue...

## External tools

 - *timelapse-deflicker* - 
    Simple script to deflicker images taken for timelapses 
    https://github.com/cyberang3l/timelapse-deflicker

 - *gphoto2* -
    The gphoto2 commandline tool for accessing and controlling digital cameras. 
    https://github.com/gphoto/gphoto2
