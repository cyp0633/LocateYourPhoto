#include "main_window.h"
#include "core/exif_handler.h"
#include "core/exiftool_writer.h"
#include "core/photo_processor.h"
#include "file_list_panel.h"
#include "map_panel.h"
#include "models/photo_list_model.h"
#include <QAction>
#include <QCheckBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QMap>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QSettings>
#include <QStatusBar>

namespace lyp {

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), m_processor(new PhotoProcessor(this)),
      m_photoModel(new PhotoListModel(this)) {
  setupUi();
  setupMenus();

  // Connect processor signals
  connect(m_processor, &PhotoProcessor::gpxLoaded, this,
          &MainWindow::onGpxLoaded);
  connect(m_processor, &PhotoProcessor::gpxLoadError, this,
          &MainWindow::onGpxLoadError);
  connect(m_processor, &PhotoProcessor::photosScanComplete, this,
          &MainWindow::onPhotosScanComplete);
  connect(m_processor, &PhotoProcessor::photoProcessed, this,
          &MainWindow::onPhotoProcessed);
  connect(m_processor, &PhotoProcessor::processingComplete, this,
          &MainWindow::onProcessingComplete);
  connect(m_processor, &PhotoProcessor::progressUpdated, this,
          &MainWindow::onProgressUpdated);

  // Connect model to map panel
  connect(m_photoModel, &PhotoListModel::photoUpdated, [this](int index) {
    m_mapPanel->updatePhotoMarker(index, m_photoModel->photos()[index]);
  });
}

MainWindow::~MainWindow() = default;

void MainWindow::setupUi() {
  setWindowTitle("LocateYourPhoto");
  resize(1200, 800);

  // Central widget with splitter
  m_splitter = new QSplitter(Qt::Horizontal, this);
  setCentralWidget(m_splitter);

  // Left panel (file list with workflow) - 30% width
  m_fileListPanel = new FileListPanel(this);
  m_fileListPanel->setModel(m_photoModel);
  m_splitter->addWidget(m_fileListPanel);

  // Right panel (map) - 70% width
  m_mapPanel = new MapPanel(this);
  m_splitter->addWidget(m_mapPanel);

  m_splitter->setSizes({350, 850});

  // Status bar
  m_statusLabel = new QLabel("Ready — Load a GPX file to begin", this);
  statusBar()->addWidget(m_statusLabel, 1);

  m_progressBar = new QProgressBar(this);
  m_progressBar->setMaximumWidth(200);
  m_progressBar->setVisible(false);
  statusBar()->addPermanentWidget(m_progressBar);

  // Connect file list panel signals
  connect(m_fileListPanel, &FileListPanel::gpxLoadRequested, this,
          &MainWindow::onLoadGpx);
  connect(m_fileListPanel, &FileListPanel::addPhotosRequested, this,
          &MainWindow::onAddPhotos);
  connect(m_fileListPanel, &FileListPanel::photosDropped, this,
          &MainWindow::onPhotosDropped);
  connect(m_fileListPanel, &FileListPanel::photoSelectionChanged, this,
          &MainWindow::onPhotoSelectionChanged);
  connect(m_fileListPanel, &FileListPanel::processRequested, this,
          &MainWindow::onProcessPhotos);
  connect(m_fileListPanel, &FileListPanel::photosCleared, m_mapPanel,
          &MapPanel::clearPhotoMarkers);
  connect(m_fileListPanel, &FileListPanel::moreSettingsRequested, this,
          &MainWindow::onMoreSettings);
}

void MainWindow::setupMenus() {
  // File menu
  QMenu *fileMenu = menuBar()->addMenu("&File");

  QAction *loadGpxAction = fileMenu->addAction("Load &GPX...");
  loadGpxAction->setShortcut(QKeySequence("Ctrl+G"));
  connect(loadGpxAction, &QAction::triggered, this, &MainWindow::onLoadGpx);

  QAction *addPhotosAction = fileMenu->addAction("&Add Photos...");
  addPhotosAction->setShortcut(QKeySequence("Ctrl+O"));
  connect(addPhotosAction, &QAction::triggered, this, &MainWindow::onAddPhotos);

  fileMenu->addSeparator();

  QAction *quitAction = fileMenu->addAction("&Quit");
  quitAction->setShortcut(QKeySequence::Quit);
  connect(quitAction, &QAction::triggered, this, &QMainWindow::close);

  // Settings menu
  QMenu *settingsMenu = menuBar()->addMenu("&Settings");

  QAction *configureAction = settingsMenu->addAction("&Advanced Settings...");
  connect(configureAction, &QAction::triggered, this,
          &MainWindow::onMoreSettings);

  // Help menu
  QMenu *helpMenu = menuBar()->addMenu("&Help");

  QAction *aboutAction = helpMenu->addAction("&About");
  connect(aboutAction, &QAction::triggered, [this]() {
    QMessageBox::about(
        this, "About LocateYourPhoto",
        "LocateYourPhoto v1.0\n\n"
        "Add GPS coordinates to your photos using GPX trace files.\n\n"
        "Workflow:\n"
        "1. Load a GPX trace file\n"
        "2. Add photos (drag & drop or use button)\n"
        "3. Adjust time offset if needed\n"
        "4. Click Process to add GPS data");
  });
}

void MainWindow::onMoreSettings() { createSettingsDialog(); }

void MainWindow::createSettingsDialog() {
  QDialog dialog(this);
  dialog.setWindowTitle("Advanced Settings");

  auto *layout = new QFormLayout(&dialog);

  auto *maxTimeDiffSpin = new QDoubleSpinBox(&dialog);
  maxTimeDiffSpin->setRange(0.0, 3600.0);
  maxTimeDiffSpin->setDecimals(0);
  maxTimeDiffSpin->setSuffix(" seconds");
  maxTimeDiffSpin->setSpecialValueText("Auto");
  maxTimeDiffSpin->setValue(m_maxTimeDiff);
  maxTimeDiffSpin->setToolTip("Maximum time difference for GPS matching.\n0 = "
                              "Automatic (based on GPX interval).");
  layout->addRow("Max Time Diff:", maxTimeDiffSpin);

  auto *forceCheck =
      new QCheckBox("Force interpolate (ignore time threshold)", &dialog);
  forceCheck->setChecked(m_forceInterpolate);
  forceCheck->setToolTip(
      "Always interpolate between trackpoints regardless of time difference.");
  layout->addRow(forceCheck);

  auto *buttonBox = new QDialogButtonBox(
      QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
  connect(buttonBox, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
  connect(buttonBox, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
  layout->addRow(buttonBox);

  if (dialog.exec() == QDialog::Accepted) {
    m_maxTimeDiff = maxTimeDiffSpin->value();
    m_forceInterpolate = forceCheck->isChecked();
  }
}

ProcessingSettings MainWindow::getSettings() const {
  ProcessingSettings settings;
  settings.maxTimeDiffSeconds = m_maxTimeDiff;
  settings.timeOffsetHours = m_fileListPanel->timeOffsetHours();
  settings.overwriteExistingGps = m_fileListPanel->isOverwriteGps();
  settings.forceInterpolate = m_forceInterpolate;
  settings.dryRun = m_fileListPanel->isDryRun();
  return settings;
}

void MainWindow::onLoadGpx() {
  QString filePath = QFileDialog::getOpenFileName(
      this, "Load GPX Trace", QString(), "GPX Files (*.gpx);;All Files (*)");

  if (!filePath.isEmpty()) {
    m_gpxFileName = QFileInfo(filePath).fileName();
    m_statusLabel->setText("Loading GPX file...");
    m_processor->loadGpxFile(filePath);
  }
}

void MainWindow::onAddPhotos() {
  // Build filter from all supported extensions
  QStringList exts = ExifHandler::supportedExtensions();
  QStringList patterns;
  for (const QString &ext : exts) {
    patterns << QString("*.%1").arg(ext);
  }
  QString filter =
      QString("Photo Files (%1);;All Files (*)").arg(patterns.join(" "));

  QStringList filePaths =
      QFileDialog::getOpenFileNames(this, "Add Photos", QString(), filter);

  if (!filePaths.isEmpty()) {
    onPhotosDropped(filePaths);
  }
}

void MainWindow::onPhotosDropped(const QStringList &filePaths) {
  m_statusLabel->setText("Scanning photos...");
  m_processor->scanPhotos(filePaths, m_photoModel);
}

void MainWindow::onProcessPhotos() {
  if (!m_processor->hasGpxLoaded()) {
    QMessageBox::warning(this, "No GPX Loaded",
                         "Please load a GPX trace file first.");
    return;
  }

  if (m_photoModel->count() == 0) {
    QMessageBox::warning(this, "No Photos", "Please add photos to process.");
    return;
  }

  // Check for file formats that need special handling
  QStringList exiftoolFiles;  // BMFF formats needing exiftool
  QStringList dangerousFiles; // Dangerous RAW formats
  QStringList rawFiles;       // FullWrite RAW formats (still need warning)
  QStringList minimalFiles;   // No metadata support
  QMap<QString, QString> formatWarnings;

  for (int i = 0; i < m_photoModel->count(); ++i) {
    const PhotoItem &photo = m_photoModel->photos()[i];
    FormatInfo info = ExifHandler::getFormatInfo(photo.filePath);

    if (info.level == FormatSupportLevel::NeedsExifTool) {
      exiftoolFiles.append(photo.fileName);
      formatWarnings[photo.fileName] = info.warning;
    } else if (info.level == FormatSupportLevel::DangerousRAW) {
      dangerousFiles.append(photo.fileName);
      formatWarnings[photo.fileName] = info.warning;
    } else if (info.level == FormatSupportLevel::Minimal) {
      minimalFiles.append(photo.fileName);
      formatWarnings[photo.fileName] = info.warning;
    } else if (info.level == FormatSupportLevel::FullWrite && 
               ExifHandler::isRawFormat(photo.filePath)) {
      // Also warn about FullWrite RAW files (ARW, NEF, CR2, etc.)
      rawFiles.append(photo.fileName);
      QString ext = QFileInfo(photo.filePath).suffix().toUpper();
      formatWarnings[photo.fileName] = QString("%1 RAW - modification may affect file integrity").arg(ext);
    }
  }

  // Show warning dialog if there are files needing attention
  if (!exiftoolFiles.isEmpty() || !dangerousFiles.isEmpty() ||
      !rawFiles.isEmpty() || !minimalFiles.isEmpty()) {
    QString warningText;

    if (!exiftoolFiles.isEmpty()) {
      bool exiftoolAvailable = ExifToolWriter::isAvailable();
      warningText +=
          QString("<b>%1 file(s) will use external exiftool:</b><br>")
              .arg(exiftoolFiles.size());
      const int maxShow = 10;
      int shown = 0;
      for (const QString &file : exiftoolFiles) {
        if (shown < maxShow) {
          warningText += QString("• %1<br>").arg(file);
          ++shown;
        } else {
          break;
        }
      }
      if (exiftoolFiles.size() > maxShow) {
        warningText += QString("<i>... and %1 more</i><br>")
                           .arg(exiftoolFiles.size() - maxShow);
      }
      if (exiftoolAvailable) {
        warningText += "<br><span style='color: green;'>✓ exiftool is available</span><br>";
      } else {
        warningText += "<br><span style='color: red;'>✗ exiftool is NOT available - these files will fail</span><br>";
      }
    }

    if (!rawFiles.isEmpty()) {
      if (!warningText.isEmpty())
        warningText += "<br>";
      warningText += QString("<b>⚠️ %1 RAW file(s) (modification may affect file integrity):</b><br>")
                         .arg(rawFiles.size());
      const int maxShow = 10;
      int shown = 0;
      for (const QString &file : rawFiles) {
        if (shown < maxShow) {
          warningText +=
              QString(
                  "• %1<br><i style='color: #666; font-size: small;'>%2</i><br>")
                  .arg(file)
                  .arg(formatWarnings[file]);
          ++shown;
        } else {
          break;
        }
      }
      if (rawFiles.size() > maxShow) {
        warningText += QString("<i>... and %1 more</i><br>")
                           .arg(rawFiles.size() - maxShow);
      }
    }

    if (!dangerousFiles.isEmpty()) {
      if (!warningText.isEmpty())
        warningText += "<br>";
      warningText += QString("<b>⚠️ %1 file(s) with risky RAW format:</b><br>")
                         .arg(dangerousFiles.size());
      const int maxShow = 10;
      int shown = 0;
      for (const QString &file : dangerousFiles) {
        if (shown < maxShow) {
          warningText +=
              QString(
                  "• %1<br><i style='color: #666; font-size: small;'>%2</i><br>")
                  .arg(file)
                  .arg(formatWarnings[file]);
          ++shown;
        } else {
          break;
        }
      }
      if (dangerousFiles.size() > maxShow) {
        warningText += QString("<i>... and %1 more</i><br>")
                           .arg(dangerousFiles.size() - maxShow);
      }
    }

    if (!minimalFiles.isEmpty()) {
      if (!warningText.isEmpty())
        warningText += "<br>";
      warningText +=
          QString("<b>%1 file(s) with no metadata support (will skip):</b><br>")
              .arg(minimalFiles.size());
      const int maxShow = 10;
      int shown = 0;
      for (const QString &file : minimalFiles) {
        if (shown < maxShow) {
          warningText += QString("• %1<br>").arg(file);
          ++shown;
        } else {
          break;
        }
      }
      if (minimalFiles.size() > maxShow) {
        warningText += QString("<i>... and %1 more</i><br>")
                           .arg(minimalFiles.size() - maxShow);
      }
    }

    warningText +=
        "<br>These files may fail to write GPS data. Continue anyway?";

    QMessageBox msgBox(this);
    msgBox.setWindowTitle("Format Compatibility Warning");
    msgBox.setIcon(QMessageBox::Warning);
    msgBox.setTextFormat(Qt::RichText);
    msgBox.setText(warningText);
    msgBox.setStandardButtons(QMessageBox::Yes | QMessageBox::Cancel);
    msgBox.setDefaultButton(QMessageBox::Yes);
    
    // Make the dialog scrollable and set reasonable size limits
    msgBox.setMinimumWidth(500);
    msgBox.setMaximumWidth(700);
    msgBox.setMinimumHeight(200);
    msgBox.setMaximumHeight(600);

    if (msgBox.exec() != QMessageBox::Yes) {
      return;
    }
  }

  // Reset all photo states before reprocessing
  m_photoModel->resetAllStates();

  m_mapPanel->clearPhotoMarkers();
  m_progressBar->setVisible(true);
  m_progressBar->setMaximum(m_photoModel->count());
  m_progressBar->setValue(0);
  m_statusLabel->setText("Processing photos...");

  m_processor->processPhotos(m_photoModel, getSettings());
}

void MainWindow::onPhotoSelectionChanged(int index) {
  m_mapPanel->highlightPhoto(index);
}

void MainWindow::onGpxLoaded(int trackpointCount) {
  m_statusLabel->setText(
      QString("GPX loaded: %1 trackpoints").arg(trackpointCount));
  m_fileListPanel->setGpxStatus(m_gpxFileName, trackpointCount);
  m_mapPanel->setTrack(m_processor->trackpoints());
  m_mapPanel->centerOnTrack();
}

void MainWindow::onGpxLoadError(const QString &error) {
  m_statusLabel->setText("Failed to load GPX");
  m_fileListPanel->clearGpxStatus();
  QMessageBox::warning(this, "GPX Load Error", error);
}

void MainWindow::onPhotosScanComplete(int count) {
  m_statusLabel->setText(QString("%1 photos added").arg(count));
}

void MainWindow::onPhotoProcessed(int index, bool success) {
  Q_UNUSED(success)
  const PhotoItem &photo = m_photoModel->photos()[index];
  if (photo.hasMatchedCoordinates()) {
    m_mapPanel->addPhotoMarker(photo);
  }
}

void MainWindow::onProcessingComplete(int successCount, int totalCount) {
  m_progressBar->setVisible(false);
  bool isDryRun = m_fileListPanel->isDryRun();
  QString msg = QString("Complete: %1/%2 photos updated")
                    .arg(successCount)
                    .arg(totalCount);
  if (isDryRun) {
    msg += " (dry run)";
  }
  m_statusLabel->setText(msg);

  QMessageBox::information(
      this, "Processing Complete",
      QString("Successfully processed %1 of %2 photos.%3")
          .arg(successCount)
          .arg(totalCount)
          .arg(isDryRun ? "\n\n(Dry run - no changes were made)" : ""));
}

void MainWindow::onProgressUpdated(int current, int total) {
  m_progressBar->setValue(current);
  m_statusLabel->setText(
      QString("Processing photo %1 of %2...").arg(current).arg(total));
}

} // namespace lyp
