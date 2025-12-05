#include "file_list_panel.h"
#include "models/photo_list_model.h"
#include <QAction>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFileInfo>
#include <QFrame>
#include <QHBoxLayout>
#include <QMenu>
#include <QMimeData>
#include <QPainter>
#include <QStyledItemDelegate>
#include <QUrl>
#include <QVBoxLayout>

namespace lyp {

/**
 * @brief Custom delegate for rendering photo items with state indicators.
 */
class PhotoItemDelegate : public QStyledItemDelegate {
public:
  using QStyledItemDelegate::QStyledItemDelegate;

  void paint(QPainter *painter, const QStyleOptionViewItem &option,
             const QModelIndex &index) const override {
    QStyleOptionViewItem opt = option;
    initStyleOption(&opt, index);

    // Draw background
    if (opt.state & QStyle::State_Selected) {
      painter->fillRect(opt.rect, opt.palette.highlight());
    } else if (opt.state & QStyle::State_MouseOver) {
      painter->fillRect(opt.rect, opt.palette.midlight());
    }

    // Get state
    int state = index.data(PhotoListModel::StateRole).toInt();
    QString fileName = index.data(PhotoListModel::FileNameRole).toString();
    bool hasGps = index.data(PhotoListModel::HasGpsRole).toBool();

    // State indicator color
    QColor stateColor;
    QString stateChar;
    switch (static_cast<PhotoState>(state)) {
    case PhotoState::Pending:
      stateColor = Qt::gray;
      stateChar = "○";
      break;
    case PhotoState::Processing:
      stateColor = Qt::blue;
      stateChar = "◐";
      break;
    case PhotoState::Success:
      stateColor = Qt::green;
      stateChar = "✓";
      break;
    case PhotoState::Skipped:
      stateColor = Qt::yellow;
      stateChar = "⊘";
      break;
    case PhotoState::Error:
      stateColor = Qt::red;
      stateChar = "✗";
      break;
    }

    // Draw state indicator
    painter->save();
    painter->setPen(stateColor);
    QFont font = painter->font();
    font.setPointSize(12);
    painter->setFont(font);
    painter->drawText(opt.rect.adjusted(4, 0, 0, 0),
                      Qt::AlignVCenter | Qt::AlignLeft, stateChar);
    painter->restore();

    // Draw filename
    QRect textRect = opt.rect.adjusted(24, 0, -80, -14);
    painter->drawText(textRect, Qt::AlignVCenter | Qt::AlignLeft, fileName);

    // Draw error message or status on second line
    QString errorMsg = index.data(PhotoListModel::ErrorMessageRole).toString();
    if (!errorMsg.isEmpty()) {
      painter->save();
      painter->setPen(Qt::darkGray);
      QFont smallFont = painter->font();
      smallFont.setPointSize(8);
      painter->setFont(smallFont);
      QRect errorRect = opt.rect.adjusted(24, 14, -4, 0);
      QString elidedError = painter->fontMetrics().elidedText(
          errorMsg, Qt::ElideRight, errorRect.width());
      painter->drawText(errorRect, Qt::AlignVCenter | Qt::AlignLeft,
                        elidedError);
      painter->restore();
    }

    // Draw GPS indicator if has existing GPS
    if (hasGps && static_cast<PhotoState>(state) == PhotoState::Pending) {
      painter->save();
      painter->setPen(Qt::darkGreen);
      QFont smallFont = painter->font();
      smallFont.setPointSize(9);
      painter->setFont(smallFont);
      painter->drawText(opt.rect.adjusted(0, 0, -4, -14),
                        Qt::AlignVCenter | Qt::AlignRight, "GPS");
      painter->restore();
    }
  }

  QSize sizeHint(const QStyleOptionViewItem &option,
                 const QModelIndex &index) const override {
    Q_UNUSED(option)
    Q_UNUSED(index)
    return QSize(200, 40); // Taller to show error message
  }
};

FileListPanel::FileListPanel(QWidget *parent) : QWidget(parent) {
  setupUi();
  setAcceptDrops(true);
}

void FileListPanel::setupUi() {
  auto *layout = new QVBoxLayout(this);
  layout->setContentsMargins(8, 8, 8, 8);
  layout->setSpacing(8);

  setupWorkflowPanel(layout);
  setupSettingsPanel(layout);
  setupPhotoList(layout);
}

void FileListPanel::setupWorkflowPanel(QLayout *parentLayout) {
  m_workflowGroup = new QGroupBox("Workflow", this);
  auto *layout = new QVBoxLayout(m_workflowGroup);
  layout->setSpacing(6);

  // Step 1: Load GPX
  auto *step1Layout = new QHBoxLayout();
  m_step1Label = new QLabel("① Load GPX", this);
  m_step1Label->setStyleSheet("font-weight: bold; color: #666;");
  step1Layout->addWidget(m_step1Label);
  step1Layout->addStretch();
  m_loadGpxButton = new QPushButton("Load...", this);
  m_loadGpxButton->setMaximumWidth(80);
  m_loadGpxButton->setToolTip("Load a GPX trace file");
  connect(m_loadGpxButton, &QPushButton::clicked, this,
          &FileListPanel::gpxLoadRequested);
  step1Layout->addWidget(m_loadGpxButton);
  layout->addLayout(step1Layout);

  m_gpxStatusLabel = new QLabel("No GPX loaded", this);
  m_gpxStatusLabel->setStyleSheet(
      "color: #999; font-size: 11px; margin-left: 16px;");
  layout->addWidget(m_gpxStatusLabel);

  // Separator
  auto *sep1 = new QFrame(this);
  sep1->setFrameShape(QFrame::HLine);
  sep1->setStyleSheet("color: #ddd;");
  layout->addWidget(sep1);

  // Step 2: Add Photos
  auto *step2Layout = new QHBoxLayout();
  m_step2Label = new QLabel("② Add Photos", this);
  m_step2Label->setStyleSheet("font-weight: bold; color: #666;");
  step2Layout->addWidget(m_step2Label);
  step2Layout->addStretch();
  m_addPhotosButton = new QPushButton("Add...", this);
  m_addPhotosButton->setMaximumWidth(80);
  m_addPhotosButton->setToolTip(
      "Add photos from file dialog or drag & drop below");
  connect(m_addPhotosButton, &QPushButton::clicked, this,
          &FileListPanel::addPhotosRequested);
  step2Layout->addWidget(m_addPhotosButton);
  layout->addLayout(step2Layout);

  m_photosStatusLabel = new QLabel("No photos added", this);
  m_photosStatusLabel->setStyleSheet(
      "color: #999; font-size: 11px; margin-left: 16px;");
  layout->addWidget(m_photosStatusLabel);

  // Separator
  auto *sep2 = new QFrame(this);
  sep2->setFrameShape(QFrame::HLine);
  sep2->setStyleSheet("color: #ddd;");
  layout->addWidget(sep2);

  // Step 3: Process
  auto *step3Layout = new QHBoxLayout();
  m_step3Label = new QLabel("③ Process", this);
  m_step3Label->setStyleSheet("font-weight: bold; color: #666;");
  step3Layout->addWidget(m_step3Label);
  step3Layout->addStretch();
  m_processButton = new QPushButton("Process", this);
  m_processButton->setMaximumWidth(80);
  m_processButton->setEnabled(false);
  m_processButton->setToolTip("Add GPS coordinates to photos");
  connect(m_processButton, &QPushButton::clicked, this,
          &FileListPanel::processRequested);
  step3Layout->addWidget(m_processButton);
  layout->addLayout(step3Layout);

  parentLayout->addWidget(m_workflowGroup);
}

void FileListPanel::setupSettingsPanel(QLayout *parentLayout) {
  m_settingsGroup = new QGroupBox("Settings", this);
  auto *layout = new QVBoxLayout(m_settingsGroup);
  layout->setSpacing(6);

  // Timezone hint (hidden by default, shown when GPX loaded)
  m_timezoneHintLabel = new QLabel(this);
  m_timezoneHintLabel->setWordWrap(true);
  m_timezoneHintLabel->setStyleSheet(
      "background-color: #fff3cd; color: #856404; padding: 6px; "
      "border-radius: 4px; font-size: 11px;");
  m_timezoneHintLabel->setVisible(false);
  layout->addWidget(m_timezoneHintLabel);

  // Time offset with clearer label
  auto *timeLayout = new QHBoxLayout();
  auto *timeLabel = new QLabel("Camera Timezone:", this);
  timeLabel->setToolTip(
      "Your camera's timezone setting when photos were taken.\n"
      "This is used to align photo timestamps with GPX track times.");
  timeLayout->addWidget(timeLabel);

  m_timeOffsetSpinBox = new QDoubleSpinBox(this);
  m_timeOffsetSpinBox->setRange(-12.0, 14.0);
  m_timeOffsetSpinBox->setDecimals(1);
  m_timeOffsetSpinBox->setSuffix(" h");
  m_timeOffsetSpinBox->setValue(0.0);
  m_timeOffsetSpinBox->setToolTip(
      "Offset from UTC in hours. Common values:\n"
      "  • +8 = China, Singapore, Philippines\n"
      "  • +9 = Japan, Korea\n"
      "  • +1 = Central Europe (winter)\n"
      "  • -5 = US Eastern (winter)\n"
      "  • -8 = US Pacific (winter)\n\n"
      "If your photos don't match the track, try adjusting this value.");
  connect(m_timeOffsetSpinBox,
          QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
          &FileListPanel::settingsChanged);
  connect(m_timeOffsetSpinBox,
          QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
          [this](double) { m_timezoneHintLabel->setVisible(false); });
  timeLayout->addWidget(m_timeOffsetSpinBox);
  timeLayout->addStretch();
  layout->addLayout(timeLayout);

  // Dry run checkbox with clearer text
  m_dryRunCheck = new QCheckBox("Preview only (don't modify files)", this);
  m_dryRunCheck->setToolTip(
      "When enabled, shows what would happen without actually\n"
      "writing GPS data to your photos. Useful for testing settings.");
  connect(m_dryRunCheck, &QCheckBox::toggled, this,
          &FileListPanel::settingsChanged);
  layout->addWidget(m_dryRunCheck);

  // Overwrite GPS checkbox with clearer text
  m_overwriteGpsCheck = new QCheckBox("Replace existing GPS data", this);
  m_overwriteGpsCheck->setToolTip(
      "By default, photos that already have GPS coordinates are skipped.\n"
      "Enable this to overwrite their GPS data with new coordinates from the "
      "track.");
  connect(m_overwriteGpsCheck, &QCheckBox::toggled, this,
          &FileListPanel::settingsChanged);
  layout->addWidget(m_overwriteGpsCheck);

  // More settings button
  m_moreSettingsButton = new QPushButton("Advanced settings...", this);
  m_moreSettingsButton->setFlat(true);
  m_moreSettingsButton->setStyleSheet("text-align: left; color: #0066cc;");
  m_moreSettingsButton->setToolTip(
      "Configure maximum time difference and force interpolation options");
  connect(m_moreSettingsButton, &QPushButton::clicked, this,
          &FileListPanel::moreSettingsRequested);
  layout->addWidget(m_moreSettingsButton);

  parentLayout->addWidget(m_settingsGroup);
}

void FileListPanel::setupPhotoList(QLayout *parentLayout) {
  // Photos header with clear button
  auto *headerLayout = new QHBoxLayout();
  auto *photosLabel = new QLabel("Photos", this);
  QFont titleFont = photosLabel->font();
  titleFont.setPointSize(11);
  titleFont.setBold(true);
  photosLabel->setFont(titleFont);
  headerLayout->addWidget(photosLabel);
  headerLayout->addStretch();

  m_clearButton = new QPushButton("Clear", this);
  m_clearButton->setEnabled(false);
  m_clearButton->setMaximumWidth(60);
  m_clearButton->setToolTip("Clear all photos from list");
  connect(m_clearButton, &QPushButton::clicked, this,
          &FileListPanel::onClearPhotos);
  headerLayout->addWidget(m_clearButton);
  parentLayout->addItem(headerLayout);

  // List view
  m_listView = new QListView(this);
  m_listView->setItemDelegate(new PhotoItemDelegate(m_listView));
  m_listView->setSelectionMode(QAbstractItemView::ExtendedSelection);
  m_listView->setDragDropMode(QAbstractItemView::DropOnly);
  m_listView->setAcceptDrops(true);
  m_listView->setDropIndicatorShown(true);
  m_listView->setContextMenuPolicy(Qt::CustomContextMenu);
  connect(m_listView, &QListView::customContextMenuRequested, this,
          &FileListPanel::showContextMenu);
  parentLayout->addWidget(m_listView);

  // Status label (drag hint when empty)
  m_statusLabel = new QLabel("Drag and drop photos here", this);
  m_statusLabel->setStyleSheet("color: gray; font-style: italic;");
  m_statusLabel->setAlignment(Qt::AlignCenter);
  parentLayout->addWidget(m_statusLabel);
}

// Settings accessors
double FileListPanel::timeOffsetHours() const {
  return m_timeOffsetSpinBox->value();
}

bool FileListPanel::isDryRun() const { return m_dryRunCheck->isChecked(); }

bool FileListPanel::isOverwriteGps() const {
  return m_overwriteGpsCheck->isChecked();
}

void FileListPanel::setGpxStatus(const QString &filename, int trackpointCount) {
  m_gpxLoaded = true;
  m_gpxStatusLabel->setText(
      QString("✓ %1 (%2 pts)").arg(filename).arg(trackpointCount));
  m_gpxStatusLabel->setStyleSheet(
      "color: #2a2; font-size: 11px; margin-left: 16px;");
  m_step1Label->setStyleSheet("font-weight: bold; color: #2a2;");

  // Show timezone hint if offset is 0
  if (m_timeOffsetSpinBox->value() == 0.0) {
    m_timezoneHintLabel->setText("💡 Tip: Set your camera's timezone above if "
                                 "photos don't match the track location.");
    m_timezoneHintLabel->setVisible(true);
  }

  updateWorkflowSteps();
}

void FileListPanel::clearGpxStatus() {
  m_gpxLoaded = false;
  m_gpxStatusLabel->setText("No GPX loaded");
  m_gpxStatusLabel->setStyleSheet(
      "color: #999; font-size: 11px; margin-left: 16px;");
  m_step1Label->setStyleSheet("font-weight: bold; color: #666;");
  m_timezoneHintLabel->setVisible(false);
  updateWorkflowSteps();
}

void FileListPanel::setModel(PhotoListModel *model) {
  m_model = model;
  m_listView->setModel(model);

  connect(m_listView->selectionModel(), &QItemSelectionModel::selectionChanged,
          this, &FileListPanel::onSelectionChanged);
  connect(model, &PhotoListModel::rowsInserted, this,
          &FileListPanel::updatePhotoCount);
  connect(model, &PhotoListModel::rowsRemoved, this,
          &FileListPanel::updatePhotoCount);
  connect(model, &PhotoListModel::modelReset, this,
          &FileListPanel::updatePhotoCount);
}

void FileListPanel::onSelectionChanged() {
  QModelIndexList selected = m_listView->selectionModel()->selectedIndexes();
  if (!selected.isEmpty()) {
    emit photoSelectionChanged(selected.first().row());
  }
}

void FileListPanel::updatePhotoCount() {
  if (m_model) {
    int count = m_model->count();
    if (count > 0) {
      m_photosStatusLabel->setText(
          QString("%1 photo%2 loaded").arg(count).arg(count > 1 ? "s" : ""));
      m_photosStatusLabel->setStyleSheet(
          "color: #2a2; font-size: 11px; margin-left: 16px;");
      m_step2Label->setStyleSheet("font-weight: bold; color: #2a2;");
      m_statusLabel->setVisible(false);
      m_clearButton->setEnabled(true);
    } else {
      m_photosStatusLabel->setText("No photos added");
      m_photosStatusLabel->setStyleSheet(
          "color: #999; font-size: 11px; margin-left: 16px;");
      m_step2Label->setStyleSheet("font-weight: bold; color: #666;");
      m_statusLabel->setVisible(true);
      m_clearButton->setEnabled(false);
    }
    updateWorkflowSteps();
  }
}

void FileListPanel::updateWorkflowSteps() {
  // Enable Process button only when both GPX and photos are loaded
  bool canProcess = m_gpxLoaded && m_model && m_model->count() > 0;
  m_processButton->setEnabled(canProcess);

  if (canProcess) {
    m_step3Label->setStyleSheet("font-weight: bold; color: #06c;");
  } else {
    m_step3Label->setStyleSheet("font-weight: bold; color: #666;");
  }
}

void FileListPanel::showContextMenu(const QPoint &pos) {
  QModelIndex index = m_listView->indexAt(pos);
  if (!index.isValid())
    return;

  QMenu menu(this);
  QAction *removeAction = menu.addAction("Remove");
  QAction *removeAllAction = menu.addAction("Remove All");

  QAction *selected = menu.exec(m_listView->viewport()->mapToGlobal(pos));
  if (selected == removeAction) {
    QModelIndexList selectedIndexes =
        m_listView->selectionModel()->selectedIndexes();
    // Remove in reverse order to maintain valid indices
    QList<int> rows;
    for (const QModelIndex &idx : selectedIndexes) {
      rows.append(idx.row());
    }
    std::sort(rows.begin(), rows.end(), std::greater<int>());
    for (int row : rows) {
      m_model->removePhoto(row);
    }
  } else if (selected == removeAllAction) {
    onClearPhotos();
  }
}

void FileListPanel::onClearPhotos() {
  if (m_model) {
    m_model->clear();
    emit photosCleared();
  }
}

void FileListPanel::dragEnterEvent(QDragEnterEvent *event) {
  if (event->mimeData()->hasUrls()) {
    event->acceptProposedAction();
  }
}

void FileListPanel::dragMoveEvent(QDragMoveEvent *event) {
  if (event->mimeData()->hasUrls()) {
    event->acceptProposedAction();
  }
}

void FileListPanel::dropEvent(QDropEvent *event) {
  QStringList filePaths;

  for (const QUrl &url : event->mimeData()->urls()) {
    if (url.isLocalFile()) {
      QString path = url.toLocalFile();
      QFileInfo info(path);
      if (info.isFile()) {
        filePaths.append(path);
      }
    }
  }

  if (!filePaths.isEmpty()) {
    emit photosDropped(filePaths);
    event->acceptProposedAction();
  }
}

} // namespace lyp
