#include "map_panel.h"
#include <QVBoxLayout>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQuickItem>
#include <QVariantList>
#include <QVariantMap>
#include <QDebug>

namespace lyp {

MapPanel::MapPanel(QWidget* parent)
    : QWidget(parent)
{
    setupUi();
}

void MapPanel::setupUi()
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    
    m_quickWidget = new QQuickWidget(this);
    m_quickWidget->setResizeMode(QQuickWidget::SizeRootObjectToView);
    m_quickWidget->engine()->rootContext()->setContextProperty("mapPanel", this);
    m_quickWidget->setSource(QUrl("qrc:/src/qml/MapView.qml"));
    
    // Check for errors
    if (m_quickWidget->status() == QQuickWidget::Error) {
        for (const QQmlError& error : m_quickWidget->errors()) {
            qWarning() << "QML Error:" << error.toString();
        }
    }
    
    layout->addWidget(m_quickWidget);
}

void MapPanel::setTrack(const QVector<TrackPoint>& trackpoints)
{
    m_trackpoints = trackpoints;
    updateTrackInQml();
}

void MapPanel::updateTrackInQml()
{
    QVariantList points;
    for (const TrackPoint& tp : m_trackpoints) {
        QVariantMap point;
        point["latitude"] = tp.latitude;
        point["longitude"] = tp.longitude;
        if (tp.elevation.has_value()) {
            point["elevation"] = tp.elevation.value();
        }
        points.append(point);
    }
    
    QQuickItem* rootObject = m_quickWidget->rootObject();
    if (rootObject) {
        QMetaObject::invokeMethod(rootObject, "setTrackPoints",
                                  Q_ARG(QVariant, QVariant::fromValue(points)));
    }
}

void MapPanel::addPhotoMarker(const PhotoItem& photo)
{
    m_photoMarkers.append(photo);
    
    if (photo.hasMatchedCoordinates()) {
        QQuickItem* rootObject = m_quickWidget->rootObject();
        if (rootObject) {
            QVariantMap marker;
            marker["index"] = m_photoMarkers.size() - 1;
            marker["latitude"] = photo.matchedLat.value();
            marker["longitude"] = photo.matchedLon.value();
            marker["fileName"] = photo.fileName;
            marker["state"] = static_cast<int>(photo.state);
            
            QMetaObject::invokeMethod(rootObject, "addPhotoMarker",
                                      Q_ARG(QVariant, QVariant::fromValue(marker)));
        }
    }
}

void MapPanel::updatePhotoMarker(int index, const PhotoItem& photo)
{
    if (index >= 0 && index < m_photoMarkers.size()) {
        m_photoMarkers[index] = photo;
        
        if (photo.hasMatchedCoordinates()) {
            QQuickItem* rootObject = m_quickWidget->rootObject();
            if (rootObject) {
                QVariantMap marker;
                marker["index"] = index;
                marker["latitude"] = photo.matchedLat.value();
                marker["longitude"] = photo.matchedLon.value();
                marker["fileName"] = photo.fileName;
                marker["state"] = static_cast<int>(photo.state);
                
                QMetaObject::invokeMethod(rootObject, "updatePhotoMarker",
                                          Q_ARG(QVariant, QVariant::fromValue(marker)));
            }
        }
    }
}

void MapPanel::clearPhotoMarkers()
{
    m_photoMarkers.clear();
    QQuickItem* rootObject = m_quickWidget->rootObject();
    if (rootObject) {
        QMetaObject::invokeMethod(rootObject, "clearPhotoMarkers");
    }
}

void MapPanel::centerOnTrack()
{
    QQuickItem* rootObject = m_quickWidget->rootObject();
    if (rootObject) {
        QMetaObject::invokeMethod(rootObject, "centerOnTrack");
    }
}

void MapPanel::highlightPhoto(int index)
{
    QQuickItem* rootObject = m_quickWidget->rootObject();
    if (rootObject) {
        QMetaObject::invokeMethod(rootObject, "highlightPhoto",
                                  Q_ARG(QVariant, index));
    }
}

} // namespace lyp
