/*
 *   Copyright (C) 2022 Lukáš Karas <lukas.karas@centrum.cz>
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

#include <Magick++.h>
#include <Magick++/Color.h>
#include <Magick++/Image.h>

namespace timelapse {
// methods for Magick++ 6 vs. 7 compatibility

inline Magick::Quantum scaleDoubleToQuantum(double d) {
#if MagickLibVersion < 0x700
  return Magick::Color::scaleDoubleToQuantum(d);
#else
  return static_cast<Magick::Quantum>(d*QuantumRange);
#endif
}

inline double scaleQuantumToDouble(Magick::Quantum q) {
#if MagickLibVersion < 0x700
  return Magick::Color::scaleQuantumToDouble(q);
#else
  return static_cast<double>(q)/QuantumRange;
#endif
}

inline double redDouble(const Magick::Color &color) {
#if MagickLibVersion < 0x700
  return scaleQuantumToDouble(color.redQuantum());
#else
  return scaleQuantumToDouble(color.quantumRed());
#endif
}

inline double greenDouble(const Magick::Color &color) {
#if MagickLibVersion < 0x700
  return scaleQuantumToDouble(color.greenQuantum());
#else
  return scaleQuantumToDouble(color.quantumGreen());
#endif
}

inline double blueDouble(const Magick::Color &color) {
#if MagickLibVersion < 0x700
  return scaleQuantumToDouble(color.blueQuantum());
#else
  return scaleQuantumToDouble(color.quantumBlue());
#endif
}

inline void imageOpacity(Magick::Image &image, double opacity) {
#if MagickLibVersion < 0x700
  image.opacity(scaleDoubleToQuantum(opacity));
#else
  image.alpha(static_cast<unsigned int>(scaleDoubleToQuantum(opacity)));
#endif
}

}
