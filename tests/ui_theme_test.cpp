#include <catch2/catch_test_macros.hpp>

#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>

namespace {

const QString kRoot = RM_SOURCE_DIR;

QByteArray read(const QString& relative) {
  QFile file(kRoot + "/" + relative);
  REQUIRE(file.open(QIODevice::ReadOnly));
  return file.readAll();
}

QString colorIn(const QString& qml, const QString& name) {
  const QRegularExpression re(QString("property color %1: \"(#[0-9A-Fa-f]{6})\"").arg(name));
  const auto match = re.match(qml);
  return match.hasMatch() ? match.captured(1).toUpper() : QString();
}

int intIn(const QString& qml, const QString& name) {
  const QRegularExpression re(QString("property int %1: (\\d+)").arg(name));
  const auto match = re.match(qml);
  return match.hasMatch() ? match.captured(1).toInt() : -1;
}

}  // namespace

// Theme.qml is hand-written; the tokens file is the canonical source, so keep them identical.
TEST_CASE("the QML theme matches the brand tokens", "[ui][theme]") {
  const QString qml = QString::fromUtf8(read("src/ui/qml/Theme.qml"));
  const QJsonObject tokens =
      QJsonDocument::fromJson(read("docs/raspberry-milkshake-visual-profile/raspberry-milkshake-brand/tokens/raspberry-milkshake.tokens.json"))
          .object();
  REQUIRE_FALSE(tokens.isEmpty());

  const QJsonObject colors = tokens["color"].toObject();
  const std::pair<const char*, const char*> color_names[] = {
      {"canvas", "canvas"},        {"surface", "surface"},         {"surface-raised", "surfaceRaised"},
      {"border", "border"},        {"text", "text"},               {"text-muted", "textMuted"},
      {"raspberry", "raspberry"},  {"milk", "milk"},               {"raspberry-soft", "raspberrySoft"},
      {"signal", "signal"},        {"focus", "focus"}};
  for (const auto& [token, property] : color_names) {
    INFO(token);
    REQUIRE(colors.contains(token));
    REQUIRE(colorIn(qml, property) == colors[token].toObject()["$value"].toString().toUpper());
  }

  const QJsonObject radius = tokens["radius"].toObject();
  REQUIRE(intIn(qml, "radiusControl") == radius["control"].toObject()["$value"].toString().remove("px").toInt());
  REQUIRE(intIn(qml, "radiusPanel") == radius["panel"].toObject()["$value"].toString().remove("px").toInt());
  REQUIRE(intIn(qml, "radiusApp") == radius["app"].toObject()["$value"].toString().remove("px").toInt());

  const QJsonObject duration = tokens["duration"].toObject();
  REQUIRE(intIn(qml, "durationFast") == duration["fast"].toObject()["$value"].toString().remove("ms").toInt());
  REQUIRE(intIn(qml, "durationBase") == duration["base"].toObject()["$value"].toString().remove("ms").toInt());
  REQUIRE(intIn(qml, "durationSlow") == duration["slow"].toObject()["$value"].toString().remove("ms").toInt());

  const QJsonObject space = tokens["space"].toObject();
  for (const char* n : {"1", "2", "3", "4", "6", "8"}) {
    INFO(n);
    REQUIRE(intIn(qml, QString("space") + n) == space[n].toObject()["$value"].toString().remove("px").toInt());
  }
}

TEST_CASE("touch targets and type are sized for the 7 inch panel", "[ui][theme]") {
  const QString qml = QString::fromUtf8(read("src/ui/qml/Theme.qml"));
  REQUIRE(intIn(qml, "targetMin") >= 72);
  REQUIRE(intIn(qml, "fontControl") >= 22);
  REQUIRE(intIn(qml, "fontMeta") >= 20);
  REQUIRE(intIn(qml, "fontTitle") >= 30);
}
