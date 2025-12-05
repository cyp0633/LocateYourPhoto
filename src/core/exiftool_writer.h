#pragma once

#include <QString>
#include <optional>

namespace lyp {

/**
 * @brief Writer for GPS data using external exiftool command.
 *
 * Used for BMFF formats (HEIC, AVIF, CR3, JXL) that exiv2 can't write to.
 */
class ExifToolWriter {
public:
  /**
   * @brief Check if exiftool is available in PATH.
   * @return true if exiftool executable is found
   */
  static bool isAvailable();

  /**
   * @brief Write GPS coordinates using exiftool.
   * @param filePath Path to the photo file
   * @param latitude GPS latitude
   * @param longitude GPS longitude
   * @param elevation GPS elevation (optional)
   * @return true on success
   */
  static bool writeGpsData(const QString &filePath, double latitude,
                           double longitude,
                           std::optional<double> elevation = std::nullopt);

  /**
   * @brief Get the last error message.
   * @return Error message or empty string
   */
  static QString lastError();

private:
  static QString s_lastError;
  static bool s_availabilityChecked;
  static bool s_isAvailable;
};

} // namespace lyp
