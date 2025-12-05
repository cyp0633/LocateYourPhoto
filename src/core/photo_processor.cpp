#include "photo_processor.h"
#include "exif_handler.h"
#include "exiftool_writer.h"
#include "gps_matcher.h"
#include "gpx_parser.h"
#include "models/photo_list_model.h"
#include <QDebug>
#include <QFileInfo>
#include <algorithm>

namespace lyp {

PhotoProcessor::PhotoProcessor(QObject *parent) : QObject(parent) {}

bool PhotoProcessor::loadGpxFile(const QString &filePath) {
  m_trackpoints = GpxParser::parse(filePath);

  if (m_trackpoints.isEmpty()) {
    emit gpxLoadError(GpxParser::lastError());
    return false;
  }

  m_gpxFilePath = filePath;

  // Calculate adaptive max time diff if needed
  double avgInterval = GpxParser::calculateAverageInterval(m_trackpoints);
  qInfo() << "Loaded GPX with" << m_trackpoints.size() << "trackpoints,"
          << "avg interval:" << avgInterval << "seconds";

  emit gpxLoaded(m_trackpoints.size());
  return true;
}

void PhotoProcessor::scanPhotos(const QStringList &filePaths,
                                PhotoListModel *model) {
  QVector<PhotoItem> items;
  int skippedDuplicates = 0;

  for (const QString &path : filePaths) {
    if (!ExifHandler::isSupported(path)) {
      qInfo() << "Skipping unsupported file:" << path;
      continue;
    }

    // Skip duplicates
    if (model->containsFile(path)) {
      qInfo() << "Skipping duplicate file:" << path;
      ++skippedDuplicates;
      continue;
    }

    PhotoItem item;
    item.filePath = path;
    item.fileName = QFileInfo(path).fileName();
    item.hasExistingGps = ExifHandler::hasGpsData(path);
    item.state = PhotoState::Pending;

    items.append(item);
  }

  model->addPhotos(items);
  if (skippedDuplicates > 0) {
    qInfo() << "Skipped" << skippedDuplicates << "duplicate file(s)";
  }
  emit photosScanComplete(items.size());
}

void PhotoProcessor::processPhotos(PhotoListModel *model,
                                   const ProcessingSettings &settings) {
  if (m_trackpoints.isEmpty()) {
    qWarning() << "No GPX trackpoints loaded";
    emit processingComplete(0, model->count());
    return;
  }

  m_stopRequested = false;

  // Calculate effective max time diff
  double maxTimeDiff = settings.maxTimeDiffSeconds;
  if (maxTimeDiff <= 0) {
    double avgInterval = GpxParser::calculateAverageInterval(m_trackpoints);
    maxTimeDiff = std::max(60.0, std::min(avgInterval * 3.0, 600.0));
  }

  GpsMatcher matcher(m_trackpoints, maxTimeDiff, settings.forceInterpolate);
  double timeOffsetSeconds = settings.timeOffsetHours * 3600.0;

  qInfo() << "Processing" << model->count() << "photos with settings:"
          << "maxTimeDiff=" << maxTimeDiff
          << "timeOffset=" << settings.timeOffsetHours << "h"
          << "overwrite=" << settings.overwriteExistingGps
          << "forceInterpolate=" << settings.forceInterpolate
          << "dryRun=" << settings.dryRun;

  int successCount = 0;

  for (int i = 0; i < model->count(); ++i) {
    if (m_stopRequested) {
      qInfo() << "Processing stopped by user";
      break;
    }

    emit progressUpdated(i + 1, model->count());

    PhotoItem photo = model->photos()[i];
    photo.state = PhotoState::Processing;
    model->updatePhoto(i, photo);

    // Check format support level
    FormatInfo formatInfo = ExifHandler::getFormatInfo(photo.filePath);

    // Skip files with no metadata support
    if (formatInfo.level == FormatSupportLevel::Minimal) {
      photo.state = PhotoState::Skipped;
      photo.errorMessage = "No metadata support for this format";
      model->updatePhoto(i, photo);
      emit photoProcessed(i, false);
      continue;
    }

    // Skip if already has GPS and not overwriting
    if (photo.hasExistingGps && !settings.overwriteExistingGps) {
      photo.state = PhotoState::Skipped;
      photo.errorMessage = "Already has GPS data";
      model->updatePhoto(i, photo);
      emit photoProcessed(i, false);
      continue;
    }

    // Get photo timestamp
    auto timestamp =
        ExifHandler::getPhotoTimestamp(photo.filePath, timeOffsetSeconds);
    if (!timestamp.has_value()) {
      photo.state = PhotoState::Skipped;
      photo.errorMessage = "No timestamp found";
      model->updatePhoto(i, photo);
      emit photoProcessed(i, false);
      continue;
    }

    photo.captureTime = timestamp.value();

    // Find GPS coordinates
    auto gpsResult = matcher.findGpsForPhoto(photo.captureTime);
    if (!gpsResult.has_value()) {
      photo.state = PhotoState::Skipped;
      if (matcher.isWithinTrackRange(photo.captureTime)) {
        photo.errorMessage = "No GPS match within time threshold";
      } else {
        photo.errorMessage = "Photo time outside GPX range";
      }
      model->updatePhoto(i, photo);
      emit photoProcessed(i, false);
      continue;
    }

    auto [lat, lon, elevation] = gpsResult.value();
    photo.matchedLat = lat;
    photo.matchedLon = lon;
    photo.matchedElevation = elevation;

    // Write GPS data (unless dry run)
    if (!settings.dryRun) {
      bool writeSuccess = false;

      if (formatInfo.level == FormatSupportLevel::NeedsExifTool) {
        // Use exiftool for BMFF formats
        if (!ExifToolWriter::isAvailable()) {
          photo.state = PhotoState::Error;
          photo.errorMessage =
              "exiftool not found - install it to write to this format";
          model->updatePhoto(i, photo);
          emit photoProcessed(i, false);
          continue;
        }
        writeSuccess =
            ExifToolWriter::writeGpsData(photo.filePath, lat, lon, elevation);
        if (!writeSuccess) {
          photo.state = PhotoState::Error;
          photo.errorMessage = ExifToolWriter::lastError();
          model->updatePhoto(i, photo);
          emit photoProcessed(i, false);
          continue;
        }
      } else {
        // Use exiv2 for FullWrite and DangerousRAW formats
        writeSuccess =
            ExifHandler::writeGpsData(photo.filePath, lat, lon, elevation);
        if (!writeSuccess) {
          photo.state = PhotoState::Error;
          photo.errorMessage = ExifHandler::lastError();
          model->updatePhoto(i, photo);
          emit photoProcessed(i, false);
          continue;
        }
      }
    }

    photo.state = PhotoState::Success;
    model->updatePhoto(i, photo);
    emit photoProcessed(i, true);
    ++successCount;
  }

  qInfo() << "Processing complete:" << successCount << "/" << model->count()
          << "photos updated";
  emit processingComplete(successCount, model->count());
}

void PhotoProcessor::stopProcessing() { m_stopRequested = true; }

} // namespace lyp
