#pragma once

#include "models/photo_item.h"
#include <QAbstractListModel>
#include <QVector>

namespace lyp {

/**
 * @brief Qt model for the photo list view.
 */
class PhotoListModel : public QAbstractListModel {
    Q_OBJECT

public:
    enum Roles {
        FilePathRole = Qt::UserRole + 1,
        FileNameRole,
        CaptureTimeRole,
        HasGpsRole,
        StateRole,
        LatitudeRole,
        LongitudeRole,
        ErrorMessageRole
    };

    explicit PhotoListModel(QObject* parent = nullptr);

    // QAbstractListModel interface
    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    // Photo management
    void addPhoto(const PhotoItem& photo);
    void addPhotos(const QVector<PhotoItem>& photos);
    void clear();
    
    // Update photo state after processing
    void updatePhoto(int index, const PhotoItem& photo);
    
    // Accessors
    const QVector<PhotoItem>& photos() const { return m_photos; }
    PhotoItem& photoAt(int index) { return m_photos[index]; }
    int count() const { return m_photos.size(); }

signals:
    void photoAdded(int index);
    void photoUpdated(int index);

private:
    QVector<PhotoItem> m_photos;
};

} // namespace lyp
