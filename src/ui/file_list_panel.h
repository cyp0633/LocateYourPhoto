#pragma once

#include <QWidget>
#include <QListView>
#include <QPushButton>
#include <QLabel>

namespace lyp {

class PhotoListModel;

/**
 * @brief Left panel showing the photo file list with drag-and-drop support.
 */
class FileListPanel : public QWidget {
    Q_OBJECT

public:
    explicit FileListPanel(QWidget* parent = nullptr);
    
    void setModel(PhotoListModel* model);
    PhotoListModel* model() const { return m_model; }

signals:
    void gpxLoadRequested();
    void photosDropped(const QStringList& filePaths);
    void photoSelectionChanged(int index);
    void processRequested();
    void photosCleared();

protected:
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dragMoveEvent(QDragMoveEvent* event) override;
    void dropEvent(QDropEvent* event) override;

private slots:
    void onSelectionChanged();
    void updatePhotoCount();
    void showContextMenu(const QPoint& pos);
    void onClearPhotos();

private:
    void setupUi();
    
    QListView* m_listView;
    QPushButton* m_loadGpxButton;
    QPushButton* m_addPhotosButton;
    QPushButton* m_processButton;
    QPushButton* m_clearButton;
    QLabel* m_statusLabel;
    PhotoListModel* m_model = nullptr;
};

} // namespace lyp
