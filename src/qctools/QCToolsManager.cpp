// src/qctools/QCToolsManager.cpp (Cải tiến Bước 1)
#include "QCToolsManager.h"
#include "core/Constants.h"
#include <QProcess>
#include <QTemporaryDir>
#include <QXmlStreamReader>
#include <QFile>
#include <QDebug>
#include <QRegularExpression>
#include <QDateTime>
#include <QTime>
#include <algorithm>
#include <QTemporaryFile>
#include <zlib.h>
#include <stdexcept>
#include <optional>
#include <QSet>
#include <QMap>
#include <memory>

// =============================================================================
// DATA STRUCTURES & INTERNAL UTILITY FUNCTIONS
// =============================================================================

struct FrameData {
    int frameNum = 0; double yavg = 255.0; double ydif = 0.0;
    int crop_x = -1, crop_y = -1, crop_w = -1, crop_h = -1;
};

struct CropValues {
    int top = 0, bottom = 0, left = 0, right = 0;
    bool isValid() const { return top >= 0; }
    bool hasBorders(double thresholdPercent, int w, int h) const {
        if (w <= 0 || h <= 0) return false;
        if (thresholdPercent <= 0) {
             return (top > 0) || (bottom > 0) || (left > 0) || (right > 0);
        }
        double ratio = thresholdPercent / 100.0;
        return ((double)top / h > ratio) || ((double)bottom / h > ratio) || ((double)left / w > ratio) || ((double)right / w > ratio);
    }
    static CropValues fromFrameData(const FrameData& fd, int w, int h) {
        if (fd.crop_w < 0 || w <= 0 || h <= 0) return {-1,-1,-1,-1};
        return {fd.crop_y, h - (fd.crop_y + fd.crop_h), fd.crop_x, w - (fd.crop_x + fd.crop_w)};
    }
};

struct BorderGroup {
    int startFrame;
    int endFrame;
    int count;
    CropValues minCv;
    CropValues maxCv;
};

static QString formatCropDetails(const CropValues& min_vals, const CropValues& max_vals, int w, int h) {
    if (w <= 0 || h <= 0) return "Kích thước video không xác định";
    QStringList parts;
    auto formatSide = [&](const QString& name, int minVal, int maxVal, int total) {
        if (maxVal < 0 || total <= 0) return;
        if (maxVal == 0 && minVal == 0) return;
        QString valStr = (minVal == maxVal) ? QString("%1px").arg(minVal) : QString("%1>%2px").arg(minVal).arg(maxVal);
        double minP = (double)minVal / total * 100.0, maxP = (double)maxVal / total * 100.0;
        QString pStr = (qAbs(minP - maxP) < 0.1) ? QString("(%1%)").arg(minP, 0, 'f', 1) : QString("(%1>%2%)").arg(minP, 0, 'f', 1).arg(maxP, 0, 'f', 1);
        parts << QString("%1: %2 %3").arg(name, valStr, pStr);
    };
    formatSide("Trên", min_vals.top, max_vals.top, h);
    formatSide("Dưới", min_vals.bottom, max_vals.bottom, h);
    formatSide("Trái", min_vals.left, max_vals.left, w);
    formatSide("Phải", min_vals.right, max_vals.right, w);
    return parts.join(", ");
}

// =============================================================================
// CLASS IMPLEMENTATION: QCToolsManager
// =============================================================================
QCToolsManager::QCToolsManager(QObject *parent) : QObject(parent) {}
QCToolsManager::~QCToolsManager() { cleanup(); }

QString QCToolsManager::getReportPath(ReportType type) const
{
    QString baseNameWithExt;
    QString reportDir;

    if (!m_filePath.isEmpty()) {
        QFileInfo fileInfo(m_filePath);
        baseNameWithExt = fileInfo.fileName();
        reportDir = m_reportDir;
    } else if (!m_sourceReportPath.isEmpty()) {
        QFileInfo fileInfo(m_sourceReportPath);
        QString fileName = fileInfo.fileName();

        if (fileName.endsWith(".qctools.xml.gz")) {
             baseNameWithExt = fileName.left(fileName.length() - 16);
        } else if (fileName.endsWith(".qctools.mkv")) {
             baseNameWithExt = fileName.left(fileName.length() - 12);
        } else if (fileName.endsWith(".qctools.xml")) {
             baseNameWithExt = fileName.left(fileName.length() - 12);
        } else if (fileName.endsWith(".xml.gz")) {
             baseNameWithExt = fileName.left(fileName.length() - 7);
        } else if (fileName.endsWith(".xml")) {
             baseNameWithExt = fileName.left(fileName.length() - 4);
        } else {
            baseNameWithExt = fileInfo.completeBaseName();
        }
        reportDir = fileInfo.absolutePath();
    } else { return QString(); }

    if (reportDir.isEmpty()) return QString();

    switch(type) {
        case ReportType::XML:
            if (m_tempDir && m_tempDir->isValid()) { return m_tempDir->path() + "/" + baseNameWithExt + ".qctools.xml"; }
            return reportDir + "/" + baseNameWithExt + ".qctools.xml";
        case ReportType::GZ: return reportDir + "/" + baseNameWithExt + ".qctools.xml.gz";
        case ReportType::MKV: return reportDir + "/" + baseNameWithExt + ".qctools.mkv";
    }
    return QString();
}


void QCToolsManager::cleanup() {
    if (m_mainProcess) { m_mainProcess->kill(); m_mainProcess->deleteLater(); m_mainProcess = nullptr; }
    if (m_backgroundProcess) { m_backgroundProcess->kill(); m_backgroundProcess->deleteLater(); m_backgroundProcess = nullptr; }
    m_tempDir.reset();
}

void QCToolsManager::resetState() {
    cleanup();
    m_stopRequested = false;
    m_totalFramesFromLog = 0;
    m_totalFrames = 0;
    m_fps = 0;
    m_videoWidth = 0;
    m_videoHeight = 0;
    m_processBuffer.clear();
    m_isGeneratingReport = false;
    m_currentStep = 0;
    m_totalSteps = 0;
    m_currentPhase.clear();
    AnalysisResult::resetIdCounter();
}

void QCToolsManager::requestStop() {
    m_stopRequested = true;
    if (m_mainProcess && m_mainProcess->state() == QProcess::Running) m_mainProcess->kill();
    if (m_backgroundProcess && m_backgroundProcess->state() == QProcess::Running) m_backgroundProcess->kill();
}

void QCToolsManager::doWork(const QString &filePath, const QVariantMap &settings) {
    resetState();

    emit analysisStarted();
    emit logMessage(QString("[%1] Bắt đầu phiên làm việc mới (Phân tích video).").arg(QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss")));

    m_filePath = filePath;
    m_sourceReportPath.clear();
    m_settings = settings;
    m_qcliPath = settings.value(AppConstants::K_QCCLI_PATH).toString();
    m_reportDir = createReportDirectory();

    m_totalSteps = m_totalStepsAnalyze;
    m_currentStep = 1;
    m_currentPhase = "Chuẩn bị Phân tích";
    emit statusUpdated(QString("Bước %1/%2 - %3...").arg(m_currentStep).arg(m_totalSteps).arg(m_currentPhase));
    emit logMessage(QString("[%1] Bắt đầu Bước 1/%2: %3...").arg(QTime::currentTime().toString("hh:mm:ss.zzz")).arg(m_totalSteps).arg(m_currentPhase));
    emit logMessage(QString("   - Thư mục báo cáo: %1").arg(QDir::toNativeSeparators(m_reportDir)));

    if (m_qcliPath.isEmpty() || !QFile::exists(m_qcliPath) || m_reportDir.isEmpty()) {
        emit errorOccurred("Không thể chuẩn bị môi trường phân tích. Vui lòng kiểm tra lại đường dẫn qcli.exe.");
        emit analysisFinished(false); return;
    }

    m_mainProcess = new QProcess(this);
    m_mainProcess->setProcessChannelMode(QProcess::MergedChannels);
    connect(m_mainProcess, &QProcess::finished, this, &QCToolsManager::onAnalysisStage1Finished);
    connect(m_mainProcess, &QProcess::readyRead, this, &QCToolsManager::readAnalysisOutput);

    QStringList args; args << "-i" << m_filePath << "-o" << getReportPath(ReportType::XML) << "-y" << "-s";
    QStringList filters;
    if (m_settings.value(AppConstants::K_DETECT_BLACK_BORDERS, true).toBool()) filters << "cropdetect";
    if (m_settings.value(AppConstants::K_DETECT_BLACK_FRAMES, true).toBool() || m_settings.value(AppConstants::K_DETECT_ORPHAN_FRAMES, true).toBool()) {
        if (!filters.contains("signalstats")) filters << "signalstats";
    }
    if (!filters.isEmpty()) args << "-f" << filters.join("+");

    m_currentPhase = "Phân tích Video (Tạo dữ liệu)";
    emit logMessage(QString("[%1]       -> Bắt đầu chạy qcli.exe để trích xuất dữ liệu frame...").arg(QTime::currentTime().toString("hh:mm:ss.zzz")));
    emit logMessage(QString("   - Lệnh: %1 %2").arg(m_qcliPath).arg(args.join(" ")));
    m_mainProcess->start(m_qcliPath, args);
}

void QCToolsManager::processReportFile(const QString &reportPath, const QVariantMap &settings) {
    resetState();

    emit analysisStarted();
    emit logMessage(QString("[%1] Bắt đầu phiên làm việc mới (Xem báo cáo).").arg(QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss")));
    emit logMessage(QString("   - File: %1").arg(reportPath));

    m_filePath.clear();
    m_sourceReportPath = reportPath;
    m_settings = settings;
    m_qcliPath = settings.value(AppConstants::K_QCCLI_PATH).toString();

    m_totalSteps = m_totalStepsViewReport;

    QFileInfo fileInfo(reportPath);
    QString fileName = fileInfo.fileName().toLower();

    if (m_stopRequested) { emit analysisFinished(false); return; }

    if (fileName.endsWith(".xml") || fileName.endsWith(".qctools.xml")) {
        QFile reportFile(reportPath);
        if (reportFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
            if (parseReport(&reportFile)) {
                emit analysisFinished(true);
            } else {
                emit analysisFinished(false);
            }
        } else {
             emit errorOccurred("Không thể mở file báo cáo XML.");
             emit analysisFinished(false);
        }
    } else if (fileName.endsWith(".xml.gz") || fileName.endsWith(".qctools.xml.gz")) {
        decompressGzFile(reportPath);
    } else if (fileName.endsWith(".mkv") || fileName.endsWith(".qctools.mkv")) {
        m_tempDir = std::make_unique<QTemporaryDir>();
        if (m_tempDir && m_tempDir->isValid()) {
            extractFromMkv(reportPath);
        } else {
            emit errorOccurred("Không thể tạo thư mục tạm để trích xuất.");
            emit analysisFinished(false);
        }
    }
}


void QCToolsManager::onAnalysisStage1Finished(int exitCode, QProcess::ExitStatus exitStatus) {
    readAnalysisOutput();
    if (m_stopRequested) {
        emit analysisFinished(false);
        return;
    }
    if (exitCode != 0) {
        emit errorOccurred("Giai đoạn phân tích và tạo XML thất bại.");
        emit analysisFinished(false);
        return;
    }

    emit logMessage(QString("[%1] Hoàn tất Bước 1 & 2 (Phân tích và Tạo XML).").arg(QTime::currentTime().toString("hh:mm:ss.zzz")));

    QFile reportFile(getReportPath(ReportType::XML));
    if (!reportFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        emit errorOccurred("Không thể mở file báo cáo XML vừa tạo.");
        emit analysisFinished(false);
        return;
    }

    if (parseReport(&reportFile)) {
        if (!m_filePath.isEmpty()) {
            startMkvGeneration();
        } else {
            emit analysisFinished(true);
        }
    } else {
        // Nếu parseReport trả về false, nó có thể là do người dùng đã dừng,
        // không cần phát tín hiệu lỗi nữa.
        if (!m_stopRequested) {
            emit errorOccurred("Phân tích file XML thất bại.");
        }
        emit analysisFinished(false);
    }
}

void QCToolsManager::extractFromMkv(const QString &mkvPath) {
    m_totalSteps = m_totalStepsViewReport;
    m_currentStep = 1;
    m_currentPhase = "Trích xuất từ .mkv";
    emit statusUpdated(QString("Bước %1/%2 - %3...").arg(m_currentStep).arg(m_totalSteps).arg(m_currentPhase));
    emit logMessage(QString("[%1] Bắt đầu Bước 1/%2: %3...").arg(QTime::currentTime().toString("hh:mm:ss.zzz")).arg(m_totalSteps).arg(m_currentPhase));
    m_mainProcess = new QProcess(this);
    connect(m_mainProcess, &QProcess::finished, this, &QCToolsManager::onExtractionFinished);
    QString xmlPath = getReportPath(ReportType::XML);
    QStringList args; args << "-i" << mkvPath << "-o" << xmlPath << "-y" << "-s";
    emit logMessage(QString("   - Lệnh: %1 %2").arg(m_qcliPath).arg(args.join(" ")));
    m_mainProcess->start(m_qcliPath, args);
}

void QCToolsManager::onExtractionFinished(int exitCode, QProcess::ExitStatus exitStatus) {
    if (m_stopRequested) {
        emit analysisFinished(false);
        return;
    }
    if (exitCode != 0) {
        emit errorOccurred("Trích xuất dữ liệu từ .mkv thất bại.");
        emit analysisFinished(false);
        return;
    }
    emit logMessage(QString("[%1] Trích xuất thành công.").arg(QTime::currentTime().toString("hh:mm:ss.zzz")));

    QFile reportFile(getReportPath(ReportType::XML));
    if (!reportFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        emit errorOccurred("Không thể mở file báo cáo XML vừa trích xuất.");
        emit analysisFinished(false);
        return;
    }

    if (parseReport(&reportFile)) {
        emit analysisFinished(true);
    } else {
        emit analysisFinished(false);
    }
}

void QCToolsManager::startMkvGeneration() {
    if (m_filePath.isEmpty()) {
        emit analysisFinished(true);
        return;
    }
    m_currentStep = 6;
    m_currentPhase = "Tạo .qctools.mkv";
    emit logMessage(QString("[%1] Bắt đầu Bước %2/%3: %4...").arg(QTime::currentTime().toString("hh:mm:ss.zzz")).arg(m_currentStep).arg(m_totalSteps).arg(m_currentPhase));
    emit statusUpdated(QString("Bước %1/%2 - %3 ...").arg(m_currentStep).arg(m_totalSteps).arg(m_currentPhase));
    m_backgroundProcess = new QProcess(this);
    connect(m_backgroundProcess, &QProcess::finished, this, &QCToolsManager::onMkvGenerationFinished);
    QStringList args; args << "-i" << m_filePath << "-o" << getReportPath(ReportType::MKV) << "-y";
    emit logMessage(QString("   - Lệnh: %1 %2").arg(m_qcliPath).arg(args.join(" ")));
    m_backgroundProcess->start(m_qcliPath, args);
}

void QCToolsManager::onMkvGenerationFinished(int exitCode, QProcess::ExitStatus exitStatus) {
    if (m_stopRequested) {
        emit logMessage(QString("[%1] Bước 6 (tạo .qctools.mkv) đã được người dùng dừng lại.").arg(QTime::currentTime().toString("hh:mm:ss.zzz")));
        emit analysisFinished(false);
        return;
    }

    QString msg;
    bool success = (exitCode == 0);
    if (success) {
        msg = QString("Đã tạo thành công báo cáo '%1'").arg(QFileInfo(getReportPath(ReportType::MKV)).fileName());
        emit logMessage(QString("[%1] Hoàn tất Bước 6. Đã tạo file .qctools.mkv thành công.").arg(QTime::currentTime().toString("hh:mm:ss.zzz")));
    } else {
        msg = "Lỗi: Không thể tạo báo cáo .qctools.mkv";
        emit logMessage(QString("[%1] Bước 6 (tạo .qctools.mkv) thất bại.").arg(QTime::currentTime().toString("hh:mm:ss.zzz")));
    }
    emit backgroundTaskFinished(msg);
    emit analysisFinished(true);
}

void QCToolsManager::readAnalysisOutput() {
    QProcess *source = qobject_cast<QProcess*>(sender());
    if (!source) return;

    m_processBuffer.append(source->readAll());

    while (m_processBuffer.contains('\r') || m_processBuffer.contains('\n')) {
        int endOfLineIndex = -1;
        int crIndex = m_processBuffer.indexOf('\r');
        int lfIndex = m_processBuffer.indexOf('\n');

        if (crIndex != -1 && lfIndex != -1) {
            endOfLineIndex = qMin(crIndex, lfIndex);
        } else if (crIndex != -1) {
            endOfLineIndex = crIndex;
        } else {
            endOfLineIndex = lfIndex;
        }

        QString line = m_processBuffer.left(endOfLineIndex).trimmed();
        m_processBuffer.remove(0, endOfLineIndex + 1);

        if (line.isEmpty()) continue;

        if (!m_isGeneratingReport && line.contains("generating QCTools report")) {
            m_isGeneratingReport = true;
            m_currentStep = 2;
            m_currentPhase = "Tạo Báo cáo XML";
            emit statusUpdated(QString("Bước %1/%2 - %3...").arg(m_currentStep).arg(m_totalSteps).arg(m_currentPhase));
            emit logMessage(QString("[%1] Bắt đầu Bước 2/%2: %3...").arg(QTime::currentTime().toString("hh:mm:ss.zzz")).arg(m_totalSteps).arg(m_currentPhase));
        }

        QRegularExpression re("(\\d+)\\s+of\\s+(\\d+)");

        auto it = re.globalMatch(line);
        if (it.hasNext()) {
            auto match = it.next();
            int currentFrame = match.captured(1).toInt();
            int totalFrames = match.captured(2).toInt();

            if (m_totalFramesFromLog == 0 && totalFrames > 0 && !m_isGeneratingReport) {
                m_totalFramesFromLog = totalFrames;
            }
            if (totalFrames > 0) {
                emit statusUpdated(QString("Bước %1/%2 - %3").arg(m_currentStep).arg(m_totalSteps).arg(m_currentPhase));
                emit progressUpdated(currentFrame, totalFrames);
            }
        }
    }
}

// =============================================================================
// DECOMPRESSION LOGIC (CẢI TIẾN)
// =============================================================================

#define CHUNK_SIZE 16384 // Sử dụng buffer 16KB

void QCToolsManager::decompressGzFile(const QString &gzPath) {
    m_currentStep = 1;
    m_currentPhase = "Giải nén báo cáo";
    emit statusUpdated(QString("Bước %1/%2 - %3...").arg(m_currentStep).arg(m_totalSteps).arg(m_currentPhase));
    emit progressUpdated(0, 100);
    emit logMessage(QString("[%1] Bắt đầu Bước 1/%2: %3 file nén...").arg(QTime::currentTime().toString("hh:mm:ss.zzz")).arg(m_totalSteps).arg(m_currentPhase));

    QFile gzFile(gzPath);
    if (!gzFile.open(QIODevice::ReadOnly)) {
        emit errorOccurred("Không thể mở file nén .gz để đọc.");
        emit analysisFinished(false);
        return;
    }

    auto tempOutFile = std::make_unique<QTemporaryFile>();
    if (!tempOutFile->open()) {
        emit errorOccurred("Không thể tạo file tạm để ghi dữ liệu giải nén.");
        emit analysisFinished(false);
        return;
    }

    z_stream zStream;
    zStream.zalloc = Z_NULL;
    zStream.zfree = Z_NULL;
    zStream.opaque = Z_NULL;
    zStream.avail_in = 0;
    zStream.next_in = Z_NULL;
    if (inflateInit2(&zStream, 16 + MAX_WBITS) != Z_OK) {
        emit errorOccurred("Khởi tạo zlib thất bại.");
        emit analysisFinished(false);
        return;
    }

    Bytef in[CHUNK_SIZE];
    Bytef out[CHUNK_SIZE];
    int ret;
    qint64 bytesRead = 0;
    qint64 totalSize = gzFile.size();

    do {
        if (m_stopRequested) {
            (void)inflateEnd(&zStream);
            emit analysisFinished(false);
            return;
        }

        zStream.avail_in = gzFile.read(reinterpret_cast<char*>(in), CHUNK_SIZE);
        bytesRead += zStream.avail_in;
        if (gzFile.error() != QFile::NoError) {
            emit errorOccurred("Lỗi khi đọc từ file .gz: " + gzFile.errorString());
            (void)inflateEnd(&zStream);
            emit analysisFinished(false);
            return;
        }

        if (zStream.avail_in == 0) break;
        zStream.next_in = in;
        
        do {
            zStream.avail_out = CHUNK_SIZE;
            zStream.next_out = out;
            ret = inflate(&zStream, Z_NO_FLUSH);

            switch (ret) {
                case Z_STREAM_ERROR: case Z_NEED_DICT: case Z_DATA_ERROR: case Z_MEM_ERROR:
                    emit errorOccurred(QString("Lỗi giải nén zlib nghiêm trọng: mã lỗi %1").arg(ret));
                    (void)inflateEnd(&zStream);
                    emit analysisFinished(false);
                    return;
            }

            unsigned int have = CHUNK_SIZE - zStream.avail_out;
            if (tempOutFile->write(reinterpret_cast<const char*>(out), have) != have) {
                emit errorOccurred("Lỗi khi ghi vào file tạm: " + tempOutFile->errorString());
                (void)inflateEnd(&zStream);
                emit analysisFinished(false);
                return;
            }
        } while (zStream.avail_out == 0);

        if (totalSize > 0) {
            emit progressUpdated(bytesRead, totalSize);
        }

    } while (ret != Z_STREAM_END);

    (void)inflateEnd(&zStream);
    gzFile.close();
    
    if (m_stopRequested) {
        emit analysisFinished(false);
        return;
    }

    emit progressUpdated(100, 100);
    emit logMessage(QString("[%1] Giải nén thành công.").arg(QTime::currentTime().toString("hh:mm:ss.zzz")));

    // CẢI TIẾN: Mở lại file tạm để đọc và phân tích
    tempOutFile->seek(0);
    if (parseReport(tempOutFile.get())) {
         emit analysisFinished(true);
    } else {
         emit analysisFinished(false);
    }
}


// =============================================================================
// REPORT PARSING LOGIC
// =============================================================================

bool QCToolsManager::parseReport(QIODevice* device) {
    if (!device) {
        emit errorOccurred("Thiết bị đọc file báo cáo không hợp lệ.");
        return false;
    }
    m_currentStep++;
    m_currentPhase = "Đọc & Phân tích Báo cáo";
    emit statusUpdated(QString("Bước %1/%2 - %3...").arg(m_currentStep).arg(m_totalSteps).arg(m_currentPhase));
    emit progressUpdated(0, 100);
    emit logMessage(QString("[%1] Bắt đầu Bước %2/%3: %4...").arg(QTime::currentTime().toString("hh:mm:ss.zzz")).arg(m_currentStep).arg(m_totalSteps).arg(m_currentPhase));

    QXmlStreamReader xml(device);
    
    // CẢI TIẾN: Luôn kiểm tra yêu cầu dừng
    if (m_stopRequested) { return false; }
    
    MediaInfo mediaInfo = parseMediaInfo(xml);
    device->seek(0);
    xml.setDevice(device);

    if (m_stopRequested) { return false; }

    QList<FrameData> allFramesData = extractAllFrameData(xml);
    
    if (m_stopRequested) { return false; }

    if (xml.hasError()) {
        QString errorMsg = QString("Lỗi phân tích cú pháp XML: %1 (Dòng %2, Cột %3)").arg(xml.errorString()).arg(xml.lineNumber()).arg(xml.columnNumber());
        emit errorOccurred(errorMsg);
        return false;
    }
    
    emit progressUpdated(100, 100);
    emit mediaInfoReady(mediaInfo);

    m_fps = mediaInfo.fps;
    m_videoWidth = mediaInfo.width;
    m_videoHeight = mediaInfo.height;
    if (m_totalFramesFromLog > 0) m_totalFrames = m_totalFramesFromLog;

    if (m_fps <= 0) {
        emit errorOccurred("Lỗi nghiêm trọng: Không thể xác định FPS của video từ báo cáo. Dữ liệu timecode sẽ không chính xác.");
        return false;
    }
    if (m_videoWidth <= 0 || m_videoHeight <= 0) {
        emit errorOccurred(QString("Lỗi: Đã đọc xong file XML nhưng không tìm thấy thông tin video stream hợp lệ (width/height=%1x%2).").arg(m_videoWidth).arg(m_videoHeight));
        return false;
    }
    if (allFramesData.isEmpty() && !m_stopRequested) {
        emit errorOccurred("Lỗi: Đã đọc xong file XML nhưng không tìm thấy dữ liệu của bất kỳ frame nào.");
        return false;
    }
    if (m_stopRequested) { return false; }

    emit logMessage(QString("[%1]     -> Đã đọc xong. Tìm thấy %2 frame. Bắt đầu tổng hợp lỗi...").arg(QTime::currentTime().toString("hh:mm:ss.zzz")).arg(allFramesData.count()));
    if (m_totalFrames <= 0) m_totalFrames = allFramesData.size();

    QList<AnalysisResult> finalResults = runErrorDetection(allFramesData);
    if (m_stopRequested) { return false; }

    if (!finalResults.isEmpty()) {
        emit logMessage(QString("[%1] Tổng hợp xong. Tìm thấy %2 lỗi.").arg(QTime::currentTime().toString("hh:mm:ss.zzz")).arg(finalResults.count()));
        emit resultsReady(finalResults);
    } else {
        emit logMessage(QString("[%1] Tổng hợp xong. Không tìm thấy lỗi nào với cấu hình hiện tại.").arg(QTime::currentTime().toString("hh:mm:ss.zzz")));
        emit resultsReady({});
    }
    return true;
}

MediaInfo QCToolsManager::parseMediaInfo(QXmlStreamReader &xml) {
    MediaInfo info;
    bool inFormatSection = false;
    bool foundVideoStream = false;
    bool foundAudioStream = false;

    while (!xml.atEnd()) {
        if (m_stopRequested) return {};
        xml.readNext();
        if (xml.isStartElement()) {
            if (xml.name() == QLatin1String("format")) {
                inFormatSection = true;
                const auto& attrs = xml.attributes();
                info.formatName = attrs.value("format_long_name").toString();
                info.duration = attrs.value("duration").toDouble();
                info.size = attrs.value("size").toLongLong();
                info.bitrate = attrs.value("bit_rate").toLongLong();
            } else if (inFormatSection && xml.name() == QLatin1String("tag") && xml.attributes().value("key") == QLatin1String("creation_time")) {
                info.creationTime = QDateTime::fromString(xml.attributes().value("value").toString(), Qt::ISODateWithMs);
            } else if (xml.name() == QLatin1String("stream")) {
                const auto& attrs = xml.attributes();
                if (!foundVideoStream && attrs.value("codec_type") == QLatin1String("video")) {
                    QStringList parts = attrs.value("r_frame_rate").toString().split('/');
                    if (parts.size() == 2 && parts[1].toDouble() != 0) {
                        info.fps = parts[0].toDouble() / parts[1].toDouble();
                    }
                    info.width = attrs.value("width").toInt();
                    info.height = attrs.value("height").toInt();
                    if (attrs.hasAttribute("nb_frames")) m_totalFrames = attrs.value("nb_frames").toInt();
                    info.videoCodec = attrs.value("codec_long_name").toString();
                    info.pixelFormat = attrs.value("pix_fmt").toString();
                    info.colorSpace = attrs.value("color_space").toString();
                    foundVideoStream = true;
                } else if (!foundAudioStream && attrs.value("codec_type") == QLatin1String("audio")) {
                    info.audioCodec = attrs.value("codec_long_name").toString();
                    info.sampleRate = attrs.value("sample_rate").toInt();
                    info.channelLayout = attrs.value("channel_layout").toString();
                    foundAudioStream = true;
                }
            }
        } else if (xml.isEndElement()) {
            if (xml.name() == QLatin1String("format")) {
                inFormatSection = false;
            }
            if (foundVideoStream && foundAudioStream) {
                break;
            }
        }
    }
    return info;
}


QList<FrameData> QCToolsManager::extractAllFrameData(QXmlStreamReader &xml)
{
    QList<FrameData> allFramesData;
    qint64 fileSize = xml.device() ? xml.device()->size() : 0;
    int progressCounter = 0;

    while (!xml.atEnd()) {
        if (m_stopRequested) return {};
        
        // CẢI TIẾN: Báo cáo tiến trình đọc file XML
        if (fileSize > 0 && ++progressCounter % 20 == 0) {
             emit progressUpdated(xml.device()->pos(), fileSize);
        }

        xml.readNext();
        if (xml.isStartElement() && xml.name() == QLatin1String("frame")) {
            FrameData currentFrame;
            currentFrame.frameNum = xml.attributes().value("pkt_pts").toInt();

            int x1 = -1, y1 = -1, x2 = -1, y2 = -1;

            while(xml.readNextStartElement()) {
                if(xml.name() == QLatin1String("tag")) {
                    const auto& attrs = xml.attributes();
                    const auto& key = attrs.value("key").toString();
                    const auto& value = attrs.value("value").toString();
                    if (key == "lavfi.signalstats.YAVG") currentFrame.yavg = value.toDouble();
                    else if (key == "lavfi.signalstats.YDIF") currentFrame.ydif = value.toDouble();
                    else if (key == "lavfi.cropdetect.x1") x1 = static_cast<int>(value.toDouble());
                    else if (key == "lavfi.cropdetect.y1") y1 = static_cast<int>(value.toDouble());
                    else if (key == "lavfi.cropdetect.x2") x2 = static_cast<int>(value.toDouble());
                    else if (key == "lavfi.cropdetect.y2") y2 = static_cast<int>(value.toDouble());
                }
                xml.skipCurrentElement();
            }

            if (x1 != -1 && y1 != -1 && x2 != -1 && y2 != -1) {
                currentFrame.crop_x = x1;
                currentFrame.crop_y = y1;
                currentFrame.crop_w = x2 - x1 + 1;
                currentFrame.crop_h = y2 - y1 + 1;
            }
            allFramesData.append(currentFrame);
        }
    }
    return allFramesData;
}


QList<AnalysisResult> QCToolsManager::runErrorDetection(const QList<FrameData> &allFramesData)
{
    m_currentStep++;
    m_currentPhase = "Gắn thẻ các frame";
    emit statusUpdated(QString("Bước %1/%2 - %3...").arg(m_currentStep).arg(m_totalSteps).arg(m_currentPhase));
    emit progressUpdated(0, 100);
    emit logMessage(QString("[%1] Bắt đầu Bước %2/%3: %4...").arg(QTime::currentTime().toString("hh:mm:ss.zzz")).arg(m_currentStep).arg(m_totalSteps).arg(m_currentPhase));

    if (m_stopRequested) return {};
    QMap<int, QSet<QString>> frameTags = tagFramesForErrors(allFramesData);
    
    if (m_stopRequested) return {};

    emit progressUpdated(100, 100);
    emit logMessage(QString("[%1]       - Hoàn tất.").arg(QTime::currentTime().toString("hh:mm:ss.zzz")));

    m_currentStep++;
    m_currentPhase = "Gom nhóm lỗi";
    emit statusUpdated(QString("Bước %1/%2 - %3...").arg(m_currentStep).arg(m_totalSteps).arg(m_currentPhase));
    emit progressUpdated(0, 100);
    emit logMessage(QString("[%1] Bắt đầu Bước %2/%3: %3...").arg(QTime::currentTime().toString("hh:mm:ss.zzz")).arg(m_currentStep).arg(m_totalSteps).arg(m_currentPhase));

    if (m_stopRequested) return {};
    QList<AnalysisResult> finalResults = groupErrorsFromTags(frameTags, allFramesData);

    if (m_stopRequested) return {};

    emit progressUpdated(100, 100);
    emit logMessage(QString("[%1]       - Hoàn tất.").arg(QTime::currentTime().toString("hh:mm:ss.zzz")));

    return finalResults;
}

QMap<int, QSet<QString>> QCToolsManager::tagFramesForErrors(const QList<FrameData> &allFramesData)
{
    QMap<int, QSet<QString>> frameTags;
    const double blackFrameThresh = m_settings.value(AppConstants::K_BLACK_FRAME_THRESH, 17.0).toDouble();
    const double borderThreshPercent = m_settings.value(AppConstants::K_BORDER_THRESH, 0.2).toDouble();
    const double sceneThresh = m_settings.value(AppConstants::K_SCENE_THRESH, 30.0).toDouble();
    const bool hasTransitions = m_settings.value(AppConstants::K_HAS_TRANSITIONS, false).toBool();
    const int total = allFramesData.size();

    for(int i = 0; i < total; ++i) {
        if (m_stopRequested) return {};
        if (i % 500 == 0) emit progressUpdated(i, total);

        const auto& frame = allFramesData[i];
        if (m_settings.value(AppConstants::K_DETECT_BLACK_FRAMES, true).toBool() && frame.yavg < blackFrameThresh) {
            frameTags[frame.frameNum].insert(AppConstants::TAG_IS_BLACK);
        }
        if (m_settings.value(AppConstants::K_DETECT_BLACK_BORDERS, true).toBool()) {
            bool isAlreadyBlack = frameTags.contains(frame.frameNum) && frameTags[frame.frameNum].contains(AppConstants::TAG_IS_BLACK);
            if (!isAlreadyBlack) {
                CropValues cv = CropValues::fromFrameData(frame, m_videoWidth, m_videoHeight);
                if (cv.isValid() && cv.hasBorders(borderThreshPercent, m_videoWidth, m_videoHeight)) {
                    frameTags[frame.frameNum].insert(AppConstants::TAG_HAS_BORDER);
                }
            }
        }
        if (m_settings.value(AppConstants::K_DETECT_ORPHAN_FRAMES, true).toBool() && i > 0) {
            if (hasTransitions) {
                if (i < allFramesData.size() - 1) {
                    const auto& prev = allFramesData[i-1];
                    const auto& next = allFramesData[i+1];
                    if (frame.ydif > sceneThresh && frame.ydif > prev.ydif && frame.ydif > next.ydif) {
                        frameTags[frame.frameNum].insert(AppConstants::TAG_IS_SCENE_CUT);
                    }
                }
            } else {
                bool isNormalCut = (frame.ydif > sceneThresh && allFramesData[i-1].ydif < (sceneThresh / 2.0));
                bool isTinySceneEnd = false;
                if (i + 1 < allFramesData.size()) {
                    isTinySceneEnd = (frame.ydif > sceneThresh && allFramesData[i-1].ydif > sceneThresh && allFramesData[i+1].ydif < (sceneThresh / 2.0));
                }
                if (isNormalCut || isTinySceneEnd) {
                     frameTags[frame.frameNum].insert(AppConstants::TAG_IS_SCENE_CUT);
                }
            }
        }
    }
    return frameTags;
}

QList<AnalysisResult> QCToolsManager::groupErrorsFromTags(const QMap<int, QSet<QString>> &frameTags, const QList<FrameData> &allFramesData)
{
    QList<AnalysisResult> finalResults;
    if (m_settings.value(AppConstants::K_DETECT_BLACK_FRAMES, true).toBool()) {
        if (m_stopRequested) return {};
        groupBlackFrames(finalResults, frameTags, allFramesData);
    }
    if (m_settings.value(AppConstants::K_DETECT_BLACK_BORDERS, true).toBool()) {
        if (m_stopRequested) return {};
        groupBorderedFrames(finalResults, frameTags, allFramesData);
    }
    if (m_settings.value(AppConstants::K_DETECT_ORPHAN_FRAMES, true).toBool()) {
        if (m_stopRequested) return {};
        findOrphanFrames(finalResults, frameTags, allFramesData);
    }
    return finalResults;
}

void QCToolsManager::groupBlackFrames(QList<AnalysisResult> &results, const QMap<int, QSet<QString>> &tags, const QList<FrameData> &frames)
{
    QList<FrameData> currentGroup;
    for(const auto& frame : frames) {
        if(m_stopRequested) return;
        bool isBlack = tags.contains(frame.frameNum) && tags[frame.frameNum].contains(AppConstants::TAG_IS_BLACK);
        if (isBlack) {
            currentGroup.append(frame);
        } else {
            if (!currentGroup.isEmpty()) {
                double yavgSum = 0;
                for(const auto& f : currentGroup) yavgSum += f.yavg;
                const int startFrame = currentGroup.first().frameNum;
                const int endFrame = currentGroup.last().frameNum;
                QString details = QString("Frame tối (YAVG TB: %1), từ frame %2 đến %3")
                                      .arg(yavgSum / currentGroup.count(), 0, 'f', 2)
                                      .arg(startFrame)
                                      .arg(endFrame);
                results.append({ QCToolsManager::frameToTimecodeHHMMSSFF(startFrame, m_fps), QString::number(currentGroup.count()), AppConstants::ERR_BLACK_FRAME, details, startFrame });
                currentGroup.clear();
            }
        }
    }
    if (!currentGroup.isEmpty()) {
         double yavgSum = 0;
         for(const auto& f : currentGroup) yavgSum += f.yavg;
         const int startFrame = currentGroup.first().frameNum;
         const int endFrame = currentGroup.last().frameNum;
         QString details = QString("Frame tối (YAVG TB: %1), từ frame %2 đến %3")
                               .arg(yavgSum / currentGroup.count(), 0, 'f', 2)
                               .arg(startFrame)
                               .arg(endFrame);
         results.append({ QCToolsManager::frameToTimecodeHHMMSSFF(startFrame, m_fps), QString::number(currentGroup.count()), AppConstants::ERR_BLACK_FRAME, details, startFrame });
    }
}

void QCToolsManager::groupBorderedFrames(QList<AnalysisResult> &results, const QMap<int, QSet<QString>> &tags, const QList<FrameData> &frames)
{
    std::optional<BorderGroup> currentGroupOpt;
    for(const auto& frame : frames) {
        if(m_stopRequested) return;
        bool isBordered = tags.contains(frame.frameNum) && tags[frame.frameNum].contains(AppConstants::TAG_HAS_BORDER);
         if (isBordered) {
            CropValues cv = CropValues::fromFrameData(frame, m_videoWidth, m_videoHeight);
            if (!currentGroupOpt.has_value()) {
                currentGroupOpt.emplace(BorderGroup{frame.frameNum, frame.frameNum, 1, cv, cv});
            } else {
                BorderGroup& currentGroup = currentGroupOpt.value();
                currentGroup.endFrame = frame.frameNum;
                currentGroup.count++;
                currentGroup.minCv.top = std::min(currentGroup.minCv.top, cv.top); currentGroup.maxCv.top = std::max(currentGroup.maxCv.top, cv.top);
                currentGroup.minCv.bottom = std::min(currentGroup.minCv.bottom, cv.bottom); currentGroup.maxCv.bottom = std::max(currentGroup.maxCv.bottom, cv.bottom);
                currentGroup.minCv.left = std::min(currentGroup.minCv.left, cv.left); currentGroup.maxCv.left = std::max(currentGroup.maxCv.left, cv.left);
                currentGroup.minCv.right = std::min(currentGroup.minCv.right, cv.right); currentGroup.maxCv.right = std::max(currentGroup.maxCv.right, cv.right);
            }
        } else {
            if (currentGroupOpt.has_value()) {
                QString details = formatCropDetails(currentGroupOpt->minCv, currentGroupOpt->maxCv, m_videoWidth, m_videoHeight) +
                                  QString(", từ frame %1 đến %2").arg(currentGroupOpt->startFrame).arg(currentGroupOpt->endFrame);
                results.append({ QCToolsManager::frameToTimecodeHHMMSSFF(currentGroupOpt->startFrame, m_fps), QString::number(currentGroupOpt->count), AppConstants::ERR_BLACK_BORDER, details, currentGroupOpt->startFrame });
                currentGroupOpt.reset();
            }
        }
    }
    if (currentGroupOpt.has_value()) {
        QString details = formatCropDetails(currentGroupOpt->minCv, currentGroupOpt->maxCv, m_videoWidth, m_videoHeight) +
                          QString(", từ frame %1 đến %2").arg(currentGroupOpt->startFrame).arg(currentGroupOpt->endFrame);
        results.append({ QCToolsManager::frameToTimecodeHHMMSSFF(currentGroupOpt->startFrame, m_fps), QString::number(currentGroupOpt->count), AppConstants::ERR_BLACK_BORDER, details, currentGroupOpt->startFrame });
    }
}

void QCToolsManager::findOrphanFrames(QList<AnalysisResult> &results, const QMap<int, QSet<QString>> &tags, const QList<FrameData> &frames)
{
    const int orphanThresh = m_settings.value(AppConstants::K_ORPHAN_THRESH, 5).toInt();
    if (orphanThresh <= 0) return;

    QList<int> scene_cuts;
    scene_cuts.append(0);
    for(const auto& frame : frames) {
        if(tags.value(frame.frameNum).contains(AppConstants::TAG_IS_SCENE_CUT)) {
            scene_cuts.append(frame.frameNum);
        }
    }
    if (m_totalFrames > 0 && (scene_cuts.isEmpty() || scene_cuts.last() != m_totalFrames)) {
        scene_cuts.append(m_totalFrames);
    }
    
    std::sort(scene_cuts.begin(), scene_cuts.end());
    auto last = std::unique(scene_cuts.begin(), scene_cuts.end());
    scene_cuts.erase(last, scene_cuts.end());

    for (int i = 0; i < scene_cuts.size() - 1; ++i) {
        if(m_stopRequested) return;
        const int startFrame = scene_cuts[i];
        
        if (startFrame == 0) {
            continue;
        }

        const int endFrame = scene_cuts[i+1];
        const int duration = endFrame - startFrame;

        if (duration <= 0 || duration > orphanThresh) {
            continue;
        }

        bool sceneContainsNonBlackFrames = false;
        for(int j = startFrame; j < endFrame; ++j) {
            if (!tags.value(j).contains(AppConstants::TAG_IS_BLACK)) {
                sceneContainsNonBlackFrames = true;
                break;
            }
        }

        if (sceneContainsNonBlackFrames) {
            results.append({ QCToolsManager::frameToTimecodeHHMMSSFF(startFrame, m_fps), QString::number(duration), AppConstants::ERR_ORPHAN_FRAME, QString("Cảnh ngắn bất thường, từ frame %1 đến %2").arg(startFrame).arg(endFrame - 1), startFrame });
        }
    }
}


QString QCToolsManager::createReportDirectory() {
    if (m_filePath.isEmpty()) return QString();
    QFileInfo fileInfo(m_filePath);
    return fileInfo.absolutePath();
}
