#pragma once

#include "models/photo_item.h"
#include "models/track_point.h"
#include <QObject>
#include <QVector>
#include <QFuture>

namespace lyp {

class PhotoListModel;

/**
 * @brief Processing settings for photo geotagging.
 */
struct ProcessingSettings {
    double maxTimeDiffSeconds = 300.0;  // Max time difference for matching
    double timeOffsetHours = 0.0;       // Camera timezone offset from UTC
    bool overwriteExistingGps = false;  // Overwrite photos that already have GPS
    bool forceInterpolate = false;      // Always interpolate regardless of time diff
    bool dryRun = false;                // Preview only, don't write changes
};

/**
 * @brief Orchestrates the photo geotagging process.
 * 
 * Coordinates GPX parsing, photo scanning, GPS matching, and EXIF writing.
 */
class PhotoProcessor : public QObject {
    Q_OBJECT

public:
    explicit PhotoProcessor(QObject* parent = nullptr);
    
    /**
     * @brief Load a GPX trace file.
     * @param filePath Path to GPX file
     * @return true on success
     */
    bool loadGpxFile(const QString& filePath);
    
    /**
     * @brief Get the loaded trackpoints.
     */
    const QVector<TrackPoint>& trackpoints() const { return m_trackpoints; }
    
    /**
     * @brief Scan photo files and populate the model.
     * @param filePaths List of photo file paths
     * @param model Model to populate with photo items
     */
    void scanPhotos(const QStringList& filePaths, PhotoListModel* model);
    
    /**
     * @brief Process all photos in the model.
     * @param model Photo list model
     * @param settings Processing settings
     */
    void processPhotos(PhotoListModel* model, const ProcessingSettings& settings);
    
    /**
     * @brief Stop ongoing processing.
     */
    void stopProcessing();
    
    /**
     * @brief Check if GPX is loaded.
     */
    bool hasGpxLoaded() const { return !m_trackpoints.isEmpty(); }

signals:
    /**
     * @brief Emitted when GPX file is loaded.
     * @param trackpointCount Number of trackpoints loaded
     */
    void gpxLoaded(int trackpointCount);
    
    /**
     * @brief Emitted when GPX loading fails.
     * @param error Error message
     */
    void gpxLoadError(const QString& error);
    
    /**
     * @brief Emitted when photo scanning is complete.
     * @param photoCount Number of photos found
     */
    void photosScanComplete(int photoCount);
    
    /**
     * @brief Emitted when a single photo is processed.
     * @param index Photo index in model
     * @param success Whether processing succeeded
     */
    void photoProcessed(int index, bool success);
    
    /**
     * @brief Emitted when all photos are processed.
     * @param successCount Number of photos successfully processed
     * @param totalCount Total number of photos
     */
    void processingComplete(int successCount, int totalCount);
    
    /**
     * @brief Emitted with progress updates.
     * @param current Current photo index
     * @param total Total photos
     */
    void progressUpdated(int current, int total);

private:
    QVector<TrackPoint> m_trackpoints;
    QString m_gpxFilePath;
    bool m_stopRequested = false;
};

} // namespace lyp
