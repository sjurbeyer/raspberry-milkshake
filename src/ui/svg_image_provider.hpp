#pragma once

#include <QQuickImageProvider>

namespace rm {

// Renders SVG files for QML as image://svg/<url-encoded qrc: or file: URL>. It draws with
// QSvgRenderer directly, so it does not depend on Qt's SVG image-format plugin, which not every
// distribution package ships.
class SvgImageProvider : public QQuickImageProvider {
 public:
  SvgImageProvider();
  QImage requestImage(const QString& id, QSize* size, const QSize& requested_size) override;
};

}  // namespace rm
