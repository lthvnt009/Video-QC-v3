// src/qctools/QCToolsManager.h (Cải tiến Bước 1)
#ifndef QCTOOLSMANAGER_H
#define QCTOOLSMANAGER_H

#include <QObject>
#include <QVariantMap>
#include <atomic>
#include "core/types.h"
#include "core/media_info.h"
#include <QProcess>
#include <memory>
#include <QTime>
#include <QFile>
#include <zlib.h>

class QTemporaryDir;
class QTemporaryFile;
class QXmlStreamReader;
struct FrameData;

class QCToolsManager : public QObject
{
    Q_OBJECT

public:
    explicit QCToolsManager(QObject *parent = nullptr);
    ~QCToolsManager();

    enum class ReportType { GZ, MKV, XML };
    QString getReportPath(ReportType type) const;

    // --- CÁC HÀM TIỆN ÍCH CHUYỂN ĐỔI THỜI GIAN ---
    inline static QString frameToTimecodePrecise(int frame, double fps) {
        if (fps <= 0 || frame < 0) return "00:00:00.000";
        double totalSeconds = frame / fps;
        int totalMilliseconds = static_cast<int>(totalSeconds * 1000.0);
        return QTime(0,0,0,0).addMSecs(totalMilliseconds).toString("HH:mm:ss.zzz");
    }

    inline static QString frameToTimecodeHHMMSSFF(int frame, double fps) {
        if (fps <= 0 || frame < 0) return "00:00:00:00";
        double totalSeconds = frame / fps;
        int hours = static_cast<int>(totalSeconds) / 3600;
        int minutes = (static_cast<int>(totalSeconds) % 3600) / 60;
        int seconds = static_cast<int>(totalSeconds) % 60;
        int frameOfSecond = static_cast<int>(frame % static_cast<int>(round(fps)));
        return QString("%1:%2:%3:%4")
            .arg(hours, 2, 10, QChar('0'))
            .arg(minutes, 2, 10, QChar('0'))
            .arg(seconds, 2, 10, QChar('0'))
            .arg(frameOfSecond, 2, 10, QChar('0'));
    }
    
    // CẢI TIẾN: Thêm các hàm chuyển đổi mới theo yêu cầu
    inline static QString frameToSecondsString(int frame, double fps) {
        if (fps <= 0 || frame < 0) return "0.00";
        return QString::number(frame / fps, 'f', 2);
    }
    
    inline static QString frameToMinutesString(int frame, double fps) {
        if (fps <= 0 || frame < 0) return "0.00";
        return QString::number(frame / fps / 60.0, 'f', 2);
    }


signals:
    void analysisStarted();
    void progressUpdated(int value, int max);
    void statusUpdated(const QString &status);
    void resultsReady(const QList<AnalysisResult> &results);
    void analysisFinished(bool success);
    void errorOccurred(const QString &error);
    void logMessage(const QString& message);
    void backgroundTaskFinished(const QString& message);
    void mediaInfoReady(const MediaInfo& info);

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
    // CẢI TIẾN: Các hàm giải nén giờ là hàm nội bộ, không phải slot
    void decompressGzFile(const QString& gzPath);
    bool processDecompressionChunk(Bytef* in, Bytef* out);

    void startMkvGeneration();
    void extractFromMkv(const QString& mkvPath);
    void cleanup();
    void resetState();
    QString createReportDirectory();
    
    bool parseReport(QIODevice* device); // CẢI TIẾN: Nhận QIODevice để xử lý file thường và file tạm
    
    MediaInfo parseMediaInfo(QXmlStreamReader& xml);
    QList<FrameData> extractAllFrameData(QXmlStreamReader& xml);
    QList<AnalysisResult> runErrorDetection(const QList<FrameData>& allFramesData);
    QMap<int, QSet<QString>> tagFramesForErrors(const QList<FrameData>& allFramesData);
    QList<AnalysisResult> groupErrorsFromTags(const QMap<int, QSet<QString>>& frameTags, const QList<FrameData>& allFramesData);
    void groupBlackFrames(QList<AnalysisResult>& results, const QMap<int, QSet<QString>>& tags, const QList<FrameData>& frames);
    void groupBorderedFrames(QList<AnalysisResult>& results, const QMap<int, QSet<QString>>& tags, const QList<FrameData>& frames);
    void findOrphanFrames(QList<AnalysisResult>& results, const QMap<int, QSet<QString>>& tags, const QList<FrameData>& frames);


    QProcess *m_mainProcess = nullptr;
    QProcess *m_backgroundProcess = nullptr;

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
    const int m_totalStepsAnalyze = 6;
    const int m_totalStepsViewReport = 5;
    int m_totalSteps = 0;
};

#endif // QCTOOLSMANAGER_H
