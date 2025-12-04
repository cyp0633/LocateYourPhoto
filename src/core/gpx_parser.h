#pragma once

#include "models/track_point.h"
#include <QString>
#include <QVector>

namespace lyp {

/**
 * @brief Parser for GPX trace files.
 * 
 * Extracts GPS trackpoints with timestamps and coordinates from GPX files.
 */
class GpxParser {
public:
    /**
     * @brief Parse a GPX file and extract all trackpoints.
     * @param filePath Path to the GPX file
     * @return Vector of trackpoints sorted by timestamp, empty on error
     */
    static QVector<TrackPoint> parse(const QString& filePath);
    
    /**
     * @brief Calculate the average time interval between trackpoints.
     * @param trackpoints Vector of trackpoints
     * @return Average interval in seconds, or 300.0 if unable to calculate
     */
    static double calculateAverageInterval(const QVector<TrackPoint>& trackpoints);
    
    /**
     * @brief Get the last error message.
     * @return Error message or empty string if no error
     */
    static QString lastError();

private:
    static QString s_lastError;
};

} // namespace lyp
