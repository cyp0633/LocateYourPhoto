#include "gpx_parser.h"
#include <pugixml.hpp>
#include <QFile>
#include <QDebug>
#include <QTimeZone>
#include <algorithm>

namespace lyp {

QString GpxParser::s_lastError;

QVector<TrackPoint> GpxParser::parse(const QString& filePath)
{
    s_lastError.clear();
    QVector<TrackPoint> trackpoints;
    
    pugi::xml_document doc;
    pugi::xml_parse_result result = doc.load_file(filePath.toUtf8().constData());
    
    if (!result) {
        s_lastError = QString("Failed to parse GPX file: %1").arg(result.description());
        qWarning() << s_lastError;
        return trackpoints;
    }
    
    // GPX root element
    pugi::xml_node gpx = doc.child("gpx");
    if (!gpx) {
        s_lastError = "Invalid GPX file: missing <gpx> root element";
        qWarning() << s_lastError;
        return trackpoints;
    }
    
    // Iterate through all tracks
    for (pugi::xml_node trk : gpx.children("trk")) {
        // Iterate through track segments
        for (pugi::xml_node trkseg : trk.children("trkseg")) {
            // Iterate through track points
            for (pugi::xml_node trkpt : trkseg.children("trkpt")) {
                TrackPoint point;
                
                // Parse latitude and longitude (required attributes)
                point.latitude = trkpt.attribute("lat").as_double(0.0);
                point.longitude = trkpt.attribute("lon").as_double(0.0);
                
                // Parse timestamp (child element)
                pugi::xml_node timeNode = trkpt.child("time");
                if (timeNode) {
                    QString timeStr = QString::fromUtf8(timeNode.text().get());
                    // GPX uses ISO 8601 format: 2025-12-01T07:35:10Z or with offset
                    point.timestamp = QDateTime::fromString(timeStr, Qt::ISODate);
                    if (!point.timestamp.isValid()) {
                        // Try alternative format without 'T'
                        point.timestamp = QDateTime::fromString(timeStr, "yyyy-MM-dd HH:mm:ss");
                    }
                    // Ensure UTC
                    point.timestamp.setTimeZone(QTimeZone::utc());
                }
                
                // Parse elevation (optional child element)
                pugi::xml_node eleNode = trkpt.child("ele");
                if (eleNode) {
                    point.elevation = eleNode.text().as_double();
                }
                
                // Only add valid points with timestamps
                if (point.isValid() && point.timestamp.isValid()) {
                    trackpoints.append(point);
                }
            }
        }
    }
    
    // Sort by timestamp
    std::sort(trackpoints.begin(), trackpoints.end(), 
        [](const TrackPoint& a, const TrackPoint& b) {
            return a.timestamp < b.timestamp;
        });
    
    qInfo() << "Parsed" << trackpoints.size() << "trackpoints from" << filePath;
    
    if (!trackpoints.isEmpty()) {
        qInfo() << "Time range:" << trackpoints.first().timestamp 
                << "to" << trackpoints.last().timestamp;
    }
    
    return trackpoints;
}

double GpxParser::calculateAverageInterval(const QVector<TrackPoint>& trackpoints)
{
    if (trackpoints.size() < 2) {
        return 300.0; // Default 5 minutes
    }
    
    double totalSeconds = 0.0;
    int count = 0;
    
    for (int i = 1; i < trackpoints.size(); ++i) {
        qint64 msecs = trackpoints[i - 1].timestamp.msecsTo(trackpoints[i].timestamp);
        double seconds = msecs / 1000.0;
        if (seconds > 0) {
            totalSeconds += seconds;
            ++count;
        }
    }
    
    if (count == 0) {
        return 300.0;
    }
    
    double avgInterval = totalSeconds / count;
    qInfo() << "Average trackpoint interval:" << avgInterval << "seconds";
    return avgInterval;
}

QString GpxParser::lastError()
{
    return s_lastError;
}

} // namespace lyp
