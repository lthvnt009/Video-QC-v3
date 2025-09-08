// src/ui/videowidget.h (Cải tiến Bước 2)
#ifndef VIDEOWIDGET_H
#define VIDEOWIDGET_H

#include <QWidget>
#include <QThread>
#include <QStringList>
#include "core/types.h"
#include "qctools/QCToolsManager.h"
#include "core/media_info.h"

class QLabel;
class QStackedWidget;
class QPushButton;
class QTimer;
class ConfigWidget;
class ResultsWidget;
class SettingsDialog;
class QCToolsController;
class LogDialog;

class VideoWidget : public QWidget
{
    Q_OBJECT

public:
    explicit VideoWidget(QWidget *parent = nullptr);
    ~VideoWidget();

signals:
    void videoFileChanged(const QString& videoName);

public slots:
    void handleFileDrop(const QString& path);
    void onSettingsReset();

private slots:
    void onFileSelected(const QString &path);
    void onReportSelected(const QString &path);
    void onAnalyzeClicked();
    void onStopClicked();
    void onExportTxt();
    void onCopyToClipboard();
    void onSettingsClicked();
    void onShowLogClicked();
    void onControllerError(const QString& message);
    void onResultDoubleClicked(int frameNum);
    void onStatusResetTimeout();

    // Slots for communication with manager thread
    void updateStatus(const QString &status);
    void updateProgress(int value, int max);
    void handleResults(const QList<AnalysisResult> &results);
    void handleAnalysisFinished(bool success);
    void handleError(const QString &error);
    void handleLogMessage(const QString& message);
    void handleBackgroundTaskFinished(const QString& message);
    void handleMediaInfo(const MediaInfo& info);


private:
    enum class AnalysisMode { IDLE, ANALYZE_VIDEO, VIEW_REPORT };

    void setupUI();
    void setupConnections();
    void initializePaths(); // CẢI TIẾN: Hàm mới để xử lý logic đường dẫn
    void setAnalysisInProgress(bool inProgress);
    void promptForPaths();
    
    QString findExistingReport(const QString& videoPath, QCToolsManager::ReportType type) const;
    void deleteAssociatedReports(const QString& videoPath);
    QString getCurrentDefaultSaveDir() const;

    // UI Elements
    ConfigWidget *m_configWidget;
    ResultsWidget *m_resultsWidget;
    SettingsDialog *m_settingsDialog = nullptr;
    LogDialog* m_logDialog = nullptr;
    QPushButton* m_logButton = nullptr;
    QStackedWidget *m_analysisButtonStack;
    QPushButton *m_analyzeButton;
    QPushButton *m_stopButton;
    QLabel *m_statusLabel;
    QTimer* m_statusResetTimer = nullptr;
    
    // Backend
    QThread *m_analysisThread;
    QCToolsManager *m_qctoolsManager;
    QCToolsController *m_qctoolsController;

    // State
    QString m_currentVideoPath;
    QString m_currentReportPath;
    AnalysisMode m_currentMode = AnalysisMode::IDLE;
    bool m_isAnalysisInProgress = false;
    QStringList m_logHistory;
    double m_currentFps = 0.0;
    QString m_persistentStatusText;
    
    MediaInfo m_currentMediaInfo;
};

#endif // VIDEOWIDGET_H

