#pragma once

#include <QDateTime>
#include <optional>

namespace lyp {

/**
 * @brief Represents a GPS trackpoint from a GPX file.
 */
struct TrackPoint {
    QDateTime timestamp;
    double latitude = 0.0;
    double longitude = 0.0;
    std::optional<double> elevation;
    
    bool isValid() const {
        return timestamp.isValid() && 
               latitude >= -90.0 && latitude <= 90.0 &&
               longitude >= -180.0 && longitude <= 180.0;
    }
};

} // namespace lyp
