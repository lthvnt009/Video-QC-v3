// src/ui/videowidget.cpp
#include "videowidget.h"
#include "configwidget.h"
#include "resultswidget.h"
#include "settingsdialog.h"
#include "logdialog.h"
#include "qctools/QCToolsManager.h"
#include "qctools/QCToolsController.h"
#include "core/Constants.h" 

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QStackedWidget>
#include <QProgressBar>
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

    setupUI();

    m_analysisThread = new QThread(this);
    m_qctoolsManager = new QCToolsManager();
    m_qctoolsManager->moveToThread(m_analysisThread);
    m_qctoolsController = new QCToolsController(this);

    setupConnections();
    m_analysisThread->start();
    
    handleLogMessage(QString("[%1] Chương trình đã khởi động.").arg(QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss")));
    
    QSettings settings(AppConstants::ORG_NAME, AppConstants::APP_NAME);
    QString qctoolsPath = settings.value(AppConstants::K_QCTOOLS_PATH).toString();
    QString qccliPath = settings.value(AppConstants::K_QCCLI_PATH).toString();

    if (qctoolsPath.isEmpty() || qccliPath.isEmpty() || !QFile::exists(qctoolsPath) || !QFile::exists(qccliPath)) {
        handleLogMessage("[WARNING] Đường dẫn QCTools hoặc qcli chưa được thiết lập. Yêu cầu người dùng cấu hình.");
        promptForPaths();
    } else {
        handleLogMessage(QString("[INFO] Đã tải đường dẫn QCTools.exe: %1").arg(qctoolsPath));
        handleLogMessage(QString("[INFO] Đã tải đường dẫn qcli.exe: %1").arg(qccliPath));
    }
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
    
    QWidget *controlPane = new QWidget;
    QVBoxLayout *controlLayout = new QVBoxLayout(controlPane);
    controlLayout->setSpacing(10);
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
    
    m_logButton = new QPushButton("Xem Log");

    controlButtonsLayout->addWidget(m_analysisButtonStack, 1);
    controlButtonsLayout->addWidget(m_logButton);


    // --- Bottom Status Bar Layout ---
    QHBoxLayout* bottomBarLayout = new QHBoxLayout();
    m_statusLabel = new QLabel("Sẵn sàng. Vui lòng chọn file video hoặc file báo cáo.");
    m_progressBar = new QProgressBar;
    m_progressBar->setTextVisible(false);
    m_progressBar->setFixedHeight(15);
    
    bottomBarLayout->addWidget(m_statusLabel, 1);
    bottomBarLayout->addWidget(m_progressBar, 2);

    controlLayout->addWidget(m_configWidget);
    controlLayout->addLayout(controlButtonsLayout);
    controlLayout->addLayout(bottomBarLayout);
    controlLayout->addWidget(m_resultsWidget, 1); 
    
    mainLayout->addWidget(controlPane);
}


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
    connect(m_qctoolsManager, &QCToolsManager::videoInfoReady, this, [this](double fps){ m_currentFps = fps; });
}

void VideoWidget::handleFileDrop(const QString &path)
{
    QFileInfo fileInfo(path);
    QString fileName = fileInfo.fileName().toLower();
    
    if (fileName.endsWith(".xml") || fileName.endsWith(".xml.gz") || fileName.contains(".qctools.")) {
        onReportSelected(path);
    } else {
        onFileSelected(path);
    }
}

void VideoWidget::promptForPaths()
{
    QMessageBox::information(this, "Yêu cầu Cấu hình",
                             "Đây là lần đầu bạn sử dụng chương trình hoặc đường dẫn chưa được thiết lập.\n\n"
                             "Vui lòng vào Cài đặt Nâng cao để chỉ định đường dẫn cho QCTools.exe.");
    onSettingsClicked();
}

void VideoWidget::onControllerError(const QString &message)
{
    QMessageBox::critical(this, "Lỗi Khởi Chạy QCTools", message);
    m_statusLabel->setText("Lỗi! Không thể khởi chạy QCTools.");
    promptForPaths();
}

QString VideoWidget::findExistingReport(const QString &videoPath) const
{
    QFileInfo videoInfo(videoPath);
    QString dirPath = videoInfo.absolutePath();
    QString baseNameWithExt = videoInfo.fileName(); 
    QString baseName = videoInfo.completeBaseName();

    QStringList extensionsToTry = {
        ".qctools.xml",
        ".qctools.xml.gz",
        ".qctools.mkv",
        ".xml",
        ".xml.gz"
    };

    for(const QString& ext : extensionsToTry) {
        QString reportPath = (ext.startsWith(".qctools")) 
                           ? dirPath + "/" + baseNameWithExt + ext
                           : dirPath + "/" + baseName + ext;

        if(QFile::exists(reportPath)) {
            return reportPath;
        }
    }
    return QString();
}


void VideoWidget::onFileSelected(const QString &path)
{
    handleLogMessage(QString("[%1] Đã chọn file mới: %2").arg(QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss")).arg(path));
    m_currentVideoPath = path;
    m_currentReportPath.clear();
    m_resultsWidget->clearResults();
    m_progressBar->setValue(0);
    emit videoFileChanged(QFileInfo(path).fileName());
    m_configWidget->setInputPath(path);
    
    QString existingReport = findExistingReport(path);
    
    if (!existingReport.isEmpty()) {
        handleLogMessage(QString("[INFO] Đã phát hiện file báo cáo có sẵn: %1").arg(existingReport));
        
        QMessageBox msgBox(this);
        msgBox.setWindowTitle("Phát hiện dữ liệu thô");
        msgBox.setText(QString("Đã tìm thấy một file báo cáo có sẵn cho video này.\n'%1'").arg(QFileInfo(existingReport).fileName()));
        msgBox.setInformativeText("Bạn muốn làm gì?");
        QPushButton* reanalyzeButton = msgBox.addButton("Phân tích lại (Ghi đè)", QMessageBox::ActionRole);
        QPushButton* viewReportButton = msgBox.addButton("Xem báo cáo có sẵn", QMessageBox::ActionRole);
        msgBox.setDefaultButton(viewReportButton);
        msgBox.exec();

        if (msgBox.clickedButton() == viewReportButton) {
            m_currentMode = AnalysisMode::VIEW_REPORT;
            m_currentReportPath = existingReport;
            onAnalyzeClicked(); // Kích hoạt ngay lập tức
        } else {
             m_statusLabel->setText("Đã phát hiện dữ liệu thô. Sẵn sàng phân tích lại.");
             m_currentMode = AnalysisMode::ANALYZE_VIDEO;
             onAnalyzeClicked(); // Kích hoạt ngay lập tức
        }
    } else {
        m_statusLabel->setText("Đã chọn file. Sẵn sàng phân tích.");
        m_currentMode = AnalysisMode::ANALYZE_VIDEO;
        m_analyzeButton->setEnabled(true);
    }
}

void VideoWidget::onReportSelected(const QString &path)
{
    handleLogMessage(QString("[%1] Đã nhập file báo cáo: %2").arg(QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss")).arg(path));
    m_currentReportPath = path;
    m_currentVideoPath.clear();
    m_resultsWidget->clearResults();
    m_progressBar->setValue(0);
    emit videoFileChanged(QFileInfo(path).fileName());
    m_configWidget->setInputPath(path);

    m_statusLabel->setText("Sẵn sàng xem báo cáo đã chọn.");
    m_currentMode = AnalysisMode::VIEW_REPORT;
    m_analyzeButton->setText("BẮT ĐẦU PHÂN TÍCH"); // Giữ nguyên văn bản nút
    m_analyzeButton->setEnabled(true);
}


void VideoWidget::onAnalyzeClicked()
{
    if (m_isAnalysisInProgress) {
        QMessageBox::warning(this, "Đang xử lý", "Một quá trình khác đang chạy. Vui lòng đợi.");
        return;
    }

    m_resultsWidget->clearResults();
    
    QSettings qsettings(AppConstants::ORG_NAME, AppConstants::APP_NAME);
    QVariantMap settings = m_configWidget->getSettings();
    settings[AppConstants::K_QCCLI_PATH] = qsettings.value(AppConstants::K_QCCLI_PATH).toString();
    
    if (settings[AppConstants::K_QCCLI_PATH].toString().isEmpty() || !QFile::exists(settings[AppConstants::K_QCCLI_PATH].toString())) {
        QMessageBox::critical(this, "Lỗi đường dẫn", "Đường dẫn đến qcli.exe không hợp lệ. Vui lòng kiểm tra lại trong Cài đặt Nâng cao.");
        promptForPaths();
        return;
    }

    if (m_currentMode == AnalysisMode::ANALYZE_VIDEO) {
        if (m_currentVideoPath.isEmpty()) {
            QMessageBox::warning(this, "Chưa chọn file", "Vui lòng chọn một file video.");
            return;
        }
        QMetaObject::invokeMethod(m_qctoolsManager, "doWork", Qt::QueuedConnection,
                                  Q_ARG(QString, m_currentVideoPath),
                                  Q_ARG(QVariantMap, settings));
    } else if (m_currentMode == AnalysisMode::VIEW_REPORT) {
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
        m_statusLabel->setText("Đang yêu cầu dừng...");
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
        m_statusLabel->setText("Bắt đầu xử lý...");
        m_progressBar->setRange(0, 0); 
        m_progressBar->setValue(0);
        m_analyzeButton->setEnabled(false); // Vô hiệu hóa nút chính khi đang chạy
    } else {
         m_progressBar->setRange(0, 100);
         m_progressBar->setValue(100);
         m_analyzeButton->setEnabled(!m_currentVideoPath.isEmpty() || !m_currentReportPath.isEmpty());
    }
}

void VideoWidget::onSettingsClicked()
{
    if(!m_settingsDialog){
        m_settingsDialog = new SettingsDialog(this);
    }
    
    if(m_settingsDialog->exec() == QDialog::Accepted){
        QSettings settings(AppConstants::ORG_NAME, AppConstants::APP_NAME);
        QString qctoolsPath = settings.value(AppConstants::K_QCTOOLS_PATH).toString();
        QString qccliPath = settings.value(AppConstants::K_QCCLI_PATH).toString();
        m_qctoolsController->updatePaths(qctoolsPath, qccliPath);
        handleLogMessage("[INFO] Cài đặt đường dẫn đã được cập nhật.");
    }
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

        m_originalStatusText = m_statusLabel->text();
        m_statusLabel->setText(QString("Đã sao chép timecode '%1' (đã lùi %2 frames) vào clipboard!").arg(timecode).arg(rewindFrames));
        QTimer::singleShot(3000, this, [this](){
            if(m_statusLabel->text().contains("clipboard")) { // Avoid overwriting new status
                m_statusLabel->setText(m_originalStatusText);
            }
        });
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
    QString suggestedName = defaultDir + "/" + fileInfo.fileName() + "_QC_Report.txt";
    
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
    ss << "File: " << QDir::toNativeSeparators(!m_currentVideoPath.isEmpty() ? m_currentVideoPath : m_currentReportPath) << "\n\n";
    ss << "Timecode\tThời lượng (fr)\tLoại lỗi\tChi tiết\n";
    for (const auto &res : results) {
        ss << res.timecode << "\t" << res.duration << "\t" << res.errorType << "\t" << res.details << "\n";
    }
    QApplication::clipboard()->setText(s);
    QMessageBox::information(this, "Thành công", "Đã sao chép kết quả vào clipboard.");
}


void VideoWidget::updateStatus(const QString &status){ m_statusLabel->setText(status); }

void VideoWidget::updateProgress(int value, int max){ 
    if (max > 0) {
        if (m_progressBar->maximum() != max) m_progressBar->setRange(0, max);
        m_progressBar->setValue(value);
    }
}

void VideoWidget::handleResults(const QList<AnalysisResult> &results){
    if (!m_isAnalysisInProgress && m_currentMode == AnalysisMode::IDLE) return;
    m_resultsWidget->handleResults(results);
}

void VideoWidget::handleAnalysisFinished(bool success) {
    setAnalysisInProgress(false);
    
    if (!success) {
        if(!m_stopButton->isEnabled()) 
            m_statusLabel->setText("Tác vụ đã được người dùng dừng.");
        m_progressBar->setValue(0);
        return;
    }
    
    m_statusLabel->setText("Xử lý hoàn tất!");
    QString resultMessage;
    if (m_resultsWidget->getCurrentResults().isEmpty()) {
        resultMessage = "Quá trình xử lý đã hoàn tất.\nKhông tìm thấy lỗi nào với cấu hình hiện tại.";
    } else {
        resultMessage = QString("Quá trình xử lý đã hoàn tất.\nTìm thấy %1 lỗi.").arg(m_resultsWidget->getCurrentResults().count());
    }

    QMessageBox::information(this, "Hoàn tất", resultMessage);
}

void VideoWidget::handleError(const QString &error) {
    setAnalysisInProgress(false);
    QMessageBox::critical(this, "Lỗi Xử Lý", error);
    m_statusLabel->setText("Đã xảy ra lỗi nghiêm trọng!");
    m_progressBar->setValue(0);
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
    m_statusLabel->setText(message);
    handleLogMessage(QString("[INFO] %1").arg(message));
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


