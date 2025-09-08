// src/ui/videowidget.cpp (Đã cải tiến theo Yêu cầu #1)
#include "videowidget.h"
#include "configwidget.h"
#include "resultswidget.h"
#include "settingsdialog.h"
#include "logdialog.h"
#include "qctools/QCToolsManager.h"
#include "qctools/QCToolsController.h"
#include "core/Constants.h" 
#include "core/media_info.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QStackedWidget>
#include <QLabel>
#include <QMessageBox>
#include <QFileDialog>
#include <QTextStream>
#include <QApplication>
#include <QClipboard>
#include <QFileInfo>
#include <QStandardPaths>
#include <QDir>
#include <QCryptographicHash>
#include <QTimer>
#include <QSettings> 

VideoWidget::VideoWidget(QWidget *parent)
    : QWidget(parent)
{
    qRegisterMetaType<AnalysisResult>("AnalysisResult");
    qRegisterMetaType<QList<AnalysisResult>>("QList<AnalysisResult>");
    qRegisterMetaType<MediaInfo>("MediaInfo");

    setupUI();
    
    m_statusResetTimer = new QTimer(this);
    m_statusResetTimer->setSingleShot(true);
    connect(m_statusResetTimer, &QTimer::timeout, this, &VideoWidget::onStatusResetTimeout);

    m_analysisThread = new QThread(this);
    m_qctoolsManager = new QCToolsManager();
    m_qctoolsManager->moveToThread(m_analysisThread);
    m_qctoolsController = new QCToolsController(this);

    setupConnections();
    m_analysisThread->start();
    
    handleLogMessage(QString("[%1] Chương trình đã khởi động.").arg(QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss")));
    
    initializePaths();
    
    handleLogMessage("------------------------------------------------------------------");
}

VideoWidget::~VideoWidget()
{
    if(m_analysisThread->isRunning()) {
        m_analysisThread->quit();
        if (!m_analysisThread->wait(3000)) {
            m_analysisThread->terminate();
            m_analysisThread->wait();
        }
    }
    delete m_qctoolsManager;
}

void VideoWidget::setupUI()
{
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(10,10,10,10);
    mainLayout->setSpacing(10);
    
    m_configWidget = new ConfigWidget(this);
    m_resultsWidget = new ResultsWidget(this);

    // --- Control Buttons Layout ---
    QHBoxLayout* controlButtonsLayout = new QHBoxLayout();
    m_analyzeButton = new QPushButton("BẮT ĐẦU PHÂN TÍCH");
    m_analyzeButton->setStyleSheet("font-weight: bold; padding: 5px;");
    m_analyzeButton->setEnabled(false);
    
    m_stopButton = new QPushButton("DỪNG PHÂN TÍCH");
    m_stopButton->setStyleSheet("background-color: #d9534f; color: white; font-weight: bold; padding: 5px; border-radius: 3px;");
    
    m_analysisButtonStack = new QStackedWidget;
    m_analysisButtonStack->addWidget(m_analyzeButton);
    m_analysisButtonStack->addWidget(m_stopButton);
    
    // Nút Log sẽ được chuyển xuống dưới
    controlButtonsLayout->addWidget(m_analysisButtonStack, 1);

    // --- Status & Log Layout (Layout dưới cùng MỚI) ---
    QHBoxLayout* bottomLayout = new QHBoxLayout();
    m_statusLabel = new QLabel("Sẵn sàng. Vui lòng chọn file video hoặc file báo cáo.");
    m_statusLabel->setStyleSheet("font-size: 9pt;");
    m_persistentStatusText = m_statusLabel->text();
    
    m_logButton = new QPushButton("Xem Log");
    
    bottomLayout->addWidget(m_statusLabel, 1); // Label chiếm không gian co giãn
    bottomLayout->addWidget(m_logButton);      // Button ở bên phải

    // --- Assemble Main Layout ---
    mainLayout->addWidget(m_configWidget);
    mainLayout->addLayout(controlButtonsLayout);
    mainLayout->addWidget(m_resultsWidget, 1); 
    mainLayout->addLayout(bottomLayout); // Thêm layout mới vào cuối
}

// ... các hàm còn lại của videowidget.cpp không thay đổi ...
// (Phần mã nguồn còn lại được giữ nguyên)
void VideoWidget::setupConnections()
{
    connect(m_configWidget, &ConfigWidget::filePathSelected, this, &VideoWidget::onFileSelected);
    connect(m_configWidget, &ConfigWidget::reportPathSelected, this, &VideoWidget::onReportSelected);
    connect(m_analyzeButton, &QPushButton::clicked, this, &VideoWidget::onAnalyzeClicked);
    connect(m_stopButton, &QPushButton::clicked, this, &VideoWidget::onStopClicked);
    connect(m_resultsWidget, &ResultsWidget::settingsClicked, this, &VideoWidget::onSettingsClicked);
    connect(m_resultsWidget, &ResultsWidget::exportTxtClicked, this, &VideoWidget::onExportTxt);
    connect(m_resultsWidget, &ResultsWidget::copyToClipboardClicked, this, &VideoWidget::onCopyToClipboard);
    connect(m_logButton, &QPushButton::clicked, this, &VideoWidget::onShowLogClicked);
    
    connect(m_resultsWidget, &ResultsWidget::errorDoubleClicked, this, &VideoWidget::onResultDoubleClicked);
    connect(m_qctoolsController, &QCToolsController::controllerError, this, &VideoWidget::onControllerError);

    connect(m_qctoolsManager, &QCToolsManager::analysisStarted, this, [this](){ setAnalysisInProgress(true); });
    connect(m_qctoolsManager, &QCToolsManager::statusUpdated, this, &VideoWidget::updateStatus, Qt::QueuedConnection);
    connect(m_qctoolsManager, &QCToolsManager::progressUpdated, this, &VideoWidget::updateProgress, Qt::QueuedConnection);
    connect(m_qctoolsManager, &QCToolsManager::resultsReady, this, &VideoWidget::handleResults, Qt::QueuedConnection);
    connect(m_qctoolsManager, &QCToolsManager::analysisFinished, this, &VideoWidget::handleAnalysisFinished, Qt::QueuedConnection);
    connect(m_qctoolsManager, &QCToolsManager::errorOccurred, this, &VideoWidget::handleError, Qt::QueuedConnection);
    connect(m_qctoolsManager, &QCToolsManager::logMessage, this, &VideoWidget::handleLogMessage, Qt::QueuedConnection);
    connect(m_qctoolsManager, &QCToolsManager::backgroundTaskFinished, this, &VideoWidget::handleBackgroundTaskFinished, Qt::QueuedConnection);
    connect(m_qctoolsManager, &QCToolsManager::mediaInfoReady, this, &VideoWidget::handleMediaInfo, Qt::QueuedConnection);
}

void VideoWidget::initializePaths()
{
    QSettings settings(AppConstants::ORG_NAME, AppConstants::APP_NAME);
    QString qctoolsPath = settings.value(AppConstants::K_QCTOOLS_PATH).toString();
    QString qccliPath = settings.value(AppConstants::K_QCCLI_PATH).toString();

    bool arePathsValid = !qctoolsPath.isEmpty() && !qccliPath.isEmpty() &&
                         QFile::exists(qctoolsPath) && QFile::exists(qccliPath);

    if (arePathsValid) {
        handleLogMessage(QString("[INFO] Đã tải đường dẫn hợp lệ từ cài đặt."));
        handleLogMessage(QString("   - QCTools.exe: %1").arg(qctoolsPath));
        handleLogMessage(QString("   - qcli.exe: %1").arg(qccliPath));
        m_qctoolsController->updatePaths(qctoolsPath, qccliPath);
    } else {
        handleLogMessage("[WARNING] Đường dẫn trong cài đặt không hợp lệ hoặc chưa có. Bắt đầu tìm kiếm tự động...");
        const QString defaultQCToolsPath = "C:/Program Files/QCTools/QCTools.exe";
        
        if (QFile::exists(defaultQCToolsPath)) {
            QFileInfo qctoolsInfo(defaultQCToolsPath);
            QString defaultQCliPath = qctoolsInfo.absolutePath() + "/qcli.exe";

            if (QFile::exists(defaultQCliPath)) {
                handleLogMessage(QString("[INFO] Đã tìm thấy QCTools tại vị trí mặc định. Tự động lưu cài đặt."));
                qctoolsPath = QDir::toNativeSeparators(defaultQCToolsPath);
                qccliPath = QDir::toNativeSeparators(defaultQCliPath);
                
                settings.setValue(AppConstants::K_QCTOOLS_PATH, qctoolsPath);
                settings.setValue(AppConstants::K_QCCLI_PATH, qccliPath);
                
                m_qctoolsController->updatePaths(qctoolsPath, qccliPath);
                
                handleLogMessage(QString("   - QCTools.exe: %1").arg(qctoolsPath));
                handleLogMessage(QString("   - qcli.exe: %1").arg(qccliPath));
            } else {
                handleLogMessage("[ERROR] Tìm thấy QCTools.exe nhưng không tìm thấy qcli.exe. Yêu cầu người dùng cấu hình thủ công.");
                promptForPaths();
            }
        } else {
            handleLogMessage("[INFO] Không tìm thấy QCTools tại vị trí mặc định. Yêu cầu người dùng cấu hình thủ công.");
            promptForPaths();
        }
    }
}


void VideoWidget::handleFileDrop(const QString &path)
{
    QFileInfo fileInfo(path);
    QString ext = fileInfo.suffix().toLower();
    QString fileName = fileInfo.fileName().toLower();
    
    if (ext == "xml" || ext == "gz" || ext == "mkv" || fileName.contains(".qctools.")) {
        onReportSelected(path);
    } else {
        onFileSelected(path);
    }
}


void VideoWidget::promptForPaths()
{
    QMessageBox::information(this, "Yêu cầu Cấu hình",
                             "Đường dẫn QCTools chưa được thiết lập hoặc không hợp lệ.\n\n"
                             "Vui lòng vào Cài đặt để chỉ định đường dẫn cho QCTools.exe.");
    onSettingsClicked();
}

void VideoWidget::onControllerError(const QString &message)
{
    QMessageBox::critical(this, "Lỗi Khởi Chạy QCTools", message);
    updateStatus("Lỗi! Không thể khởi chạy QCTools.");
    promptForPaths();
}

QString VideoWidget::findExistingReport(const QString &videoPath, QCToolsManager::ReportType type) const
{
    QFileInfo videoInfo(videoPath);
    QString dirPath = videoInfo.absolutePath();
    QString baseNameWithExt = videoInfo.fileName();
    
    QString reportPath;
    switch(type){
        case QCToolsManager::ReportType::XML:
            reportPath = dirPath + "/" + baseNameWithExt + ".qctools.xml";
            break;
        case QCToolsManager::ReportType::GZ:
            reportPath = dirPath + "/" + baseNameWithExt + ".qctools.xml.gz";
            break;
        case QCToolsManager::ReportType::MKV:
            reportPath = dirPath + "/" + baseNameWithExt + ".qctools.mkv";
            break;
    }

    if (QFile::exists(reportPath)) {
        return reportPath;
    }
    return QString();
}


void VideoWidget::deleteAssociatedReports(const QString &videoPath)
{
    handleLogMessage("[INFO] Xóa các file báo cáo cũ để phân tích lại...");
    QStringList reportsToDelete;
    reportsToDelete << findExistingReport(videoPath, QCToolsManager::ReportType::XML)
                    << findExistingReport(videoPath, QCToolsManager::ReportType::GZ)
                    << findExistingReport(videoPath, QCToolsManager::ReportType::MKV);
    
    for (const QString& reportPath : reportsToDelete) {
        if (!reportPath.isEmpty() && QFile::exists(reportPath)) {
            if (QFile::remove(reportPath)) {
                handleLogMessage(QString("   - Đã xóa: %1").arg(QFileInfo(reportPath).fileName()));
            } else {
                handleLogMessage(QString("[WARNING] Không thể xóa file: %1").arg(QFileInfo(reportPath).fileName()));
                QMessageBox::warning(this, "Lỗi xóa file", QString("Không thể xóa file báo cáo cũ:\n%1").arg(reportPath));
            }
        }
    }
}


void VideoWidget::onFileSelected(const QString &path)
{
    handleLogMessage(QString("[%1] Đã chọn file mới: %2").arg(QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss")).arg(path));
    m_currentVideoPath = path;
    m_currentReportPath.clear();
    m_resultsWidget->clearResults();
    m_currentMediaInfo = MediaInfo();
    emit videoFileChanged(QFileInfo(path).fileName());
    m_configWidget->setInputPath(path);
    m_analyzeButton->setText("BẮT ĐẦU PHÂN TÍCH");
    
    QString existingXml = findExistingReport(path, QCToolsManager::ReportType::XML);
    if(existingXml.isEmpty()) existingXml = findExistingReport(path, QCToolsManager::ReportType::GZ);
    
    QString existingMkv = findExistingReport(path, QCToolsManager::ReportType::MKV);

    if (!existingXml.isEmpty()) {
        handleLogMessage(QString("[INFO] Đã phát hiện file báo cáo có sẵn: %1").arg(existingXml));
        
        QMessageBox msgBox(this);
        msgBox.setWindowTitle("Phát hiện dữ liệu có sẵn");
        msgBox.setText(QString("Đã tìm thấy một file báo cáo có sẵn cho video này.\n'%1'").arg(QFileInfo(existingXml).fileName()));
        msgBox.setInformativeText("Bạn muốn làm gì?");
        QPushButton* reanalyzeButton = msgBox.addButton("Phân tích lại (Ghi đè)", QMessageBox::ActionRole);
        QPushButton* viewReportButton = msgBox.addButton("Xem báo cáo có sẵn", QMessageBox::ActionRole);
        QPushButton* cancelButton = msgBox.addButton("Hủy", QMessageBox::RejectRole);
        msgBox.setDefaultButton(viewReportButton);
        msgBox.setEscapeButton(cancelButton);
        msgBox.exec();

        if (msgBox.clickedButton() == viewReportButton) {
            onReportSelected(existingXml);
        } else if (msgBox.clickedButton() == reanalyzeButton) {
             m_currentMode = AnalysisMode::ANALYZE_VIDEO;
             onAnalyzeClicked();
        }
    } else if (!existingMkv.isEmpty()) {
        handleLogMessage(QString("[INFO] Đã phát hiện file báo cáo .mkv có sẵn: %1").arg(existingMkv));
        QMessageBox msgBox(QMessageBox::Warning, "Cảnh báo: Hạn chế của File .mkv",
            "Đã tìm thấy file báo cáo `.mkv` có sẵn. Do hạn chế của công cụ, việc xem báo cáo từ file này có thể dẫn đến việc phát hiện sai tất cả các frame là lỗi 'Viền Đen'.\n\n"
            "Để có kết quả chính xác nhất, bạn nên phân tích lại video.",
            QMessageBox::NoButton, this);

        QPushButton* reanalyzeButton = msgBox.addButton("Phân tích lại (Khuyến nghị)", QMessageBox::ActionRole);
        QPushButton* viewReportButton = msgBox.addButton("Vẫn xem báo cáo .mkv", QMessageBox::ActionRole);
        QPushButton* cancelButton = msgBox.addButton("Hủy", QMessageBox::RejectRole);
        msgBox.setDefaultButton(reanalyzeButton);
        msgBox.exec();

        if (msgBox.clickedButton() == viewReportButton) {
            onReportSelected(existingMkv);
        } else if (msgBox.clickedButton() == reanalyzeButton) {
            m_currentMode = AnalysisMode::ANALYZE_VIDEO;
            onAnalyzeClicked();
        }
    }
    else {
        updateStatus("Đã chọn file. Sẵn sàng phân tích.");
        m_currentMode = AnalysisMode::ANALYZE_VIDEO;
        m_analyzeButton->setEnabled(true);
    }
}

void VideoWidget::onReportSelected(const QString &path)
{
    handleLogMessage(QString("[%1] Đã nhập file báo cáo: %2").arg(QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss")).arg(path));
    m_currentReportPath = path;
    m_resultsWidget->clearResults();
    m_currentMediaInfo = MediaInfo();
    emit videoFileChanged(QFileInfo(path).fileName());
    m_configWidget->setInputPath(path);
    m_currentVideoPath.clear();
    m_analyzeButton->setText("XEM BÁO CÁO");

    QFileInfo reportInfo(path);
    QString reportName = reportInfo.fileName();
    QString baseName = reportName;

    if (reportName.endsWith(".qctools.xml.gz")) baseName = reportName.left(reportName.length() - 16);
    else if (reportName.endsWith(".qctools.mkv")) baseName = reportName.left(reportName.length() - 12);
    else if (reportName.endsWith(".qctools.xml")) baseName = reportName.left(reportName.length() - 12);
    else if (reportName.endsWith(".xml.gz")) baseName = reportName.left(reportName.length() - 7);
    else if (reportName.endsWith(".xml")) baseName = reportName.left(reportName.length() - 4);
    
    QString potentialVideoPath = reportInfo.absolutePath() + "/" + baseName;
    if(QFile::exists(potentialVideoPath)){
        m_currentVideoPath = potentialVideoPath;
        handleLogMessage(QString("[INFO] Đã tìm thấy file video tương ứng: %1").arg(m_currentVideoPath));
    }

    m_currentMode = AnalysisMode::VIEW_REPORT;
    m_analyzeButton->setText("XEM BÁO CÁO");
    
    if (reportName.toLower().endsWith(".mkv")) {
        if (!m_currentVideoPath.isEmpty()) {
             QMessageBox msgBox(QMessageBox::Warning, "Cảnh báo: Hạn chế của File .mkv",
                "Bạn đã chọn một file `.mkv`. Do hạn chế của công cụ, việc xem báo cáo từ file này có thể dẫn đến việc phát hiện sai tất cả các frame là lỗi 'Viền Đen'.\n\n"
                "Vì đã tìm thấy file video gốc, bạn nên phân tích lại để có kết quả chính xác nhất.",
                QMessageBox::NoButton, this);

            QPushButton* reanalyzeButton = msgBox.addButton("Phân tích lại (Khuyến nghị)", QMessageBox::ActionRole);
            QPushButton* viewReportButton = msgBox.addButton("Vẫn xem báo cáo .mkv", QMessageBox::ActionRole);
            msgBox.setDefaultButton(reanalyzeButton);
            msgBox.exec();

            if (msgBox.clickedButton() == reanalyzeButton) {
                m_currentMode = AnalysisMode::ANALYZE_VIDEO;
                m_analyzeButton->setText("BẮT ĐẦU PHÂN TÍCH");
                m_configWidget->setInputPath(m_currentVideoPath);
                emit videoFileChanged(QFileInfo(m_currentVideoPath).fileName());
                onAnalyzeClicked();
                return; 
            } else if (msgBox.clickedButton() == viewReportButton) {
                onAnalyzeClicked();
            }
        } else {
            QMessageBox::warning(this, "Cảnh báo: Hạn chế của File .mkv",
                "Bạn đã chọn một file `.mkv` và không tìm thấy file video gốc tương ứng.\n\n"
                "Do hạn chế của công cụ, kết quả phân tích 'Viền Đen' có thể không chính xác.");
            onAnalyzeClicked();
        }
    } else {
        onAnalyzeClicked();
    }
}


void VideoWidget::onAnalyzeClicked()
{
    if (m_isAnalysisInProgress) {
        QMessageBox::warning(this, "Đang xử lý", "Một quá trình khác đang chạy. Vui lòng đợi.");
        return;
    }
    
    m_resultsWidget->clearResults();
    m_currentMediaInfo = MediaInfo();
    
    QSettings qsettings(AppConstants::ORG_NAME, AppConstants::APP_NAME);
    QVariantMap settings = m_configWidget->getSettings();
    settings[AppConstants::K_QCCLI_PATH] = qsettings.value(AppConstants::K_QCCLI_PATH).toString();
    
    if (settings[AppConstants::K_QCCLI_PATH].toString().isEmpty() || !QFile::exists(settings[AppConstants::K_QCCLI_PATH].toString())) {
        QMessageBox::critical(this, "Lỗi đường dẫn", "Đường dẫn đến qcli.exe không hợp lệ. Vui lòng kiểm tra lại trong Cài đặt.");
        promptForPaths();
        return;
    }

    if (m_currentMode == AnalysisMode::ANALYZE_VIDEO) {
        deleteAssociatedReports(m_currentVideoPath);
        m_analyzeButton->setText("BẮT ĐẦU PHÂN TÍCH");

        if (m_currentVideoPath.isEmpty()) {
            QMessageBox::warning(this, "Chưa chọn file", "Vui lòng chọn một file video.");
            return;
        }
        QMetaObject::invokeMethod(m_qctoolsManager, "doWork", Qt::QueuedConnection,
                                  Q_ARG(QString, m_currentVideoPath),
                                  Q_ARG(QVariantMap, settings));
    } else if (m_currentMode == AnalysisMode::VIEW_REPORT) {
        m_analyzeButton->setText("XEM BÁO CÁO");
        if (m_currentReportPath.isEmpty()) {
            QMessageBox::warning(this, "Chưa chọn file", "Vui lòng chọn một file báo cáo.");
            return;
        }
        QMetaObject::invokeMethod(m_qctoolsManager, "processReportFile", Qt::QueuedConnection,
                                  Q_ARG(QString, m_currentReportPath),
                                  Q_ARG(QVariantMap, settings));
    }
}

void VideoWidget::onStopClicked()
{
    if (m_isAnalysisInProgress) {
        updateStatus("Đang yêu cầu dừng...");
        m_stopButton->setEnabled(false);
        QMetaObject::invokeMethod(m_qctoolsManager, "requestStop", Qt::DirectConnection);
    }
}

void VideoWidget::setAnalysisInProgress(bool inProgress)
{
    m_isAnalysisInProgress = inProgress;
    m_configWidget->setEnabled(!inProgress);
    m_analysisButtonStack->setCurrentWidget(inProgress ? m_stopButton : m_analyzeButton);
    m_stopButton->setEnabled(inProgress);

    if (inProgress) {
        updateStatus("Bắt đầu xử lý...");
        m_analyzeButton->setEnabled(false);
    } else {
         m_analyzeButton->setEnabled(!m_currentVideoPath.isEmpty() || !m_currentReportPath.isEmpty());
    }
}

void VideoWidget::onSettingsClicked()
{
    if(!m_settingsDialog){
        m_settingsDialog = new SettingsDialog(this);
        connect(m_settingsDialog, &SettingsDialog::settingsReset, this, &VideoWidget::onSettingsReset);
    }
    
    if(m_settingsDialog->exec() == QDialog::Accepted){
        QSettings settings(AppConstants::ORG_NAME, AppConstants::APP_NAME);
        QString qctoolsPath = settings.value(AppConstants::K_QCTOOLS_PATH).toString();
        QString qccliPath = settings.value(AppConstants::K_QCCLI_PATH).toString();
        m_qctoolsController->updatePaths(qctoolsPath, qccliPath);
        m_configWidget->reloadSettings();
        handleLogMessage("[INFO] Cài đặt đã được cập nhật.");
    }
}

void VideoWidget::onSettingsReset()
{
    m_configWidget->reloadSettings();
    handleLogMessage("[INFO] Cài đặt đã được người dùng reset.");
    initializePaths();
}


void VideoWidget::onShowLogClicked()
{
    if (!m_logDialog) {
        m_logDialog = new LogDialog(m_logHistory, this);
    }
    m_logDialog->setDefaultSavePath(getCurrentDefaultSaveDir());
    
    QString sourceFile = !m_currentVideoPath.isEmpty() ? m_currentVideoPath : m_currentReportPath;
    m_logDialog->setVideoFileName(QFileInfo(sourceFile).fileName());

    m_logDialog->show();
    m_logDialog->raise();
    m_logDialog->activateWindow();
}

void VideoWidget::onResultDoubleClicked(int frameNum)
{
    if (m_currentFps > 0) {
        QSettings settings(AppConstants::ORG_NAME, AppConstants::APP_NAME);
        int rewindFrames = settings.value(AppConstants::K_REWIND_FRAMES, 5).toInt();
        
        int newFrame = frameNum - rewindFrames;
        if (newFrame < 0) newFrame = 0;

        QString timecode = QCToolsManager::frameToTimecodePrecise(newFrame, m_currentFps);
        QApplication::clipboard()->setText(timecode);

        if(m_statusResetTimer->isActive()) m_statusResetTimer->stop();
        m_statusLabel->setText(QString("Đã sao chép timecode '%1' (đã lùi %2 frames) vào clipboard!").arg(timecode).arg(rewindFrames));
        m_statusResetTimer->start(3500);
    }
}

void VideoWidget::onExportTxt()
{
    const auto& results = m_resultsWidget->getCurrentResults();
    if (results.isEmpty()){ 
        QMessageBox::warning(this, "Không có dữ liệu", "Không có dữ liệu để xuất."); 
        return; 
    }
    
    QString defaultDir = getCurrentDefaultSaveDir();
    QString sourceFile = !m_currentVideoPath.isEmpty() ? m_currentVideoPath : m_currentReportPath;
    QFileInfo fileInfo(sourceFile);
    QString suggestedName = defaultDir + "/" + fileInfo.completeBaseName() + "_QC_Report.txt";
    
    QString p = QFileDialog::getSaveFileName(this, "Lưu file Text", suggestedName, "Text Files (*.txt)");

    if (p.isEmpty()) return;
    QFile f(p);
    if (f.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream out(&f);
        out.setEncoding(QStringConverter::Utf8);
        out << "File: " << QDir::toNativeSeparators(sourceFile) << "\n\n";
        out << QString("%1\t%2\t%3\t%4\n").arg("Timecode", -15).arg("Thời lượng (fr)", -15).arg("Loại lỗi", -20).arg("Chi tiết");
        out << QString(80, '-') << "\n";
        for (const auto &res : results) {
            out << QString("%1\t%2\t%3\t%4\n").arg(res.timecode, -15).arg(res.duration, -15).arg(res.errorType, -20).arg(res.details);
        }

        if (m_currentMediaInfo.width > 0) {
            out << "\n\n==================== THÔNG TIN FILE ====================\n";
            out << " File: " << fileInfo.fileName() << "\n";
            out << m_currentMediaInfo.toFormattedString();
            out << "\n====================================================\n";
        }

        f.close();
        QMessageBox::information(this, "Thành công", "Đã xuất file TXT thành công.");
    } else {
        QMessageBox::critical(this, "Lỗi", "Không thể lưu file TXT.");
    }
}

void VideoWidget::onCopyToClipboard()
{
    const auto& results = m_resultsWidget->getCurrentResults();
    if (results.isEmpty()){ 
        QMessageBox::warning(this, "Không có dữ liệu", "Không có dữ liệu để sao chép."); 
        return; 
    }
    QString s;
    QTextStream ss(&s);
    
    QString sourceFile = !m_currentVideoPath.isEmpty() ? m_currentVideoPath : m_currentReportPath;
    ss << "File: " << QDir::toNativeSeparators(sourceFile) << "\n\n";
    ss << "Timecode\tThời lượng (fr)\tLoại lỗi\tChi tiết\n";
    for (const auto &res : results) {
        ss << res.timecode << "\t" << res.duration << "\t" << res.errorType << "\t" << res.details << "\n";
    }

    if (m_currentMediaInfo.width > 0) {
        ss << "\n\n==================== THÔNG TIN FILE ====================\n";
        ss << " File: " << QFileInfo(sourceFile).fileName() << "\n";
        ss << m_currentMediaInfo.toFormattedString();
        ss << "\n====================================================\n";
    }

    QApplication::clipboard()->setText(s);
    QMessageBox::information(this, "Thành công", "Đã sao chép kết quả vào clipboard.");
}


void VideoWidget::updateStatus(const QString &status){ 
    m_persistentStatusText = status;
    m_statusLabel->setText(m_persistentStatusText);
}

void VideoWidget::updateProgress(int value, int max){ 
    if(m_isAnalysisInProgress) {
        int percentage = (max > 0) ? static_cast<int>((static_cast<double>(value) / max) * 100.0) : 0;
        m_statusLabel->setText(QString("%1 [%2%]").arg(m_persistentStatusText).arg(percentage));
    }
}

void VideoWidget::handleResults(const QList<AnalysisResult> &results){
    if (!m_isAnalysisInProgress && m_currentMode == AnalysisMode::IDLE) return;
    m_resultsWidget->handleResults(results);
}

void VideoWidget::handleAnalysisFinished(bool success) {
    setAnalysisInProgress(false);
    
    if (!success) {
        if(m_stopButton->isEnabled())
             updateStatus("Đã xảy ra lỗi trong quá trình xử lý.");
        else
             updateStatus("Tác vụ đã được người dùng dừng.");
        return;
    }
    
    updateStatus("Xử lý hoàn tất!");
    
    QString resultMessage;
    int errorCount = m_resultsWidget->getCurrentResults().count();
    if (errorCount == 0) {
        resultMessage = "Quá trình xử lý đã hoàn tất.\nKhông tìm thấy lỗi nào với cấu hình hiện tại.";
    } else {
        resultMessage = QString("Quá trình xử lý đã hoàn tất.\nTìm thấy %1 lỗi.").arg(errorCount);
    }

    QMessageBox msgBox(this);
    msgBox.setWindowTitle("Hoàn tất");
    msgBox.setIcon(QMessageBox::Information);
    msgBox.setText(resultMessage);
    msgBox.setInformativeText("Bạn có muốn mở kết quả trong QCTools để xem chi tiết không?");
    
    QPushButton *yesButton = msgBox.addButton("Có", QMessageBox::YesRole);
    QPushButton *noButton = msgBox.addButton("Không", QMessageBox::NoRole);
    msgBox.setDefaultButton(noButton);

    msgBox.exec();

    if (msgBox.clickedButton() == yesButton) {
        QString fileToOpen;
        if (!m_currentVideoPath.isEmpty()) {
            fileToOpen = m_currentVideoPath;
        } else if (!m_currentReportPath.isEmpty()) {
            fileToOpen = m_currentReportPath;
        }

        if (!fileToOpen.isEmpty()) {
            handleLogMessage(QString("[INFO] Người dùng yêu cầu mở '%1' trong QCTools.").arg(QFileInfo(fileToOpen).fileName()));
            m_qctoolsController->startAndOpenFile(fileToOpen);
        } else {
            handleLogMessage("[WARNING] Không thể mở QCTools vì không có đường dẫn video hay báo cáo.");
            QMessageBox::warning(this, "Không thể mở", "Không tìm thấy file video hay file báo cáo để mở.");
        }
    }
}

void VideoWidget::handleError(const QString &error) {
    setAnalysisInProgress(false);
    QMessageBox::critical(this, "Lỗi Xử Lý", error);
    updateStatus("Đã xảy ra lỗi nghiêm trọng!");
}

void VideoWidget::handleLogMessage(const QString &message)
{
    m_logHistory.append(message);
    if (m_logDialog) {
        m_logDialog->appendLog(message);
    }
}

void VideoWidget::handleBackgroundTaskFinished(const QString &message)
{
    updateStatus(message);
    handleLogMessage(QString("[INFO] %1").arg(message));
}

void VideoWidget::handleMediaInfo(const MediaInfo &info)
{
    m_currentFps = info.fps;
    m_resultsWidget->setCurrentFps(info.fps);
    m_currentMediaInfo = info;
    QString sourceFile = !m_currentVideoPath.isEmpty() ? m_currentVideoPath : m_currentReportPath;
    handleLogMessage("\n==================== THÔNG TIN FILE ====================");
    handleLogMessage(QString(" File: %1").arg(QFileInfo(sourceFile).fileName()));
    handleLogMessage(info.toFormattedString());
    handleLogMessage("====================================================\n");
}


QString VideoWidget::getCurrentDefaultSaveDir() const
{
    if (!m_currentVideoPath.isEmpty()) {
        return QFileInfo(m_currentVideoPath).absolutePath();
    } else if (!m_currentReportPath.isEmpty()) {
        return QFileInfo(m_currentReportPath).absolutePath();
    }
    return QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
}

void VideoWidget::onStatusResetTimeout()
{
    m_statusLabel->setText(m_persistentStatusText);
}
