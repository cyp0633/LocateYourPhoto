#include "gps_matcher.h"
#include <algorithm>

namespace lyp {

GpsMatcher::GpsMatcher(const QVector<TrackPoint>& trackpoints, 
                       double maxTimeDiffSeconds,
                       bool forceInterpolate)
    : m_trackpoints(trackpoints)
    , m_maxTimeDiff(maxTimeDiffSeconds)
    , m_forceInterpolate(forceInterpolate)
{
}

std::optional<std::tuple<double, double, std::optional<double>>> 
GpsMatcher::findGpsForPhoto(const QDateTime& photoTime) const
{
    if (m_trackpoints.isEmpty()) {
        return std::nullopt;
    }
    
    // Find the two closest trackpoints (before and after)
    const TrackPoint* before = nullptr;
    const TrackPoint* after = nullptr;
    
    for (int i = 0; i < m_trackpoints.size(); ++i) {
        if (m_trackpoints[i].timestamp <= photoTime) {
            before = &m_trackpoints[i];
        } else {
            after = &m_trackpoints[i];
            break;
        }
    }
    
    // Handle edge cases
    if (!before) {
        // Photo is before first trackpoint
        double timeDiff = m_trackpoints.first().timestamp.toMSecsSinceEpoch() / 1000.0 
                        - photoTime.toMSecsSinceEpoch() / 1000.0;
        if (m_forceInterpolate || timeDiff <= m_maxTimeDiff) {
            const auto& p = m_trackpoints.first();
            return std::make_tuple(p.latitude, p.longitude, p.elevation);
        }
        return std::nullopt;
    }
    
    if (!after) {
        // Photo is after last trackpoint
        double timeDiff = photoTime.toMSecsSinceEpoch() / 1000.0 
                        - m_trackpoints.last().timestamp.toMSecsSinceEpoch() / 1000.0;
        if (m_forceInterpolate || timeDiff <= m_maxTimeDiff) {
            const auto& p = m_trackpoints.last();
            return std::make_tuple(p.latitude, p.longitude, p.elevation);
        }
        return std::nullopt;
    }
    
    // Calculate time differences
    double timeDiffBefore = photoTime.toMSecsSinceEpoch() / 1000.0 
                          - before->timestamp.toMSecsSinceEpoch() / 1000.0;
    double timeDiffAfter = after->timestamp.toMSecsSinceEpoch() / 1000.0 
                         - photoTime.toMSecsSinceEpoch() / 1000.0;
    
    // Check if within acceptable time range
    if (!m_forceInterpolate && 
        std::min(timeDiffBefore, timeDiffAfter) > m_maxTimeDiff) {
        return std::nullopt;
    }
    
    // Linear interpolation
    double totalTime = after->timestamp.toMSecsSinceEpoch() / 1000.0 
                     - before->timestamp.toMSecsSinceEpoch() / 1000.0;
    
    if (totalTime <= 0) {
        // Exact match or very close points
        return std::make_tuple(before->latitude, before->longitude, before->elevation);
    }
    
    double ratio = timeDiffBefore / totalTime;
    
    double latitude = before->latitude + (after->latitude - before->latitude) * ratio;
    double longitude = before->longitude + (after->longitude - before->longitude) * ratio;
    
    std::optional<double> elevation;
    if (before->elevation.has_value() && after->elevation.has_value()) {
        elevation = before->elevation.value() + 
                   (after->elevation.value() - before->elevation.value()) * ratio;
    }
    
    return std::make_tuple(latitude, longitude, elevation);
}

bool GpsMatcher::isWithinTrackRange(const QDateTime& time) const
{
    if (m_trackpoints.isEmpty()) {
        return false;
    }
    return time >= m_trackpoints.first().timestamp && 
           time <= m_trackpoints.last().timestamp;
}

std::pair<QDateTime, QDateTime> GpsMatcher::trackTimeRange() const
{
    if (m_trackpoints.isEmpty()) {
        return {QDateTime(), QDateTime()};
    }
    return {m_trackpoints.first().timestamp, m_trackpoints.last().timestamp};
}

} // namespace lyp
