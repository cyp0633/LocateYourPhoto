#pragma once

#include <QCheckBox>
#include <QDoubleSpinBox>
#include <QGroupBox>
#include <QLabel>
#include <QListView>
#include <QPushButton>
#include <QWidget>

namespace lyp {

class PhotoListModel;

/**
 * @brief Left panel with workflow-guided UI showing steps, settings, and photo
 * list.
 */
class FileListPanel : public QWidget {
  Q_OBJECT

public:
  explicit FileListPanel(QWidget *parent = nullptr);

  void setModel(PhotoListModel *model);
  PhotoListModel *model() const { return m_model; }

  // Settings accessors
  double timeOffsetHours() const;
  bool isDryRun() const;
  bool isOverwriteGps() const;

  // Update GPX status display
  void setGpxStatus(const QString &filename, int trackpointCount);
  void clearGpxStatus();

signals:
  void gpxLoadRequested();
  void addPhotosRequested();
  void photosDropped(const QStringList &filePaths);
  void photoSelectionChanged(int index);
  void processRequested();
  void photosCleared();
  void moreSettingsRequested();
  void settingsChanged();

protected:
  void dragEnterEvent(QDragEnterEvent *event) override;
  void dragMoveEvent(QDragMoveEvent *event) override;
  void dropEvent(QDropEvent *event) override;

private slots:
  void onSelectionChanged();
  void updatePhotoCount();
  void showContextMenu(const QPoint &pos);
  void onClearPhotos();
  void updateWorkflowSteps();

private:
  void setupUi();
  void setupWorkflowPanel(QLayout *layout);
  void setupSettingsPanel(QLayout *layout);
  void setupPhotoList(QLayout *layout);

  // Workflow panel
  QGroupBox *m_workflowGroup;
  QLabel *m_step1Label;
  QLabel *m_gpxStatusLabel;
  QPushButton *m_loadGpxButton;
  QLabel *m_step2Label;
  QLabel *m_photosStatusLabel;
  QPushButton *m_addPhotosButton;
  QLabel *m_step3Label;
  QPushButton *m_processButton;

  // Settings panel
  QGroupBox *m_settingsGroup;
  QLabel *m_timezoneHintLabel;
  QDoubleSpinBox *m_timeOffsetSpinBox;
  QCheckBox *m_dryRunCheck;
  QCheckBox *m_overwriteGpsCheck;
  QPushButton *m_moreSettingsButton;

  // Photo list
  QListView *m_listView;
  QPushButton *m_clearButton;
  QLabel *m_statusLabel;

  PhotoListModel *m_model = nullptr;
  bool m_gpxLoaded = false;
};

} // namespace lyp
