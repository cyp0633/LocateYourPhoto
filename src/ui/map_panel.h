#pragma once

#include "models/track_point.h"
#include "models/photo_item.h"
#include <QWidget>
#include <QQuickWidget>
#include <QVector>

namespace lyp {

/**
 * @brief Right panel showing the map with GPS trace and photo markers.
 */
class MapPanel : public QWidget {
    Q_OBJECT

public:
    explicit MapPanel(QWidget* parent = nullptr);
    
    /**
     * @brief Set the GPS track to display.
     * @param trackpoints Vector of trackpoints
     */
    void setTrack(const QVector<TrackPoint>& trackpoints);
    
    /**
     * @brief Add a photo marker to the map.
     * @param photo Photo item with matched coordinates
     */
    void addPhotoMarker(const PhotoItem& photo);
    
    /**
     * @brief Update a photo marker on the map.
     * @param index Photo index
     * @param photo Photo item with updated state
     */
    void updatePhotoMarker(int index, const PhotoItem& photo);
    
    /**
     * @brief Clear all photo markers.
     */
    void clearPhotoMarkers();
    
    /**
     * @brief Center map on the track.
     */
    void centerOnTrack();
    
    /**
     * @brief Highlight a specific photo marker.
     * @param index Photo index to highlight
     */
    void highlightPhoto(int index);

signals:
    void photoMarkerClicked(int index);

private:
    void setupUi();
    void updateTrackInQml();
    
    QQuickWidget* m_quickWidget;
    QVector<TrackPoint> m_trackpoints;
    QVector<PhotoItem> m_photoMarkers;
};

} // namespace lyp
