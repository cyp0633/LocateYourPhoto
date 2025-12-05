#include "exif_handler.h"
#include <QDebug>
#include <QFileInfo>
#include <QHash>
#include <QTimeZone>
#include <cmath>
#include <exiv2/exiv2.hpp>

namespace lyp {

QString ExifHandler::s_lastError;

// Format support database based on exiv2 manual
// Key: extension (lowercase), Value: {level, warning}
static const QHash<QString, FormatInfo> &getFormatDatabase() {
  static const QHash<QString, FormatInfo> db = {
      // FullWrite - exiv2 handles natively
      {"jpg", {FormatSupportLevel::FullWrite, ""}},
      {"jpeg", {FormatSupportLevel::FullWrite, ""}},
      {"tiff", {FormatSupportLevel::FullWrite, ""}},
      {"tif", {FormatSupportLevel::FullWrite, ""}},
      {"dng", {FormatSupportLevel::FullWrite, ""}},
      {"arw", {FormatSupportLevel::FullWrite, ""}}, // Sony
      {"cr2", {FormatSupportLevel::FullWrite, ""}}, // Canon
      {"nef", {FormatSupportLevel::FullWrite, ""}}, // Nikon
      {"orf", {FormatSupportLevel::FullWrite, ""}}, // Olympus
      {"pef", {FormatSupportLevel::FullWrite, ""}}, // Pentax
      {"srw", {FormatSupportLevel::FullWrite, ""}}, // Samsung
      {"webp", {FormatSupportLevel::FullWrite, ""}},
      {"jp2", {FormatSupportLevel::FullWrite, ""}}, // JPEG 2000
      {"exv", {FormatSupportLevel::FullWrite, ""}}, // Exiv2 sidecar
      {"psd", {FormatSupportLevel::FullWrite, ""}}, // Photoshop
      {"pgf", {FormatSupportLevel::FullWrite, ""}},
      {"png", {FormatSupportLevel::FullWrite, ""}}, // XMP GPS works

      // NeedsExifTool - BMFF formats require external exiftool
      {"heic",
       {FormatSupportLevel::NeedsExifTool, "Will use external exiftool"}},
      {"heif",
       {FormatSupportLevel::NeedsExifTool, "Will use external exiftool"}},
      {"avif",
       {FormatSupportLevel::NeedsExifTool, "Will use external exiftool"}},
      {"cr3",
       {FormatSupportLevel::NeedsExifTool, "Will use external exiftool"}},
      {"jxl",
       {FormatSupportLevel::NeedsExifTool, "Will use external exiftool"}},

      // DangerousRAW - may work but risky for proprietary formats
      {"raf",
       {FormatSupportLevel::DangerousRAW,
        "Fujifilm RAW - modification may corrupt file"}},
      {"rw2",
       {FormatSupportLevel::DangerousRAW,
        "Panasonic RAW - modification may corrupt file"}},
      {"sr2",
       {FormatSupportLevel::DangerousRAW,
        "Sony old RAW - modification may corrupt file"}},
      {"mrw",
       {FormatSupportLevel::DangerousRAW,
        "Minolta RAW - modification may corrupt file"}},
      {"crw",
       {FormatSupportLevel::DangerousRAW,
        "Canon old RAW - modification may corrupt file"}},
      {"raw",
       {FormatSupportLevel::DangerousRAW,
        "Generic RAW - modification may corrupt file"}},

      // Minimal support - no metadata
      {"bmp", {FormatSupportLevel::Minimal, "No metadata support"}},
      {"gif", {FormatSupportLevel::Minimal, "No metadata support"}},
      {"tga", {FormatSupportLevel::Minimal, "No metadata support"}},
  };
  return db;
}

const QStringList &ExifHandler::supportedExtensions() {
  static const QStringList extensions = {
      // JPEG - Full support
      "jpg", "jpeg",
      // RAW formats - Full write support
      "arw", "nef", "cr2", "dng", "orf", "pef", "srw",
      // RAW formats - Read-only/risky
      "cr3", "rw2", "raf", "raw", "mrw", "sr2", "crw",
      // Modern formats
      "heic", "heif", "avif", "jxl", "webp",
      // Other
      "tiff", "tif", "png", "jp2", "psd", "pgf", "exv"};
  return extensions;
}

bool ExifHandler::isSupported(const QString &path) {
  QString ext = QFileInfo(path).suffix().toLower();
  return supportedExtensions().contains(ext);
}

std::optional<QDateTime>
ExifHandler::getPhotoTimestamp(const QString &filePath,
                               double timeOffsetSeconds) {
  s_lastError.clear();

  try {
    auto image = Exiv2::ImageFactory::open(filePath.toStdString());
    image->readMetadata();

    const Exiv2::ExifData &exifData = image->exifData();
    if (exifData.empty()) {
      s_lastError = "No EXIF data found";
      return std::nullopt;
    }

    // Try DateTimeOriginal first, then CreateDate
    QStringList dateKeys = {
        "Exif.Photo.DateTimeOriginal", "Exif.Image.DateTimeOriginal",
        "Exif.Photo.DateTimeDigitized", "Exif.Image.DateTime"};

    for (const QString &key : dateKeys) {
      auto it = exifData.findKey(Exiv2::ExifKey(key.toStdString()));
      if (it != exifData.end()) {
        QString dateStr = QString::fromStdString(it->toString());
        // EXIF format: "YYYY:MM:DD HH:MM:SS"
        QDateTime dt = QDateTime::fromString(dateStr, "yyyy:MM:dd HH:mm:ss");
        if (dt.isValid()) {
          // Convert from local camera time to UTC
          dt.setTimeZone(QTimeZone::utc());
          if (timeOffsetSeconds != 0.0) {
            dt = dt.addSecs(static_cast<qint64>(-timeOffsetSeconds));
          }
          return dt;
        }
      }
    }

    s_lastError = "No valid timestamp found in EXIF";
    return std::nullopt;

  } catch (const Exiv2::Error &e) {
    s_lastError = QString("Exiv2 error: %1").arg(e.what());
    qWarning() << s_lastError;
    return std::nullopt;
  }
}

bool ExifHandler::hasGpsData(const QString &filePath) {
  try {
    auto image = Exiv2::ImageFactory::open(filePath.toStdString());
    image->readMetadata();

    const Exiv2::ExifData &exifData = image->exifData();
    auto latIt = exifData.findKey(Exiv2::ExifKey("Exif.GPSInfo.GPSLatitude"));
    auto lonIt = exifData.findKey(Exiv2::ExifKey("Exif.GPSInfo.GPSLongitude"));

    return latIt != exifData.end() && lonIt != exifData.end();

  } catch (const Exiv2::Error &) {
    return false;
  }
}

std::optional<GpsCoord> ExifHandler::readGpsData(const QString &filePath) {
  try {
    auto image = Exiv2::ImageFactory::open(filePath.toStdString());
    image->readMetadata();

    const Exiv2::ExifData &exifData = image->exifData();

    auto latIt = exifData.findKey(Exiv2::ExifKey("Exif.GPSInfo.GPSLatitude"));
    auto lonIt = exifData.findKey(Exiv2::ExifKey("Exif.GPSInfo.GPSLongitude"));
    auto latRefIt =
        exifData.findKey(Exiv2::ExifKey("Exif.GPSInfo.GPSLatitudeRef"));
    auto lonRefIt =
        exifData.findKey(Exiv2::ExifKey("Exif.GPSInfo.GPSLongitudeRef"));

    if (latIt == exifData.end() || lonIt == exifData.end()) {
      return std::nullopt;
    }

    // Parse DMS to decimal degrees
    auto parseCoord = [](const Exiv2::Value &value) -> double {
      if (value.count() < 3)
        return 0.0;
      double deg = value.toRational(0).first /
                   static_cast<double>(value.toRational(0).second);
      double min = value.toRational(1).first /
                   static_cast<double>(value.toRational(1).second);
      double sec = value.toRational(2).first /
                   static_cast<double>(value.toRational(2).second);
      return deg + min / 60.0 + sec / 3600.0;
    };

    GpsCoord coord;
    coord.latitude = parseCoord(latIt->value());
    coord.longitude = parseCoord(lonIt->value());

    // Apply reference (N/S, E/W)
    if (latRefIt != exifData.end() && latRefIt->toString() == "S") {
      coord.latitude = -coord.latitude;
    }
    if (lonRefIt != exifData.end() && lonRefIt->toString() == "W") {
      coord.longitude = -coord.longitude;
    }

    // Check for elevation
    auto altIt = exifData.findKey(Exiv2::ExifKey("Exif.GPSInfo.GPSAltitude"));
    if (altIt != exifData.end()) {
      auto altRefIt =
          exifData.findKey(Exiv2::ExifKey("Exif.GPSInfo.GPSAltitudeRef"));
      double alt = altIt->toFloat();
      if (altRefIt != exifData.end() && altRefIt->toInt64() == 1) {
        alt = -alt; // Below sea level
      }
      coord.elevation = alt;
    }

    return coord;

  } catch (const Exiv2::Error &e) {
    s_lastError = QString("Exiv2 error: %1").arg(e.what());
    return std::nullopt;
  }
}

bool ExifHandler::writeGpsData(const QString &filePath, double latitude,
                               double longitude,
                               std::optional<double> elevation) {
  s_lastError.clear();

  try {
    auto image = Exiv2::ImageFactory::open(filePath.toStdString());
    image->readMetadata();

    Exiv2::ExifData &exifData = image->exifData();

    // Helper to convert decimal degrees to DMS rational
    auto toRational = [](double decimal) -> std::vector<Exiv2::Rational> {
      decimal = std::abs(decimal);
      int deg = static_cast<int>(decimal);
      double minDecimal = (decimal - deg) * 60.0;
      int min = static_cast<int>(minDecimal);
      double sec = (minDecimal - min) * 60.0;
      // Use high precision for seconds (multiply by 10000)
      int secNumerator = static_cast<int>(sec * 10000);
      return {{deg, 1}, {min, 1}, {secNumerator, 10000}};
    };

    // Set GPS Version ID
    Exiv2::Value::UniquePtr versionValue =
        Exiv2::Value::create(Exiv2::unsignedByte);
    versionValue->read("2 3 0 0");
    exifData["Exif.GPSInfo.GPSVersionID"] = *versionValue;

    // Set latitude
    auto latRationals = toRational(latitude);
    exifData["Exif.GPSInfo.GPSLatitudeRef"] = (latitude >= 0) ? "N" : "S";
    Exiv2::URationalValue latValue;
    for (const auto &r : latRationals) {
      latValue.value_.push_back(std::make_pair(
          static_cast<uint32_t>(r.first), static_cast<uint32_t>(r.second)));
    }
    exifData["Exif.GPSInfo.GPSLatitude"] = latValue;

    // Set longitude
    auto lonRationals = toRational(longitude);
    exifData["Exif.GPSInfo.GPSLongitudeRef"] = (longitude >= 0) ? "E" : "W";
    Exiv2::URationalValue lonValue;
    for (const auto &r : lonRationals) {
      lonValue.value_.push_back(std::make_pair(
          static_cast<uint32_t>(r.first), static_cast<uint32_t>(r.second)));
    }
    exifData["Exif.GPSInfo.GPSLongitude"] = lonValue;

    // Set elevation if provided
    if (elevation.has_value()) {
      double alt = elevation.value();
      exifData["Exif.GPSInfo.GPSAltitudeRef"] = (alt >= 0) ? "0" : "1";
      int altNumerator = static_cast<int>(std::abs(alt) * 100);
      Exiv2::URationalValue altValue;
      altValue.value_.push_back(
          std::make_pair(static_cast<uint32_t>(altNumerator), 100u));
      exifData["Exif.GPSInfo.GPSAltitude"] = altValue;
    }

    image->writeMetadata();

    qInfo() << "Wrote GPS to" << filePath << ":" << latitude << ","
            << longitude;
    return true;

  } catch (const Exiv2::Error &e) {
    // Provide more helpful error messages based on format
    FormatInfo formatInfo = getFormatInfo(filePath);
    QString errorMsg = QString::fromStdString(e.what());

    if (formatInfo.level == FormatSupportLevel::DangerousRAW) {
      s_lastError = QString("Failed to write GPS to %1 RAW: %2\n%3")
                        .arg(QFileInfo(filePath).suffix().toUpper())
                        .arg(errorMsg)
                        .arg(formatInfo.warning);
    } else if (formatInfo.level == FormatSupportLevel::Minimal) {
      s_lastError = QString("Cannot write metadata to %1 format: %2")
                        .arg(QFileInfo(filePath).suffix().toUpper())
                        .arg(formatInfo.warning);
    } else {
      s_lastError = QString("Failed to write GPS: %1").arg(errorMsg);
    }
    qWarning() << s_lastError;
    return false;
  }
}

QString ExifHandler::lastError() { return s_lastError; }

FormatInfo ExifHandler::getFormatInfo(const QString &path) {
  QString ext = QFileInfo(path).suffix().toLower();
  const auto &db = getFormatDatabase();

  if (db.contains(ext)) {
    return db.value(ext);
  }

  // Unknown format - treat as dangerous RAW
  return {
      FormatSupportLevel::DangerousRAW,
      QString("Unknown format '%1' - modification may corrupt file").arg(ext)};
}

QStringList ExifHandler::getExtensionsByLevel(FormatSupportLevel level) {
  QStringList result;
  const auto &db = getFormatDatabase();

  for (auto it = db.constBegin(); it != db.constEnd(); ++it) {
    if (it.value().level == level) {
      result.append(it.key());
    }
  }
  result.sort();
  return result;
}

bool ExifHandler::canSafelyWrite(const QString &path) {
  return getFormatInfo(path).level == FormatSupportLevel::FullWrite;
}

bool ExifHandler::isRawFormat(const QString &path) {
  QString ext = QFileInfo(path).suffix().toLower();
  // List of all RAW format extensions
  static const QStringList rawExtensions = {
      "arw", "nef", "cr2", "cr3", "dng", "orf", "pef", "srw",  // FullWrite RAW
      "raf", "rw2", "sr2", "mrw", "crw", "raw"                  // DangerousRAW
  };
  return rawExtensions.contains(ext);
}

} // namespace lyp
