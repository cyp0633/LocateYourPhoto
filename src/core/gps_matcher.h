#pragma once

#include "models/track_point.h"
#include <QVector>
#include <optional>
#include <tuple>

namespace lyp {

/**
 * @brief Matches photo timestamps with GPS trackpoints.
 * 
 * Uses linear interpolation to find GPS coordinates for a given timestamp.
 */
class GpsMatcher {
public:
    /**
     * @brief Construct a GPS matcher with trackpoints.
     * @param trackpoints Sorted vector of GPS trackpoints
     * @param maxTimeDiffSeconds Maximum time difference for matching
     * @param forceInterpolate If true, always return coordinates even outside time range
     */
    GpsMatcher(const QVector<TrackPoint>& trackpoints, 
               double maxTimeDiffSeconds,
               bool forceInterpolate = false);
    
    /**
     * @brief Find GPS coordinates for a photo timestamp.
     * @param photoTime Photo capture time (UTC)
     * @return Tuple of (latitude, longitude, optional elevation) or nullopt if no match
     */
    std::optional<std::tuple<double, double, std::optional<double>>> 
    findGpsForPhoto(const QDateTime& photoTime) const;
    
    /**
     * @brief Check if a timestamp is within the GPX track time range.
     * @param time Timestamp to check
     * @return true if within range
     */
    bool isWithinTrackRange(const QDateTime& time) const;
    
    /**
     * @brief Get the time range of the track.
     * @return Pair of (start, end) timestamps
     */
    std::pair<QDateTime, QDateTime> trackTimeRange() const;

private:
    QVector<TrackPoint> m_trackpoints;
    double m_maxTimeDiff;
    bool m_forceInterpolate;
};

} // namespace lyp
