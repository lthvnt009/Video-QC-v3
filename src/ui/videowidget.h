// src/ui/videowidget.h
#ifndef VIDEOWIDGET_H
#define VIDEOWIDGET_H

#include <QWidget>
#include <QThread>
#include <QStringList>
#include "core/types.h"

class QProgressBar;
class QLabel;
class QStackedWidget;
class QPushButton;
class ConfigWidget;
class ResultsWidget;
class SettingsDialog;
class QCToolsManager;
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

    void updateStatus(const QString &status);
    void updateProgress(int value, int max);
    void handleResults(const QList<AnalysisResult> &results);
    void handleAnalysisFinished(bool success);
    void handleError(const QString &error);
    void handleLogMessage(const QString& message);
    void handleBackgroundTaskFinished(const QString& message);


private:
    enum class AnalysisMode { IDLE, ANALYZE_VIDEO, VIEW_REPORT };

    void setupUI();
    void setupConnections();
    void setAnalysisInProgress(bool inProgress);
    void promptForPaths(); // SỬA LỖI: Cập nhật tên hàm cho nhất quán
    QString getCacheKey(const QString& filePath);
    QString getCachedReportPath(const QString& cacheKey);
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
    QProgressBar *m_progressBar;
    QLabel *m_statusLabel;
    
    // Backend
    QThread *m_analysisThread;
    QCToolsManager *m_qctoolsManager;
    QCToolsController *m_qctoolsController;

    // State
    QString m_currentVideoPath;
    QString m_currentReportPath;
    QString m_currentCacheKey;
    AnalysisMode m_currentMode = AnalysisMode::IDLE;
    QString m_cacheDir;
    bool m_isAnalysisInProgress = false;
    QStringList m_logHistory;
    double m_currentFps = 0.0;
    QString m_originalStatusText;
};

#endif // VIDEOWIDGET_H
