#include "exiftool_writer.h"
#include <QDebug>
#include <QProcess>
#include <QStandardPaths>

namespace lyp {

QString ExifToolWriter::s_lastError;
bool ExifToolWriter::s_availabilityChecked = false;
bool ExifToolWriter::s_isAvailable = false;

bool ExifToolWriter::isAvailable() {
  if (!s_availabilityChecked) {
    s_availabilityChecked = true;

    // Try to find exiftool in PATH
    QString exiftoolPath = QStandardPaths::findExecutable("exiftool");
    s_isAvailable = !exiftoolPath.isEmpty();

    if (s_isAvailable) {
      qInfo() << "Found exiftool at:" << exiftoolPath;
    } else {
      qWarning() << "exiftool not found in PATH";
    }
  }
  return s_isAvailable;
}

bool ExifToolWriter::writeGpsData(const QString &filePath, double latitude,
                                  double longitude,
                                  std::optional<double> elevation) {
  s_lastError.clear();

  if (!isAvailable()) {
    s_lastError = "exiftool is not installed or not in PATH";
    return false;
  }

  // Build exiftool command arguments
  QStringList args;
  args << "-overwrite_original"; // Don't create backup files

  // GPS coordinates
  QString latRef = latitude >= 0 ? "N" : "S";
  QString lonRef = longitude >= 0 ? "E" : "W";

  args << QString("-GPSLatitude=%1").arg(qAbs(latitude), 0, 'f', 8);
  args << QString("-GPSLatitudeRef=%1").arg(latRef);
  args << QString("-GPSLongitude=%1").arg(qAbs(longitude), 0, 'f', 8);
  args << QString("-GPSLongitudeRef=%1").arg(lonRef);

  // Elevation if provided
  if (elevation.has_value()) {
    double alt = elevation.value();
    args << QString("-GPSAltitude=%1").arg(qAbs(alt), 0, 'f', 2);
    args << QString("-GPSAltitudeRef=%1")
                .arg(alt >= 0 ? "Above Sea Level" : "Below Sea Level");
  }

  args << filePath;

  // Execute exiftool
  QProcess process;
  process.start("exiftool", args);

  if (!process.waitForFinished(30000)) { // 30 second timeout
    s_lastError = QString("exiftool timed out for %1").arg(filePath);
    process.kill();
    return false;
  }

  if (process.exitCode() != 0) {
    QString stderr = QString::fromUtf8(process.readAllStandardError());
    s_lastError = QString("exiftool failed: %1").arg(stderr.trimmed());
    qWarning() << s_lastError;
    return false;
  }

  qInfo() << "exiftool wrote GPS to" << filePath << ":" << latitude << ","
          << longitude;
  return true;
}

QString ExifToolWriter::lastError() { return s_lastError; }

} // namespace lyp
