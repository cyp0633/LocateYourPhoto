#include "main_window.h"
#include "file_list_panel.h"
#include "map_panel.h"
#include "core/photo_processor.h"
#include "models/photo_list_model.h"
#include <QFileDialog>
#include <QMenuBar>
#include <QMenu>
#include <QAction>
#include <QStatusBar>
#include <QMessageBox>
#include <QInputDialog>
#include <QSettings>
#include <QDialog>
#include <QFormLayout>
#include <QDoubleSpinBox>
#include <QCheckBox>
#include <QDialogButtonBox>

namespace lyp {

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
    , m_processor(new PhotoProcessor(this))
    , m_photoModel(new PhotoListModel(this))
{
    setupUi();
    setupMenus();
    
    // Connect processor signals
    connect(m_processor, &PhotoProcessor::gpxLoaded, this, &MainWindow::onGpxLoaded);
    connect(m_processor, &PhotoProcessor::gpxLoadError, this, &MainWindow::onGpxLoadError);
    connect(m_processor, &PhotoProcessor::photosScanComplete, this, &MainWindow::onPhotosScanComplete);
    connect(m_processor, &PhotoProcessor::photoProcessed, this, &MainWindow::onPhotoProcessed);
    connect(m_processor, &PhotoProcessor::processingComplete, this, &MainWindow::onProcessingComplete);
    connect(m_processor, &PhotoProcessor::progressUpdated, this, &MainWindow::onProgressUpdated);
    
    // Connect model to map panel
    connect(m_photoModel, &PhotoListModel::photoUpdated, 
            [this](int index) {
                m_mapPanel->updatePhotoMarker(index, m_photoModel->photos()[index]);
            });
}

MainWindow::~MainWindow() = default;

void MainWindow::setupUi()
{
    setWindowTitle("LocateYourPhoto");
    resize(1200, 800);
    
    // Central widget with splitter
    m_splitter = new QSplitter(Qt::Horizontal, this);
    setCentralWidget(m_splitter);
    
    // Left panel (file list) - 30% width
    m_fileListPanel = new FileListPanel(this);
    m_fileListPanel->setModel(m_photoModel);
    m_splitter->addWidget(m_fileListPanel);
    
    // Right panel (map) - 70% width
    m_mapPanel = new MapPanel(this);
    m_splitter->addWidget(m_mapPanel);
    
    m_splitter->setSizes({300, 900});
    
    // Status bar
    m_statusLabel = new QLabel("Ready", this);
    statusBar()->addWidget(m_statusLabel, 1);
    
    m_progressBar = new QProgressBar(this);
    m_progressBar->setMaximumWidth(200);
    m_progressBar->setVisible(false);
    statusBar()->addPermanentWidget(m_progressBar);
    
    // Connect file list panel signals
    connect(m_fileListPanel, &FileListPanel::gpxLoadRequested, this, &MainWindow::onLoadGpx);
    connect(m_fileListPanel, &FileListPanel::photosDropped, this, &MainWindow::onPhotosDropped);
    connect(m_fileListPanel, &FileListPanel::photoSelectionChanged, this, &MainWindow::onPhotoSelectionChanged);
    connect(m_fileListPanel, &FileListPanel::processRequested, this, &MainWindow::onProcessPhotos);
}

void MainWindow::setupMenus()
{
    // File menu
    QMenu* fileMenu = menuBar()->addMenu("&File");
    
    QAction* loadGpxAction = fileMenu->addAction("Load &GPX...");
    loadGpxAction->setShortcut(QKeySequence("Ctrl+G"));
    connect(loadGpxAction, &QAction::triggered, this, &MainWindow::onLoadGpx);
    
    QAction* addPhotosAction = fileMenu->addAction("&Add Photos...");
    addPhotosAction->setShortcut(QKeySequence("Ctrl+O"));
    connect(addPhotosAction, &QAction::triggered, this, &MainWindow::onAddPhotos);
    
    fileMenu->addSeparator();
    
    QAction* quitAction = fileMenu->addAction("&Quit");
    quitAction->setShortcut(QKeySequence::Quit);
    connect(quitAction, &QAction::triggered, this, &QMainWindow::close);
    
    // Settings menu
    QMenu* settingsMenu = menuBar()->addMenu("&Settings");
    
    QAction* configureAction = settingsMenu->addAction("&Configure...");
    connect(configureAction, &QAction::triggered, this, &MainWindow::createSettingsDialog);
    
    // Help menu
    QMenu* helpMenu = menuBar()->addMenu("&Help");
    
    QAction* aboutAction = helpMenu->addAction("&About");
    connect(aboutAction, &QAction::triggered, [this]() {
        QMessageBox::about(this, "About LocateYourPhoto",
            "LocateYourPhoto v1.0\n\n"
            "Add GPS coordinates to your photos using GPX trace files.\n\n"
            "Drag and drop photos, load a GPX trace, and click Process.");
    });
}

void MainWindow::createSettingsDialog()
{
    QDialog dialog(this);
    dialog.setWindowTitle("Processing Settings");
    
    auto* layout = new QFormLayout(&dialog);
    
    auto* timeOffsetSpin = new QDoubleSpinBox(&dialog);
    timeOffsetSpin->setRange(-12.0, 14.0);
    timeOffsetSpin->setDecimals(1);
    timeOffsetSpin->setSuffix(" hours");
    timeOffsetSpin->setValue(m_timeOffsetHours);
    timeOffsetSpin->setToolTip("Camera timezone offset from UTC.\nE.g., +8 for Asia/Shanghai, -5 for US Eastern.");
    layout->addRow("Time Offset:", timeOffsetSpin);
    
    auto* maxTimeDiffSpin = new QDoubleSpinBox(&dialog);
    maxTimeDiffSpin->setRange(0.0, 3600.0);
    maxTimeDiffSpin->setDecimals(0);
    maxTimeDiffSpin->setSuffix(" seconds");
    maxTimeDiffSpin->setSpecialValueText("Auto");
    maxTimeDiffSpin->setValue(m_maxTimeDiff);
    maxTimeDiffSpin->setToolTip("Maximum time difference for GPS matching.\n0 = Automatic (based on GPX interval).");
    layout->addRow("Max Time Diff:", maxTimeDiffSpin);
    
    auto* overwriteCheck = new QCheckBox("Overwrite existing GPS data", &dialog);
    overwriteCheck->setChecked(m_overwriteGps);
    layout->addRow(overwriteCheck);
    
    auto* forceCheck = new QCheckBox("Force interpolate (ignore time threshold)", &dialog);
    forceCheck->setChecked(m_forceInterpolate);
    layout->addRow(forceCheck);
    
    auto* dryRunCheck = new QCheckBox("Dry run (preview only)", &dialog);
    dryRunCheck->setChecked(m_dryRun);
    layout->addRow(dryRunCheck);
    
    auto* buttonBox = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    connect(buttonBox, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    layout->addRow(buttonBox);
    
    if (dialog.exec() == QDialog::Accepted) {
        m_timeOffsetHours = timeOffsetSpin->value();
        m_maxTimeDiff = maxTimeDiffSpin->value();
        m_overwriteGps = overwriteCheck->isChecked();
        m_forceInterpolate = forceCheck->isChecked();
        m_dryRun = dryRunCheck->isChecked();
    }
}

ProcessingSettings MainWindow::getSettings() const
{
    ProcessingSettings settings;
    settings.maxTimeDiffSeconds = m_maxTimeDiff;
    settings.timeOffsetHours = m_timeOffsetHours;
    settings.overwriteExistingGps = m_overwriteGps;
    settings.forceInterpolate = m_forceInterpolate;
    settings.dryRun = m_dryRun;
    return settings;
}

void MainWindow::onLoadGpx()
{
    QString filePath = QFileDialog::getOpenFileName(
        this, "Load GPX Trace", QString(),
        "GPX Files (*.gpx);;All Files (*)");
    
    if (!filePath.isEmpty()) {
        m_statusLabel->setText("Loading GPX file...");
        m_processor->loadGpxFile(filePath);
    }
}

void MainWindow::onAddPhotos()
{
    QStringList filePaths = QFileDialog::getOpenFileNames(
        this, "Add Photos", QString(),
        "Photo Files (*.jpg *.jpeg *.arw *.nef *.cr2 *.dng *.heic *.heif);;All Files (*)");
    
    if (!filePaths.isEmpty()) {
        onPhotosDropped(filePaths);
    }
}

void MainWindow::onPhotosDropped(const QStringList& filePaths)
{
    m_statusLabel->setText("Scanning photos...");
    m_processor->scanPhotos(filePaths, m_photoModel);
}

void MainWindow::onProcessPhotos()
{
    if (!m_processor->hasGpxLoaded()) {
        QMessageBox::warning(this, "No GPX Loaded", 
            "Please load a GPX trace file first.");
        return;
    }
    
    if (m_photoModel->count() == 0) {
        QMessageBox::warning(this, "No Photos", 
            "Please add photos to process.");
        return;
    }
    
    m_mapPanel->clearPhotoMarkers();
    m_progressBar->setVisible(true);
    m_progressBar->setMaximum(m_photoModel->count());
    m_progressBar->setValue(0);
    m_statusLabel->setText("Processing photos...");
    
    m_processor->processPhotos(m_photoModel, getSettings());
}

void MainWindow::onPhotoSelectionChanged(int index)
{
    m_mapPanel->highlightPhoto(index);
}

void MainWindow::onGpxLoaded(int trackpointCount)
{
    m_statusLabel->setText(QString("GPX loaded: %1 trackpoints").arg(trackpointCount));
    m_mapPanel->setTrack(m_processor->trackpoints());
    m_mapPanel->centerOnTrack();
}

void MainWindow::onGpxLoadError(const QString& error)
{
    m_statusLabel->setText("Failed to load GPX");
    QMessageBox::warning(this, "GPX Load Error", error);
}

void MainWindow::onPhotosScanComplete(int count)
{
    m_statusLabel->setText(QString("%1 photos added").arg(count));
}

void MainWindow::onPhotoProcessed(int index, bool success)
{
    Q_UNUSED(success)
    const PhotoItem& photo = m_photoModel->photos()[index];
    if (photo.hasMatchedCoordinates()) {
        m_mapPanel->addPhotoMarker(photo);
    }
}

void MainWindow::onProcessingComplete(int successCount, int totalCount)
{
    m_progressBar->setVisible(false);
    QString msg = QString("Complete: %1/%2 photos updated").arg(successCount).arg(totalCount);
    if (m_dryRun) {
        msg += " (dry run)";
    }
    m_statusLabel->setText(msg);
    
    QMessageBox::information(this, "Processing Complete",
        QString("Successfully processed %1 of %2 photos.%3")
            .arg(successCount)
            .arg(totalCount)
            .arg(m_dryRun ? "\n\n(Dry run - no changes were made)" : ""));
}

void MainWindow::onProgressUpdated(int current, int total)
{
    m_progressBar->setValue(current);
    m_statusLabel->setText(QString("Processing photo %1 of %2...").arg(current).arg(total));
}

} // namespace lyp
