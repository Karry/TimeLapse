#include <TimeLapse/pipeline_deflicker.h>

#include <catch2/catch_test_macros.hpp>

#include <QtCore/QCoreApplication>

TEST_CASE("deflicker debug image composes original and corrected images") {
  Magick::InitializeMagick("deflicker_composition_test");
  int argc = 1;
  char applicationName[] = "deflicker_composition_test";
  char *argv[] = {applicationName, nullptr};
  QCoreApplication application(argc, argv);
  QString log;
  QTextStream verboseOutput(&log);

  for (size_t width : {8u, 9u}) {
    const size_t height = 5;
    Magick::Image original(Magick::Geometry(width, height), Magick::Color("black"));
    for (size_t y = 0; y < height; y++) {
      for (size_t x = 0; x < width; x++) {
        original.pixelColor(x, y, Magick::ColorRGB(
          0.1 + 0.05 * x, 0.2 + 0.1 * y, 0.3 + 0.02 * (x + y)));
      }
    }

    std::vector<std::pair<Magick::Color, size_t>> histogram;
    Magick::colorHistogram(&histogram, original);
    InputImageInfo info;
    info.luminance = timelapse::ComputeLuminance::computeLuminance(histogram);
    info.luminanceChange = info.luminance * 0.1;

    Magick::Image corrected;
    Magick::Image debug;
    bool correctedEmitted = false;
    bool debugEmitted = false;
    timelapse::AdjustLuminance normalHandler(&verboseOutput, false);
    timelapse::AdjustLuminance debugHandler(&verboseOutput, true);
    QObject::connect(&normalHandler, &timelapse::ImageHandler::inputImg,
      [&](InputImageInfo, Magick::Image image) {
        corrected = image;
        correctedEmitted = true;
      });
    QObject::connect(&debugHandler, &timelapse::ImageHandler::inputImg,
      [&](InputImageInfo, Magick::Image image) {
        debug = image;
        debugEmitted = true;
      });
    normalHandler.onInputImg(info, original);
    debugHandler.onInputImg(info, original);

    INFO("image width: " << width);
    REQUIRE(correctedEmitted);
    REQUIRE(debugEmitted);
    REQUIRE(debug.columns() == width);
    REQUIRE(debug.rows() == height);

    for (size_t y = 0; y < height; y++) {
      for (size_t x = 0; x < width; x++) {
        const Magick::Image &expected = x < width / 2 ? original : corrected;
        CAPTURE(x, y);
        REQUIRE(debug.pixelColor(x, y) == expected.pixelColor(x, y));
      }
    }
  }
}
