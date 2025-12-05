#pragma once

#include <QLabel>
#include <QMainWindow>
#include <QProgressBar>
#include <QSplitter>

namespace lyp {

class FileListPanel;
class MapPanel;
class PhotoProcessor;
class PhotoListModel;
struct ProcessingSettings;

/**
 * @brief Main application window with two-column layout.
 */
class MainWindow : public QMainWindow {
  Q_OBJECT

public:
  explicit MainWindow(QWidget *parent = nullptr);
  ~MainWindow();

private slots:
  void onLoadGpx();
  void onAddPhotos();
  void onPhotosDropped(const QStringList &filePaths);
  void onProcessPhotos();
  void onPhotoSelectionChanged(int index);
  void onMoreSettings();

  // Processor signals
  void onGpxLoaded(int trackpointCount);
  void onGpxLoadError(const QString &error);
  void onPhotosScanComplete(int count);
  void onPhotoProcessed(int index, bool success);
  void onProcessingComplete(int successCount, int totalCount);
  void onProgressUpdated(int current, int total);

private:
  void setupUi();
  void setupMenus();
  void createSettingsDialog();
  ProcessingSettings getSettings() const;

  // UI components
  QSplitter *m_splitter;
  FileListPanel *m_fileListPanel;
  MapPanel *m_mapPanel;
  QProgressBar *m_progressBar;
  QLabel *m_statusLabel;

  // Core components
  PhotoProcessor *m_processor;
  PhotoListModel *m_photoModel;

  // Advanced settings (not in panel)
  double m_maxTimeDiff = 0.0; // 0 = auto
  bool m_forceInterpolate = false;

  // Store GPX filename for display
  QString m_gpxFileName;
};

} // namespace lyp
