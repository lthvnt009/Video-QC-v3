// src/qctools/QCToolsManager.h
#ifndef QCTOOLSMANAGER_H
#define QCTOOLSMANAGER_H

#include <QObject>
#include <QVariantMap>
#include <atomic>
#include "core/types.h"
#include <QProcess>
#include <memory> // CẢI TIẾN: Thêm thư viện con trỏ thông minh

class QTemporaryDir;
class QXmlStreamReader; // Forward declaration
struct FrameData; // Forward declaration

class QCToolsManager : public QObject
{
    Q_OBJECT

public:
    explicit QCToolsManager(QObject *parent = nullptr);
    ~QCToolsManager();
    
    enum class ReportType { GZ, MKV, XML };
    QString getReportPath(ReportType type) const;

    static QString frameToTimecodePrecise(int frame, double fps);

signals:
    void analysisStarted();
    void progressUpdated(int value, int max);
    void statusUpdated(const QString &status);
    void resultsReady(const QList<AnalysisResult> &results);
    void analysisFinished(bool success);
    void errorOccurred(const QString &error);
    void logMessage(const QString& message);
    void backgroundTaskFinished(const QString& message);
    void videoInfoReady(double fps); 

public slots:
    void doWork(const QString &filePath, const QVariantMap &settings);
    void processReportFile(const QString &reportPath, const QVariantMap &settings);
    void requestStop();

private slots:
    void onAnalysisStage1Finished(int exitCode, QProcess::ExitStatus exitStatus);
    void onExtractionFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void onMkvGenerationFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void readAnalysisOutput();

private:
    void startMkvGeneration();
    void extractFromMkv(const QString& mkvPath);
    void cleanup();
    QString createReportDirectory();
    
    void parseReport(const QString &reportPath);
    bool parseVideoMetadata(QXmlStreamReader& xml);
    QList<FrameData> extractAllFrameData(QXmlStreamReader& xml);
    QList<AnalysisResult> runErrorDetection(const QList<FrameData>& allFramesData);
    QMap<int, QSet<QString>> tagFramesForErrors(const QList<FrameData>& allFramesData);
    QList<AnalysisResult> groupErrorsFromTags(const QMap<int, QSet<QString>>& frameTags, const QList<FrameData>& allFramesData);
    void groupBlackFrames(QList<AnalysisResult>& results, const QMap<int, QSet<QString>>& tags, const QList<FrameData>& frames);
    void groupBorderedFrames(QList<AnalysisResult>& results, const QMap<int, QSet<QString>>& tags, const QList<FrameData>& frames);
    void findOrphanFrames(QList<AnalysisResult>& results, const QMap<int, QSet<QString>>& tags, const QList<FrameData>& frames);


    QProcess *m_mainProcess = nullptr;
    QProcess *m_backgroundProcess = nullptr;

    // CẢI TIẾN: Sử dụng con trỏ thông minh để quản lý bộ nhớ tự động và an toàn
    std::unique_ptr<QTemporaryDir> m_tempDir;
    QString m_reportDir;

    QString m_filePath;
    QString m_sourceReportPath; 
    QVariantMap m_settings;
    QString m_qcliPath;

    double m_fps = 0;
    int m_videoWidth = 0;
    int m_videoHeight = 0;
    int m_totalFrames = 0;
    int m_totalFramesFromLog = 0;

    std::atomic<bool> m_stopRequested{false};
    QString m_processBuffer;
    bool m_isGeneratingReport = false;
    QString m_currentPhase; 
    int m_currentStep = 0;
    
    // CẢI TIẾN: Đưa hằng số vào header để quản lý tập trung
    const int m_totalStepsAnalyze = 6;
    const int m_totalStepsViewReport = 5;
    int m_totalSteps = 0;
};

#endif // QCTOOLSMANAGER_H

