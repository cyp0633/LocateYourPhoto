#include "file_list_panel.h"
#include "models/photo_list_model.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QUrl>
#include <QFileInfo>
#include <QStyledItemDelegate>
#include <QPainter>

namespace lyp {

/**
 * @brief Custom delegate for rendering photo items with state indicators.
 */
class PhotoItemDelegate : public QStyledItemDelegate {
public:
    using QStyledItemDelegate::QStyledItemDelegate;
    
    void paint(QPainter* painter, const QStyleOptionViewItem& option,
               const QModelIndex& index) const override
    {
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
        painter->drawText(opt.rect.adjusted(4, 0, 0, 0), Qt::AlignVCenter | Qt::AlignLeft, stateChar);
        painter->restore();
        
        // Draw filename
        QRect textRect = opt.rect.adjusted(24, 0, 0, 0);
        painter->drawText(textRect, Qt::AlignVCenter | Qt::AlignLeft, fileName);
        
        // Draw GPS indicator if has existing GPS
        if (hasGps && static_cast<PhotoState>(state) == PhotoState::Pending) {
            painter->save();
            painter->setPen(Qt::darkGreen);
            QFont smallFont = painter->font();
            smallFont.setPointSize(9);
            painter->setFont(smallFont);
            painter->drawText(opt.rect.adjusted(0, 0, -4, 0), Qt::AlignVCenter | Qt::AlignRight, "GPS");
            painter->restore();
        }
    }
    
    QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override
    {
        Q_UNUSED(option)
        Q_UNUSED(index)
        return QSize(200, 28);
    }
};

FileListPanel::FileListPanel(QWidget* parent)
    : QWidget(parent)
{
    setupUi();
    setAcceptDrops(true);
}

void FileListPanel::setupUi()
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(8);
    
    // Title
    auto* titleLabel = new QLabel("Photos", this);
    QFont titleFont = titleLabel->font();
    titleFont.setPointSize(14);
    titleFont.setBold(true);
    titleLabel->setFont(titleFont);
    layout->addWidget(titleLabel);
    
    // Buttons row
    auto* buttonLayout = new QHBoxLayout();
    
    m_loadGpxButton = new QPushButton("Load GPX", this);
    m_loadGpxButton->setToolTip("Load a GPX trace file");
    connect(m_loadGpxButton, &QPushButton::clicked, this, &FileListPanel::gpxLoadRequested);
    buttonLayout->addWidget(m_loadGpxButton);
    
    m_addPhotosButton = new QPushButton("Add Photos", this);
    m_addPhotosButton->setToolTip("Add photos from file dialog");
    buttonLayout->addWidget(m_addPhotosButton);
    
    layout->addLayout(buttonLayout);
    
    // List view
    m_listView = new QListView(this);
    m_listView->setItemDelegate(new PhotoItemDelegate(m_listView));
    m_listView->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_listView->setDragDropMode(QAbstractItemView::DropOnly);
    m_listView->setAcceptDrops(true);
    m_listView->setDropIndicatorShown(true);
    layout->addWidget(m_listView, 1);
    
    // Status label
    m_statusLabel = new QLabel("Drag and drop photos here", this);
    m_statusLabel->setStyleSheet("color: gray; font-style: italic;");
    m_statusLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(m_statusLabel);
    
    // Process button
    m_processButton = new QPushButton("Process Photos", this);
    m_processButton->setEnabled(false);
    m_processButton->setToolTip("Add GPS coordinates to photos");
    connect(m_processButton, &QPushButton::clicked, this, &FileListPanel::processRequested);
    layout->addWidget(m_processButton);
}

void FileListPanel::setModel(PhotoListModel* model)
{
    m_model = model;
    m_listView->setModel(model);
    
    connect(m_listView->selectionModel(), &QItemSelectionModel::selectionChanged,
            this, &FileListPanel::onSelectionChanged);
    connect(model, &PhotoListModel::rowsInserted, this, &FileListPanel::updatePhotoCount);
    connect(model, &PhotoListModel::modelReset, this, &FileListPanel::updatePhotoCount);
}

void FileListPanel::onSelectionChanged()
{
    QModelIndexList selected = m_listView->selectionModel()->selectedIndexes();
    if (!selected.isEmpty()) {
        emit photoSelectionChanged(selected.first().row());
    }
}

void FileListPanel::updatePhotoCount()
{
    if (m_model) {
        int count = m_model->count();
        if (count > 0) {
            m_statusLabel->setText(QString("%1 photos").arg(count));
            m_processButton->setEnabled(true);
        } else {
            m_statusLabel->setText("Drag and drop photos here");
            m_processButton->setEnabled(false);
        }
    }
}

void FileListPanel::dragEnterEvent(QDragEnterEvent* event)
{
    if (event->mimeData()->hasUrls()) {
        event->acceptProposedAction();
    }
}

void FileListPanel::dragMoveEvent(QDragMoveEvent* event)
{
    if (event->mimeData()->hasUrls()) {
        event->acceptProposedAction();
    }
}

void FileListPanel::dropEvent(QDropEvent* event)
{
    QStringList filePaths;
    
    for (const QUrl& url : event->mimeData()->urls()) {
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
