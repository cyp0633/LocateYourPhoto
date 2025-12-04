#pragma once

#include <QString>
#include <QDateTime>
#include <optional>

namespace lyp {

/**
 * @brief Processing state of a photo.
 */
enum class PhotoState {
    Pending,        // Not yet processed
    Processing,     // Currently being processed
    Success,        // GPS data written successfully
    Skipped,        // Skipped (already has GPS or no match)
    Error           // Processing failed
};

/**
 * @brief Represents a photo file with its metadata and processing state.
 */
struct PhotoItem {
    QString filePath;
    QString fileName;
    QDateTime captureTime;
    bool hasExistingGps = false;
    PhotoState state = PhotoState::Pending;
    QString errorMessage;
    
    // Matched GPS coordinates (set after processing)
    std::optional<double> matchedLat;
    std::optional<double> matchedLon;
    std::optional<double> matchedElevation;
    
    bool isProcessed() const {
        return state == PhotoState::Success || 
               state == PhotoState::Skipped || 
               state == PhotoState::Error;
    }
    
    bool hasMatchedCoordinates() const {
        return matchedLat.has_value() && matchedLon.has_value();
    }
};

} // namespace lyp
