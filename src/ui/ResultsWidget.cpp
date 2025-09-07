// src/ui/resultswidget.cpp
#include "resultswidget.h"
#include "core/Constants.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QTreeView>
#include <QStandardItemModel>
#include <QPushButton>
#include <QHeaderView>
#include <QCheckBox>
#include <QLabel>
#include <algorithm>

ResultsWidget::ResultsWidget(QWidget *parent)
    : QWidget(parent)
{
    setupUI();
    updateButtonStates();
}

void ResultsWidget::setupUI()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0,0,0,0);

    QGroupBox *resultsBox = new QGroupBox();
    QVBoxLayout *resultsLayout = new QVBoxLayout(resultsBox);

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
    titleLayout->addWidget(new QLabel("Lọc hiển thị:"));
    titleLayout->addWidget(m_filterBlackFramesCheck);
    titleLayout->addWidget(m_filterBlackBordersCheck);
    titleLayout->addWidget(m_filterOrphanFramesCheck);

    connect(m_filterBlackFramesCheck, &QCheckBox::stateChanged, this, &ResultsWidget::onFilterChanged);
    connect(m_filterBlackBordersCheck, &QCheckBox::stateChanged, this, &ResultsWidget::onFilterChanged);
    connect(m_filterOrphanFramesCheck, &QCheckBox::stateChanged, this, &ResultsWidget::onFilterChanged);

    resultsLayout->addWidget(titleWidget);

    m_resultsTreeView = new QTreeView;
    m_resultsModel = new QStandardItemModel(0, 5, this);
    m_resultsModel->setHorizontalHeaderLabels({"Timecode", "Thời lượng (frames)", "Loại lỗi", "Chi tiết", "ID"});
    m_resultsTreeView->setModel(m_resultsModel);
    m_resultsTreeView->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_resultsTreeView->header()->setSectionResizeMode(0, QHeaderView::Interactive);
    m_resultsTreeView->header()->setSectionResizeMode(1, QHeaderView::Interactive);
    m_resultsTreeView->header()->setSectionResizeMode(2, QHeaderView::Interactive);
    m_resultsTreeView->header()->setSectionResizeMode(3, QHeaderView::Stretch);
    m_resultsTreeView->setColumnWidth(0, 120);
    m_resultsTreeView->setColumnWidth(1, 120);
    m_resultsTreeView->setColumnWidth(2, 100);
    m_resultsTreeView->setColumnHidden(4, true); // Ẩn cột ID
    connect(m_resultsTreeView, &QTreeView::doubleClicked, this, &ResultsWidget::onTreeViewDoubleClicked);

    resultsLayout->addWidget(m_resultsTreeView);

    QHBoxLayout *bottomButtonsLayout = new QHBoxLayout;

    // VIỆT HÓA: Thay đổi văn bản và tooltip của nút
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

void ResultsWidget::onFilterChanged()
{
    updateResultsView();
}

void ResultsWidget::updateButtonStates()
{
    bool hasResults = !m_currentResults.isEmpty();
    m_exportTxtButton->setEnabled(hasResults);
    m_copyButton->setEnabled(hasResults);
}


void ResultsWidget::updateResultsView()
{
    m_resultsModel->removeRows(0, m_resultsModel->rowCount());

    bool showBlackFrames = m_filterBlackFramesCheck->isChecked();
    bool showBlackBorders = m_filterBlackBordersCheck->isChecked();
    bool showOrphanFrames = m_filterOrphanFramesCheck->isChecked();

    for (const auto &res : m_currentResults) {
        bool shouldShow = false;
        if (res.errorType == AppConstants::ERR_BLACK_FRAME && showBlackFrames) shouldShow = true;
        else if (res.errorType == AppConstants::ERR_BLACK_BORDER && showBlackBorders) shouldShow = true;
        else if (res.errorType == AppConstants::ERR_ORPHAN_FRAME && showOrphanFrames) shouldShow = true;

        if (shouldShow) {
            QList<QStandardItem *> rowItems;
            rowItems.append(new QStandardItem(res.timecode));
            rowItems.append(new QStandardItem(res.duration));
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
