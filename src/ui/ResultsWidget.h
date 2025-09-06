// src/ui/resultswidget.h (v7.2 - UI Refinements)
#ifndef RESULTSWIDGET_H
#define RESULTSWIDGET_H

#include <QWidget>
#include <QList>
#include "core/types.h"

// Forward declarations
class QTreeView;
class QStandardItemModel;
class QPushButton;
class QCheckBox; // Thêm QCheckBox

class ResultsWidget : public QWidget
{
    Q_OBJECT

public:
    explicit ResultsWidget(QWidget *parent = nullptr);
    void handleResults(const QList<AnalysisResult> &newResults);
    void clearResults();
    const QList<AnalysisResult>& getCurrentResults() const;

signals:
    void exportTxtClicked();
    void copyToClipboardClicked();
    void settingsClicked();
    void errorDoubleClicked(int frameNum);

private slots:
    void onTreeViewDoubleClicked(const QModelIndex &index);
    void onFilterChanged(); // Slot mới để xử lý việc lọc kết quả

private:
    void setupUI();
    void updateResultsView(); // Hàm mới để cập nhật bảng kết quả
    void updateButtonStates(); // C-04: Hàm mới để cập nhật trạng thái nút

    // UI Elements
    QTreeView *m_resultsTreeView;
    QStandardItemModel *m_resultsModel;
    QPushButton *m_settingsButton;
    QPushButton *m_exportTxtButton;
    QPushButton *m_copyButton;

    // CẢI TIẾN: Thêm các checkbox để lọc
    QCheckBox *m_filterBlackFramesCheck;
    QCheckBox *m_filterBlackBordersCheck;
    QCheckBox *m_filterOrphanFramesCheck;

    // State
    QList<AnalysisResult> m_currentResults; // Dữ liệu gốc
};

#endif // RESULTSWIDGET_H
