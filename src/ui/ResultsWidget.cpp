// src/ui/resultswidget.cpp (Đã cải tiến theo Yêu cầu #3)
#include "resultswidget.h"
#include "clickableheaderview.h" 
#include "core/Constants.h"
#include "qctools/QCToolsManager.h" 
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QTreeView>
#include <QStandardItemModel>
#include <QPushButton>
#include <QHeaderView>
#include <QCheckBox>
#include <QLabel>
#include <QMenu>      
#include <QAction>    
#include <algorithm>

ResultsWidget::ResultsWidget(QWidget *parent)
    : QWidget(parent)
{
    setupUI();
    setupTimecodeMenu();
    updateButtonStates();
}

void ResultsWidget::setupUI()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0,0,0,0);

    QGroupBox *resultsBox = new QGroupBox();
    QVBoxLayout *resultsLayout = new QVBoxLayout(resultsBox);

    // --- Dòng tiêu đề và các bộ lọc ---
    QWidget *titleWidget = new QWidget();
    QHBoxLayout *titleLayout = new QHBoxLayout(titleWidget);
    titleLayout->setContentsMargins(0,0,0,0);

    QLabel *titleLabel = new QLabel("<b>Kết quả Phân tích</b>");
    
    m_filterBlackFramesCheck = new QCheckBox("Frame Đen");
    m_filterBlackBordersCheck = new QCheckBox("Viền Đen");
    m_filterOrphanFramesCheck = new QCheckBox("Frame Dư");
    m_filterBlackFramesCheck->setChecked(true);
    m_filterBlackBordersCheck->setChecked(true);
    m_filterOrphanFramesCheck->setChecked(true);

    titleLayout->addWidget(titleLabel);
    titleLayout->addStretch();
    titleLayout->addWidget(new QLabel("Lọc:"));
    titleLayout->addWidget(m_filterBlackFramesCheck);
    titleLayout->addWidget(m_filterBlackBordersCheck);
    titleLayout->addWidget(m_filterOrphanFramesCheck);

    connect(m_filterBlackFramesCheck, &QCheckBox::stateChanged, this, &ResultsWidget::onDisplayOptionsChanged);
    connect(m_filterBlackBordersCheck, &QCheckBox::stateChanged, this, &ResultsWidget::onDisplayOptionsChanged);
    connect(m_filterOrphanFramesCheck, &QCheckBox::stateChanged, this, &ResultsWidget::onDisplayOptionsChanged);

    resultsLayout->addWidget(titleWidget);

    // --- Bảng kết quả ---
    m_resultsTreeView = new QTreeView;
    m_headerView = new ClickableHeaderView(Qt::Horizontal, m_resultsTreeView);
    m_resultsTreeView->setHeader(m_headerView);
    connect(m_headerView, &ClickableHeaderView::sectionClickedWithPos, this, &ResultsWidget::onHeaderClicked);

    m_resultsModel = new QStandardItemModel(0, 5, this);
    m_resultsModel->setHorizontalHeaderLabels({"Timecode", "Số lượng", "Loại lỗi", "Chi tiết", "ID"});
    m_resultsTreeView->setModel(m_resultsModel);
    m_resultsTreeView->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_headerView->setSectionResizeMode(0, QHeaderView::Interactive);
    m_headerView->setSectionResizeMode(1, QHeaderView::Interactive);
    m_headerView->setSectionResizeMode(2, QHeaderView::Interactive);
    m_headerView->setSectionResizeMode(3, QHeaderView::Stretch);
    m_resultsTreeView->setColumnWidth(0, 140); 
    m_resultsTreeView->setColumnWidth(1, 120);
    m_resultsTreeView->setColumnWidth(2, 100);
    m_resultsTreeView->setColumnHidden(4, true); 
    connect(m_resultsTreeView, &QTreeView::doubleClicked, this, &ResultsWidget::onTreeViewDoubleClicked);

    resultsLayout->addWidget(m_resultsTreeView);

    // --- Các nút chức năng dưới bảng ---
    QHBoxLayout *bottomButtonsLayout = new QHBoxLayout;
    m_settingsButton = new QPushButton("⚙️ Cài đặt");
    m_settingsButton->setToolTip("Mở cài đặt (đường dẫn, ngưỡng lỗi...)");
    connect(m_settingsButton, &QPushButton::clicked, this, &ResultsWidget::settingsClicked);

    m_exportTxtButton = new QPushButton("Xuất TXT");
    m_copyButton = new QPushButton("Sao chép vào Clipboard");
    connect(m_exportTxtButton, &QPushButton::clicked, this, &ResultsWidget::exportTxtClicked);
    connect(m_copyButton, &QPushButton::clicked, this, &ResultsWidget::copyToClipboardClicked);

    bottomButtonsLayout->addWidget(m_settingsButton);
    bottomButtonsLayout->addStretch();
    bottomButtonsLayout->addWidget(m_exportTxtButton);
    bottomButtonsLayout->addWidget(m_copyButton);

    resultsLayout->addLayout(bottomButtonsLayout);
    mainLayout->addWidget(resultsBox);
}

void ResultsWidget::setupTimecodeMenu()
{
    m_timecodeFormatMenu = new QMenu(this);
    QStringList formats = {"Timecode", "Time", "Frame", "Giây (s)", "Phút (m)"};
    for(int i = 0; i < formats.size(); ++i) {
        QAction* action = m_timecodeFormatMenu->addAction(formats[i]);
        action->setData(i);
        action->setCheckable(true);
        if (i == m_currentTimecodeFormat) {
            action->setChecked(true);
        }
    }
    connect(m_timecodeFormatMenu, &QMenu::triggered, this, &ResultsWidget::onTimecodeFormatSelected);
}

void ResultsWidget::onHeaderClicked(int logicalIndex, const QPoint &pos)
{
    if (logicalIndex == 0) {
        for(auto action : m_timecodeFormatMenu->actions()) {
            action->setChecked(action->data().toInt() == m_currentTimecodeFormat);
        }
        m_timecodeFormatMenu->popup(pos);
    }
}

void ResultsWidget::onTimecodeFormatSelected(QAction *action)
{
    m_currentTimecodeFormat = action->data().toInt();
    onDisplayOptionsChanged();
}

void ResultsWidget::setCurrentFps(double fps)
{
    m_currentFps = fps;
}

void ResultsWidget::onDisplayOptionsChanged()
{
    updateResultsView();
}

void ResultsWidget::updateButtonStates()
{
    bool hasResults = !m_currentResults.isEmpty();
    m_exportTxtButton->setEnabled(hasResults);
    m_copyButton->setEnabled(hasResults);
}

QString ResultsWidget::getFormattedTime(const AnalysisResult& res) const
{
    switch (m_currentTimecodeFormat) {
        case 0: return QCToolsManager::frameToTimecodeHHMMSSFF(res.startFrame, m_currentFps);
        case 1: return QCToolsManager::frameToTimecodePrecise(res.startFrame, m_currentFps);
        case 2: return QString::number(res.startFrame);
        case 3: return QCToolsManager::frameToSecondsString(res.startFrame, m_currentFps);
        case 4: return QCToolsManager::frameToMinutesString(res.startFrame, m_currentFps);
        default: return res.timecode;
    }
}

void ResultsWidget::updateResultsView()
{
    m_resultsModel->removeRows(0, m_resultsModel->rowCount());

    bool showBlackFrames = m_filterBlackFramesCheck->isChecked();
    bool showBlackBorders = m_filterBlackBordersCheck->isChecked();
    bool showOrphanFrames = m_filterOrphanFramesCheck->isChecked();

    QString currentFormatText = m_timecodeFormatMenu->actions().at(m_currentTimecodeFormat)->text();
    m_resultsModel->setHorizontalHeaderItem(0, new QStandardItem(currentFormatText));

    for (const auto &res : m_currentResults) {
        bool shouldShow = false;
        if (res.errorType == AppConstants::ERR_BLACK_FRAME && showBlackFrames) shouldShow = true;
        else if (res.errorType == AppConstants::ERR_BLACK_BORDER && showBlackBorders) shouldShow = true;
        else if (res.errorType == AppConstants::ERR_ORPHAN_FRAME && showOrphanFrames) shouldShow = true;

        if (shouldShow) {
            // CẢI TIẾN: Căn giữa cho 2 cột đầu tiên
            QStandardItem *timecodeItem = new QStandardItem(getFormattedTime(res));
            timecodeItem->setTextAlignment(Qt::AlignCenter);

            QStandardItem *durationItem = new QStandardItem(res.duration);
            durationItem->setTextAlignment(Qt::AlignCenter);

            QList<QStandardItem *> rowItems;
            rowItems.append(timecodeItem);
            rowItems.append(durationItem);
            rowItems.append(new QStandardItem(res.errorType));
            rowItems.append(new QStandardItem(res.details));
            rowItems.append(new QStandardItem(QString::number(res.id)));
            m_resultsModel->appendRow(rowItems);
        }
    }
}

void ResultsWidget::onTreeViewDoubleClicked(const QModelIndex &index)
{
    if (!index.isValid()) return;

    QModelIndex idIndex = m_resultsModel->index(index.row(), 4);
    bool ok;
    int resultId = m_resultsModel->data(idIndex).toInt(&ok);
    if (!ok) return;

    for(const auto& res : m_currentResults) {
        if (res.id == resultId) {
            emit errorDoubleClicked(res.startFrame);
            return;
        }
    }
}

void ResultsWidget::handleResults(const QList<AnalysisResult> &newResults)
{
    m_currentResults.clear();
    m_currentResults.append(newResults);
    std::sort(m_currentResults.begin(), m_currentResults.end(), [](const auto& a, const auto& b){
        return a.startFrame < b.startFrame;
    });

    updateResultsView();
    updateButtonStates();
}

void ResultsWidget::clearResults()
{
    m_currentResults.clear();
    updateResultsView();
    updateButtonStates();
}

const QList<AnalysisResult>& ResultsWidget::getCurrentResults() const
{
    return m_currentResults;
}

