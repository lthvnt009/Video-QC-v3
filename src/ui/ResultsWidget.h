// src/ui/resultswidget.h (Đã cải tiến theo Yêu cầu #2)
#ifndef RESULTSWIDGET_H
#define RESULTSWIDGET_H

#include <QWidget>
#include <QList>
#include "core/types.h"

// Forward declarations
class QTreeView;
class QStandardItemModel;
class QPushButton;
class QCheckBox;
class QMenu;
class QAction;
class ClickableHeaderView; // Thêm lớp header tùy chỉnh

class ResultsWidget : public QWidget
{
    Q_OBJECT

public:
    explicit ResultsWidget(QWidget *parent = nullptr);
    void handleResults(const QList<AnalysisResult> &newResults);
    void clearResults();
    const QList<AnalysisResult>& getCurrentResults() const;

public slots:
    void setCurrentFps(double fps);

signals:
    void exportTxtClicked();
    void copyToClipboardClicked();
    void settingsClicked();
    void errorDoubleClicked(int frameNum);

private slots:
    void onTreeViewDoubleClicked(const QModelIndex &index);
    void onDisplayOptionsChanged(); 
    // Slot mới để xử lý click vào header
    void onHeaderClicked(int logicalIndex, const QPoint& pos);
    // Slot mới để xử lý khi một định dạng được chọn từ menu
    void onTimecodeFormatSelected(QAction* action);


private:
    void setupUI();
    void setupTimecodeMenu(); // Hàm mới để khởi tạo menu
    void updateResultsView(); 
    void updateButtonStates();
    QString getFormattedTime(const AnalysisResult& res) const;

    // UI Elements
    QTreeView *m_resultsTreeView;
    ClickableHeaderView *m_headerView; // Sử dụng header tùy chỉnh
    QStandardItemModel *m_resultsModel;
    QPushButton *m_settingsButton;
    QPushButton *m_exportTxtButton;
    QPushButton *m_copyButton;

    // Loại bỏ QComboBox, thay bằng QMenu
    QMenu* m_timecodeFormatMenu;
    QCheckBox *m_filterBlackFramesCheck;
    QCheckBox *m_filterBlackBordersCheck;
    QCheckBox *m_filterOrphanFramesCheck;

    // State
    QList<AnalysisResult> m_currentResults; // Dữ liệu gốc
    double m_currentFps = 0.0;
    int m_currentTimecodeFormat = 0; // Lưu trạng thái định dạng hiện tại
};

#endif // RESULTSWIDGET_H
