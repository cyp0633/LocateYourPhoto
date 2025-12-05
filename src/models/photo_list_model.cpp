#include "photo_list_model.h"

namespace lyp {

PhotoListModel::PhotoListModel(QObject *parent) : QAbstractListModel(parent) {}

int PhotoListModel::rowCount(const QModelIndex &parent) const {
  if (parent.isValid())
    return 0;
  return m_photos.size();
}

QVariant PhotoListModel::data(const QModelIndex &index, int role) const {
  if (!index.isValid() || index.row() >= m_photos.size())
    return QVariant();

  const PhotoItem &photo = m_photos[index.row()];

  switch (role) {
  case Qt::DisplayRole:
  case FileNameRole:
    return photo.fileName;
  case FilePathRole:
    return photo.filePath;
  case CaptureTimeRole:
    return photo.captureTime;
  case HasGpsRole:
    return photo.hasExistingGps;
  case StateRole:
    return static_cast<int>(photo.state);
  case LatitudeRole:
    return photo.matchedLat.value_or(0.0);
  case LongitudeRole:
    return photo.matchedLon.value_or(0.0);
  case ErrorMessageRole:
    return photo.errorMessage;
  default:
    return QVariant();
  }
}

QHash<int, QByteArray> PhotoListModel::roleNames() const {
  return {{FilePathRole, "filePath"},
          {FileNameRole, "fileName"},
          {CaptureTimeRole, "captureTime"},
          {HasGpsRole, "hasGps"},
          {StateRole, "state"},
          {LatitudeRole, "latitude"},
          {LongitudeRole, "longitude"},
          {ErrorMessageRole, "errorMessage"}};
}

void PhotoListModel::addPhoto(const PhotoItem &photo) {
  beginInsertRows(QModelIndex(), m_photos.size(), m_photos.size());
  m_photos.append(photo);
  endInsertRows();
  emit photoAdded(m_photos.size() - 1);
}

void PhotoListModel::addPhotos(const QVector<PhotoItem> &photos) {
  if (photos.isEmpty())
    return;

  beginInsertRows(QModelIndex(), m_photos.size(),
                  m_photos.size() + photos.size() - 1);
  m_photos.append(photos);
  endInsertRows();

  for (int i = m_photos.size() - photos.size(); i < m_photos.size(); ++i) {
    emit photoAdded(i);
  }
}

void PhotoListModel::removePhoto(int index) {
  if (index < 0 || index >= m_photos.size())
    return;

  beginRemoveRows(QModelIndex(), index, index);
  m_photos.removeAt(index);
  endRemoveRows();
}

void PhotoListModel::clear() {
  beginResetModel();
  m_photos.clear();
  endResetModel();
}

void PhotoListModel::updatePhoto(int index, const PhotoItem &photo) {
  if (index < 0 || index >= m_photos.size())
    return;

  m_photos[index] = photo;
  QModelIndex modelIndex = createIndex(index, 0);
  emit dataChanged(modelIndex, modelIndex);
  emit photoUpdated(index);
}

void PhotoListModel::resetAllStates() {
  for (int i = 0; i < m_photos.size(); ++i) {
    m_photos[i].state = PhotoState::Pending;
    m_photos[i].errorMessage.clear();
    m_photos[i].matchedLat.reset();
    m_photos[i].matchedLon.reset();
    m_photos[i].matchedElevation.reset();
  }
  emit dataChanged(createIndex(0, 0), createIndex(m_photos.size() - 1, 0));
}

} // namespace lyp
