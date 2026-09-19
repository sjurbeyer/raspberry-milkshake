#include <catch2/catch_test_macros.hpp>

#include <QImage>
#include <QUrl>

#include "ui/svg_image_provider.hpp"

using namespace rm;

namespace {

QString id_for(const QString& file) { return QString::fromLatin1(QUrl::toPercentEncoding(QUrl::fromLocalFile(file).toString())); }

const QString kCover = QString(RM_SOURCE_DIR) + "/examples/collections/night-drive/cover.svg";

}  // namespace

TEST_CASE("the SVG provider renders a cover at the requested size", "[ui][svg]") {
  SvgImageProvider provider;
  QSize natural;
  const QImage image = provider.requestImage(id_for(kCover), &natural, QSize(128, 128));
  REQUIRE_FALSE(image.isNull());
  REQUIRE(image.size() == QSize(128, 128));
  REQUIRE(natural == QSize(1024, 1024));
  REQUIRE(qAlpha(image.pixel(0, 0)) == 255);  // the cover background is opaque
  const QRgb centre = image.pixel(64, 64);    // the white dot in the middle of every cover
  REQUIRE(qRed(centre) > 200);
  REQUIRE(qGreen(centre) > 200);
  REQUIRE(qBlue(centre) > 200);
}

TEST_CASE("the SVG provider uses the natural size when none is requested", "[ui][svg]") {
  SvgImageProvider provider;
  QSize natural;
  const QImage image = provider.requestImage(id_for(kCover), &natural, QSize());
  REQUIRE(image.size() == QSize(1024, 1024));
}

TEST_CASE("the SVG provider returns nothing for a missing or broken file", "[ui][svg]") {
  SvgImageProvider provider;
  QSize natural;
  REQUIRE(provider.requestImage(id_for("/no/such/file.svg"), &natural, QSize(32, 32)).isNull());
  REQUIRE(provider.requestImage(id_for(QString(RM_SOURCE_DIR) + "/examples/collections/night-drive/collection.toml"),
                                &natural, QSize(32, 32))
              .isNull());
}
