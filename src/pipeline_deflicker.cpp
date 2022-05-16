/*
 *   Copyright (C) 2015 Lukáš Karas <lukas.karas@centrum.cz>
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

#include "pipeline_deflicker.h"
#include "pipeline_deflicker.moc"

#include "timelapse.h"

#include <QtCore/QTextStream>
#include <QtCore/QString>

#include <Magick++.h>
#include <ImageMagick-6/Magick++/Color.h>

#include <exception>
#include <vector>
#include <utility>

using namespace std;
using namespace timelapse;


namespace timelapse {

  ComputeLuminance::ComputeLuminance(QTextStream *_verboseOutput) :
  verboseOutput(_verboseOutput) {
  }

  double ComputeLuminance::computeLuminance(
    const std::vector<std::pair<Magick::Color, size_t>> &histogram,
    double gamma) {

    double powArg = 1.0 / gamma;
    unsigned long sumR = 0;
    unsigned long sumG = 0;
    unsigned long sumB = 0;
    unsigned long pixelCount = 0;
    for (std::pair<Magick::Color, size_t> p : histogram) {
      pixelCount += p.second;
      sumR += p.second * Magick::Color::scaleDoubleToQuantum(
        std::pow(Magick::Color::scaleQuantumToDouble(p.first.redQuantum()), powArg));
      sumG += p.second * Magick::Color::scaleDoubleToQuantum(
        std::pow(Magick::Color::scaleQuantumToDouble(p.first.greenQuantum()), powArg));
      sumB += p.second * Magick::Color::scaleDoubleToQuantum(
        std::pow(Magick::Color::scaleQuantumToDouble(p.first.blueQuantum()), powArg));
    }

    // We use the following formula to get the perceived luminance.
    return 0.299 * ((double) sumR / (double) pixelCount)
      + 0.587 * ((double) sumG / (double) pixelCount)
      + 0.114 * ((double) sumB / (double) pixelCount);
  }

  void ComputeLuminance::onInputImg(InputImageInfo info, Magick::Image img) {

    Magick::Image::ImageStatistics stat;
    img.statistics(&stat);

    // We use the following formula to get the perceived luminance.
    info.luminance = 0.299 * stat.red.mean + 0.587 * stat.green.mean + 0.114 * stat.blue.mean;

    *verboseOutput << info.fileInfo().filePath()
      << " luminance: " << info.luminance
      << endl;

    emit inputImg(info, img);
  }

  ComputeAverageLuminance::ComputeAverageLuminance(QTextStream *_verboseOutput) :
  verboseOutput(_verboseOutput), inputs() {
  }

  void ComputeAverageLuminance::onInput(InputImageInfo info) {
    inputs.append(info);
  }

  void ComputeAverageLuminance::onLast() {
    double sumLumi = 0;
    for (InputImageInfo i : inputs) {
      sumLumi += i.luminance;
    }
    double avgLumi = sumLumi / inputs.size();
    *verboseOutput << "Average luminance: " << avgLumi << endl;

    for (InputImageInfo i : inputs) {
      i.luminanceChange = avgLumi - i.luminance;
      emit input(i);
    }
    emit last();
  }

  WMALuminance::WMALuminance(QTextStream *_verboseOutput, size_t _count) :
  count(_count), verboseOutput(_verboseOutput), inputs() {
    if (count < 1)
      throw std::logic_error("Count for weighted moving average have to be greater than 0.");
  }

  void WMALuminance::onInput(InputImageInfo info) {
    inputs.append(info);
  }

  void WMALuminance::onLast() {
    std::list<double> queue;
    for (InputImageInfo in : inputs) {
      queue.push_back(in.luminance);
      if (queue.size() > count)
        queue.pop_front();
      double sum = 0;
      double sumWeight = 0;
      size_t i = 1;
      for (double d : queue) {
        double w = (double) i / (double) queue.size();
        sum += w * d;
        sumWeight += w;
        i++;
      }

      in.luminanceChange = (sum / sumWeight) - in.luminance;
      emit input(in);
    }
    emit last();
  }

  AdjustLuminance::AdjustLuminance(QTextStream *_verboseOutput, bool _debugView) :
  verboseOutput(_verboseOutput), debugView(_debugView) {
  }

  void AdjustLuminance::onInputImg(InputImageInfo info, Magick::Image img) {
    std::vector<std::pair < Magick::Color, size_t>> histogram;
    Magick::colorHistogram(&histogram, img);
    Magick::Image original = img;
    /* gamma correction rules:
     * http://www.imagemagick.org/Usage/transform/#evaluate_pow
     * 
     *  updatedColor = color ^ (1 / gamma)
     *  gamma = 1 / log(color, updatedColor)
     *  gamma = 1 / (log(updatedColor) / log(color))
     * 
     * We can't compute gamma just from current and target luminance, 
     * but we can estimate it. 
     * 
     * We can compute expected luminance after gamma 
     * correction from image histogram, it is relatively fast. 
     * So we iterate gamma estimation ten times. Results are relatively good.
     */
    double gamma = 1.0;
    double targetLuminance = info.luminanceChange + info.luminance;
    double expectedLuminance = info.luminance;

    for (int iteration = 0; iteration < 10; iteration++) {

      gamma *= 1 / (
        std::log(Magick::Color::scaleQuantumToDouble(targetLuminance)) /
        std::log(Magick::Color::scaleQuantumToDouble(expectedLuminance)));

      expectedLuminance = ComputeLuminance::computeLuminance(histogram, gamma);

      *verboseOutput << QString("%1 iteration %2 changing gamma to %3 (expected luminance: %4, target %5, abs(diff) %6)")
        .arg(info.fileInfo().filePath())
        .arg(iteration)
        .arg(gamma)
        .arg(expectedLuminance)
        .arg(targetLuminance)
        .arg(std::abs(expectedLuminance - targetLuminance))
        << endl;
    }

    img.gamma(gamma);
    if (debugView) {
      original.transform(
        Magick::Geometry(original.columns(), original.rows()),
        Magick::Geometry(original.columns() / 2, original.rows())
        );
      img.composite(original, 0, 0, Magick::DissolveCompositeOp);
    }
    //img.brightnessContrast(brighnessChange, /* contrast */0.0);
    emit inputImg(info, img);
  }

}

