#pragma once

#include <QString>
#include <QDateTime>
#include <optional>

namespace lyp {

/**
 * @brief GPS coordinate result from EXIF operations.
 */
struct GpsCoord {
    double latitude;
    double longitude;
    std::optional<double> elevation;
};

/**
 * @brief Handler for reading and writing EXIF metadata using exiv2.
 * 
 * Supports JPEG and common RAW formats (ARW, NEF, CR2, DNG, etc.)
 */
class ExifHandler {
public:
    /**
     * @brief Supported photo file extensions.
     */
    static const QStringList& supportedExtensions();
    
    /**
     * @brief Check if a file extension is supported.
     * @param path File path to check
     * @return true if the file type is supported
     */
    static bool isSupported(const QString& path);
    
    /**
     * @brief Extract capture timestamp from photo EXIF.
     * @param filePath Path to the photo file
     * @param timeOffsetSeconds Timezone offset in seconds to apply (positive = camera ahead of UTC)
     * @return Capture time in UTC, or invalid QDateTime on failure
     */
    static std::optional<QDateTime> getPhotoTimestamp(const QString& filePath, 
                                                       double timeOffsetSeconds = 0.0);
    
    /**
     * @brief Check if photo already has GPS data in EXIF.
     * @param filePath Path to the photo file
     * @return true if GPS coordinates exist
     */
    static bool hasGpsData(const QString& filePath);
    
    /**
     * @brief Read existing GPS coordinates from photo.
     * @param filePath Path to the photo file
     * @return GPS coordinates or nullopt if not present
     */
    static std::optional<GpsCoord> readGpsData(const QString& filePath);
    
    /**
     * @brief Write GPS coordinates to photo EXIF.
     * @param filePath Path to the photo file
     * @param latitude GPS latitude
     * @param longitude GPS longitude
     * @param elevation GPS elevation (optional)
     * @return true on success
     */
    static bool writeGpsData(const QString& filePath, 
                             double latitude, 
                             double longitude,
                             std::optional<double> elevation = std::nullopt);
    
    /**
     * @brief Get the last error message.
     * @return Error message or empty string
     */
    static QString lastError();

private:
    static QString s_lastError;
};

} // namespace lyp
