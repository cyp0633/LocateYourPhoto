#include "exif_handler.h"
#include <exiv2/exiv2.hpp>
#include <QFileInfo>
#include <QDebug>
#include <cmath>

namespace lyp {

QString ExifHandler::s_lastError;

const QStringList& ExifHandler::supportedExtensions()
{
    static const QStringList extensions = {
        // JPEG
        "jpg", "jpeg",
        // RAW formats
        "arw", "nef", "cr2", "cr3", "dng", "orf", "rw2", "raf", "raw",
        // Modern formats
        "heic", "heif", "avif", "webp",
        // Other
        "tiff", "tif", "png"
    };
    return extensions;
}

bool ExifHandler::isSupported(const QString& path)
{
    QString ext = QFileInfo(path).suffix().toLower();
    return supportedExtensions().contains(ext);
}

std::optional<QDateTime> ExifHandler::getPhotoTimestamp(const QString& filePath, 
                                                         double timeOffsetSeconds)
{
    s_lastError.clear();
    
    try {
        auto image = Exiv2::ImageFactory::open(filePath.toStdString());
        image->readMetadata();
        
        const Exiv2::ExifData& exifData = image->exifData();
        if (exifData.empty()) {
            s_lastError = "No EXIF data found";
            return std::nullopt;
        }
        
        // Try DateTimeOriginal first, then CreateDate
        QStringList dateKeys = {
            "Exif.Photo.DateTimeOriginal",
            "Exif.Image.DateTimeOriginal",
            "Exif.Photo.DateTimeDigitized",
            "Exif.Image.DateTime"
        };
        
        for (const QString& key : dateKeys) {
            auto it = exifData.findKey(Exiv2::ExifKey(key.toStdString()));
            if (it != exifData.end()) {
                QString dateStr = QString::fromStdString(it->toString());
                // EXIF format: "YYYY:MM:DD HH:MM:SS"
                QDateTime dt = QDateTime::fromString(dateStr, "yyyy:MM:dd HH:mm:ss");
                if (dt.isValid()) {
                    // Convert from local camera time to UTC
                    dt.setTimeSpec(Qt::UTC);
                    if (timeOffsetSeconds != 0.0) {
                        dt = dt.addSecs(static_cast<qint64>(-timeOffsetSeconds));
                    }
                    return dt;
                }
            }
        }
        
        s_lastError = "No valid timestamp found in EXIF";
        return std::nullopt;
        
    } catch (const Exiv2::Error& e) {
        s_lastError = QString("Exiv2 error: %1").arg(e.what());
        qWarning() << s_lastError;
        return std::nullopt;
    }
}

bool ExifHandler::hasGpsData(const QString& filePath)
{
    try {
        auto image = Exiv2::ImageFactory::open(filePath.toStdString());
        image->readMetadata();
        
        const Exiv2::ExifData& exifData = image->exifData();
        auto latIt = exifData.findKey(Exiv2::ExifKey("Exif.GPSInfo.GPSLatitude"));
        auto lonIt = exifData.findKey(Exiv2::ExifKey("Exif.GPSInfo.GPSLongitude"));
        
        return latIt != exifData.end() && lonIt != exifData.end();
        
    } catch (const Exiv2::Error&) {
        return false;
    }
}

std::optional<GpsCoord> ExifHandler::readGpsData(const QString& filePath)
{
    try {
        auto image = Exiv2::ImageFactory::open(filePath.toStdString());
        image->readMetadata();
        
        const Exiv2::ExifData& exifData = image->exifData();
        
        auto latIt = exifData.findKey(Exiv2::ExifKey("Exif.GPSInfo.GPSLatitude"));
        auto lonIt = exifData.findKey(Exiv2::ExifKey("Exif.GPSInfo.GPSLongitude"));
        auto latRefIt = exifData.findKey(Exiv2::ExifKey("Exif.GPSInfo.GPSLatitudeRef"));
        auto lonRefIt = exifData.findKey(Exiv2::ExifKey("Exif.GPSInfo.GPSLongitudeRef"));
        
        if (latIt == exifData.end() || lonIt == exifData.end()) {
            return std::nullopt;
        }
        
        // Parse DMS to decimal degrees
        auto parseCoord = [](const Exiv2::Value& value) -> double {
            if (value.count() < 3) return 0.0;
            double deg = value.toRational(0).first / static_cast<double>(value.toRational(0).second);
            double min = value.toRational(1).first / static_cast<double>(value.toRational(1).second);
            double sec = value.toRational(2).first / static_cast<double>(value.toRational(2).second);
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
            auto altRefIt = exifData.findKey(Exiv2::ExifKey("Exif.GPSInfo.GPSAltitudeRef"));
            double alt = altIt->toFloat();
            if (altRefIt != exifData.end() && altRefIt->toInt64() == 1) {
                alt = -alt; // Below sea level
            }
            coord.elevation = alt;
        }
        
        return coord;
        
    } catch (const Exiv2::Error& e) {
        s_lastError = QString("Exiv2 error: %1").arg(e.what());
        return std::nullopt;
    }
}

bool ExifHandler::writeGpsData(const QString& filePath, 
                                double latitude, 
                                double longitude,
                                std::optional<double> elevation)
{
    s_lastError.clear();
    
    try {
        auto image = Exiv2::ImageFactory::open(filePath.toStdString());
        image->readMetadata();
        
        Exiv2::ExifData& exifData = image->exifData();
        
        // Helper to convert decimal degrees to DMS rational
        auto toRational = [](double decimal) -> std::vector<Exiv2::Rational> {
            decimal = std::abs(decimal);
            int deg = static_cast<int>(decimal);
            double minDecimal = (decimal - deg) * 60.0;
            int min = static_cast<int>(minDecimal);
            double sec = (minDecimal - min) * 60.0;
            // Use high precision for seconds (multiply by 10000)
            int secNumerator = static_cast<int>(sec * 10000);
            return {
                {deg, 1},
                {min, 1},
                {secNumerator, 10000}
            };
        };
        
        // Set GPS Version ID
        Exiv2::Value::UniquePtr versionValue = Exiv2::Value::create(Exiv2::unsignedByte);
        versionValue->read("2 3 0 0");
        exifData["Exif.GPSInfo.GPSVersionID"] = *versionValue;
        
        // Set latitude
        auto latRationals = toRational(latitude);
        exifData["Exif.GPSInfo.GPSLatitudeRef"] = (latitude >= 0) ? "N" : "S";
        Exiv2::URationalValue latValue;
        for (const auto& r : latRationals) {
            latValue.value_.push_back(std::make_pair(static_cast<uint32_t>(r.first), 
                                                      static_cast<uint32_t>(r.second)));
        }
        exifData["Exif.GPSInfo.GPSLatitude"] = latValue;
        
        // Set longitude
        auto lonRationals = toRational(longitude);
        exifData["Exif.GPSInfo.GPSLongitudeRef"] = (longitude >= 0) ? "E" : "W";
        Exiv2::URationalValue lonValue;
        for (const auto& r : lonRationals) {
            lonValue.value_.push_back(std::make_pair(static_cast<uint32_t>(r.first), 
                                                      static_cast<uint32_t>(r.second)));
        }
        exifData["Exif.GPSInfo.GPSLongitude"] = lonValue;
        
        // Set elevation if provided
        if (elevation.has_value()) {
            double alt = elevation.value();
            exifData["Exif.GPSInfo.GPSAltitudeRef"] = (alt >= 0) ? "0" : "1";
            int altNumerator = static_cast<int>(std::abs(alt) * 100);
            Exiv2::URationalValue altValue;
            altValue.value_.push_back(std::make_pair(static_cast<uint32_t>(altNumerator), 100u));
            exifData["Exif.GPSInfo.GPSAltitude"] = altValue;
        }
        
        image->writeMetadata();
        
        qInfo() << "Wrote GPS to" << filePath << ":" << latitude << "," << longitude;
        return true;
        
    } catch (const Exiv2::Error& e) {
        s_lastError = QString("Failed to write GPS: %1").arg(e.what());
        qWarning() << s_lastError;
        return false;
    }
}

QString ExifHandler::lastError()
{
    return s_lastError;
}

} // namespace lyp
