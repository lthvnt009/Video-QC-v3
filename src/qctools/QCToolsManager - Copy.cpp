// src/qctools/QCToolsManager.cpp (v47.3 - Final Data Parsing Fix)
#include "QCToolsManager.h"
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

// =============================================================================
// CẤU TRÚC DỮ LIỆU & HÀM TIỆN ÍCH NỘI BỘ
// =============================================================================

static QString frameToTimecodeHHMMSSFF(int frame, double fps) {
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

struct FrameData {
    int frameNum = 0; double yavg = 255.0; double ydif = 0.0;
    int crop_x = -1, crop_y = -1, crop_w = -1, crop_h = -1;
};

struct CropValues {
    int top = 0, bottom = 0, left = 0, right = 0;
    bool isValid() const { return top >= 0; }
    bool hasBorders() const {
        return (top > 0) || (bottom > 0) || (left > 0) || (right > 0);
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

static QString decompressGzFile(const QString& gzPath)
{
    QFile gzFile(gzPath);
    if (!gzFile.open(QIODevice::ReadOnly)) {
        qWarning() << "Cannot open gzipped file:" << gzPath;
        return QString();
    }
    QByteArray compressedData = gzFile.readAll();
    gzFile.close();

    if (compressedData.isEmpty()) {
        qWarning() << "Gzip file is empty.";
        return QString();
    }

    z_stream strm;
    strm.zalloc = Z_NULL;
    strm.zfree = Z_NULL;
    strm.opaque = Z_NULL;
    strm.avail_in = compressedData.size();
    strm.next_in = (Bytef*)compressedData.data();

    if (inflateInit2(&strm, 16 + MAX_WBITS) != Z_OK) {
        qWarning() << "Zlib inflateInit2 failed!";
        return QString();
    }

    QByteArray uncompressedData;
    char buffer[4096];
    int ret;
    do {
        strm.avail_out = sizeof(buffer);
        strm.next_out = (Bytef*)buffer;
        ret = inflate(&strm, Z_NO_FLUSH);
        if (ret != Z_OK && ret != Z_STREAM_END) {
            (void)inflateEnd(&strm);
            qWarning() << "Zlib inflation failed with code:" << ret;
            return QString();
        }
        int have = sizeof(buffer) - strm.avail_out;
        uncompressedData.append(buffer, have);
    } while (strm.avail_out == 0);
    
    (void)inflateEnd(&strm);
    
    QTemporaryFile* outFile = new QTemporaryFile();
    if (!outFile->open()) {
        delete outFile;
        qWarning() << "Could not open temporary file for writing decompressed data.";
        return QString();
    }
    outFile->write(uncompressedData);
    QString tempPath = outFile->fileName();
    outFile->setAutoRemove(false);
    outFile->close();
    delete outFile;
    return tempPath;
}


// =============================================================================
// TRIỂN KHAI LỚP QCToolsManager
// =============================================================================
QCToolsManager::QCToolsManager(QObject *parent) : QObject(parent) {}
QCToolsManager::~QCToolsManager() { cleanup(); }

QString QCToolsManager::getReportPath(ReportType type) const
{
    QString baseName; QString reportDir;
    if (!m_filePath.isEmpty()) {
        QFileInfo fileInfo(m_filePath);
        baseName = fileInfo.completeBaseName();
        reportDir = m_reportDir;
    } else if (!m_sourceReportPath.isEmpty()) {
        QFileInfo fileInfo(m_sourceReportPath);
        baseName = fileInfo.completeBaseName();
        if (baseName.endsWith(".qctools.xml")) { baseName.chop(12); }
        reportDir = fileInfo.absolutePath();
    } else { return QString(); }

    if (reportDir.isEmpty()) return QString();

    switch(type) {
        case ReportType::XML:
            if (m_tempDir) { return m_tempDir->path() + "/" + baseName + ".xml"; }
            return reportDir + "/" + baseName + ".xml";
        case ReportType::GZ: return reportDir + "/" + baseName + ".xml.gz";
        case ReportType::MKV: return reportDir + "/" + baseName + ".mkv";
    }
    return QString();
}

void QCToolsManager::cleanup() {
    if (m_mainProcess) { m_mainProcess->kill(); m_mainProcess->deleteLater(); m_mainProcess = nullptr; }
    if (m_backgroundProcess) { m_backgroundProcess->kill(); m_backgroundProcess->deleteLater(); m_backgroundProcess = nullptr; }
    if (m_tempDir) { delete m_tempDir; m_tempDir = nullptr; }
}

void QCToolsManager::requestStop() {
    m_stopRequested = true;
    if (m_mainProcess && m_mainProcess->state() == QProcess::Running) m_mainProcess->kill();
    if (m_backgroundProcess && m_backgroundProcess->state() == QProcess::Running) m_backgroundProcess->kill();
}

void QCToolsManager::doWork(const QString &filePath, const QVariantMap &settings) {
    cleanup();
    m_stopRequested = false; m_totalFramesFromLog = 0; m_totalFrames = 0; m_fps = 0; m_videoWidth = 0; m_videoHeight = 0;
    m_processBuffer.clear(); m_isGeneratingReport = false;
    emit analysisStarted();
    emit logMessage(QString("[%1] Bắt đầu phiên làm việc mới (Phân tích video).").arg(QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss")));
    
    m_filePath = filePath; m_sourceReportPath.clear();
    m_settings = settings; m_qcliPath = settings.value("qctoolsPath").toString();
    m_reportDir = createReportDirectory();
    
    emit logMessage(QString("[%1] Chuẩn bị môi trường...").arg(QTime::currentTime().toString("hh:mm:ss.zzz")));
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
    if (m_settings.value("detectBlackBorders", true).toBool()) filters << "cropdetect";
    if (m_settings.value("detectBlackFrames", true).toBool() || m_settings.value("detectOrphanFrames", true).toBool()) {
        if (!filters.contains("signalstats")) filters << "signalstats";
    }
    if (!filters.isEmpty()) args << "-f" << filters.join("+");

    emit logMessage(QString("[%1] Bắt đầu Giai đoạn 1 (tạo .xml)...").arg(QTime::currentTime().toString("hh:mm:ss.zzz")));
    emit logMessage(QString("   - Lệnh: %1 %2").arg(m_qcliPath).arg(args.join(" ")));
    m_mainProcess->start(m_qcliPath, args);
}

void QCToolsManager::processReportFile(const QString &reportPath, const QVariantMap &settings) {
    cleanup();
    m_stopRequested = false; m_totalFrames = 0; m_fps = 0; m_videoWidth = 0; m_videoHeight = 0;
    emit analysisStarted();
    emit logMessage(QString("[%1] Bắt đầu phiên làm việc mới (Xem báo cáo).").arg(QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss")));
    emit logMessage(QString("   - File: %1").arg(reportPath));
    
    m_filePath.clear(); m_sourceReportPath = reportPath;
    m_settings = settings; m_qcliPath = settings.value("qctoolsPath").toString();

    QString suffix = QFileInfo(reportPath).suffix().toLower();
    
    try {
        if (suffix == "xml") {
            parseReport(reportPath);
        } else if (suffix == "gz") {
            emit logMessage(QString("[%1] Bắt đầu giải nén file .gz...").arg(QTime::currentTime().toString("hh:mm:ss.zzz")));
            m_tempDir = new QTemporaryDir();
            if (m_tempDir->isValid()) {
                QString decompressedPath = decompressGzFile(reportPath);
                if (!decompressedPath.isEmpty()) {
                    emit logMessage(QString("[%1] Giải nén thành công vào: %2").arg(QTime::currentTime().toString("hh:mm:ss.zzz"), decompressedPath));
                    parseReport(decompressedPath);
                } else {
                    emit errorOccurred("Giải nén file .gz thất bại.");
                    throw std::runtime_error("Gzip decompression failed.");
                }
            } else {
                 emit errorOccurred("Không thể tạo thư mục tạm để giải nén.");
                 throw std::runtime_error("Failed to create temp dir.");
            }
        } else if (suffix == "mkv") {
            m_tempDir = new QTemporaryDir();
            if (m_tempDir->isValid()) {
                extractFromMkv(reportPath); 
                return; 
            } else {
                emit errorOccurred("Không thể tạo thư mục tạm để trích xuất.");
                 throw std::runtime_error("Failed to create temp dir.");
            }
        }
        emit analysisFinished(true);
    } catch (const std::exception& e) {
        qWarning() << "Caught exception during report processing:" << e.what();
        emit analysisFinished(false);
    }
}

void QCToolsManager::onAnalysisStage1Finished(int exitCode, QProcess::ExitStatus exitStatus) {
    readAnalysisOutput();
    if (m_stopRequested) {
        emit analysisFinished(false);
        return;
    }
    if (exitCode != 0) {
        emit errorOccurred("Giai đoạn 1 (tạo XML) thất bại.");
        emit analysisFinished(false); 
        return;
    }
    
    emit logMessage(QString("[%1] Hoàn tất Giai đoạn 1. Bắt đầu phân tích báo cáo...").arg(QTime::currentTime().toString("hh:mm:ss.zzz")));
    
    try {
        parseReport(getReportPath(ReportType::XML));

        if (!m_filePath.isEmpty()) { 
            startMkvGeneration(); 
        } else { 
            emit analysisFinished(true); 
        }
    } catch (const std::exception& e) {
        qWarning() << "Caught exception after stage 1:" << e.what();
        emit analysisFinished(false);
    }
}

void QCToolsManager::extractFromMkv(const QString &mkvPath) {
    emit statusUpdated("Đang trích xuất dữ liệu từ .mkv...");
    emit logMessage(QString("[%1] Bắt đầu trích xuất dữ liệu từ .mkv...").arg(QTime::currentTime().toString("hh:mm:ss.zzz")));
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
    
    try {
        parseReport(getReportPath(ReportType::XML));
        emit analysisFinished(true);
    } catch (const std::exception& e) {
        qWarning() << "Caught exception after extraction:" << e.what();
        emit analysisFinished(false);
    }
}

void QCToolsManager::startMkvGeneration() {
    if (m_filePath.isEmpty()) {
        emit analysisFinished(true); 
        return; 
    }
    emit logMessage(QString("[%1] Bắt đầu Giai đoạn 2 (tạo .mkv ở chế độ nền)...").arg(QTime::currentTime().toString("hh:mm:ss.zzz")));
    m_backgroundProcess = new QProcess(this);
    connect(m_backgroundProcess, &QProcess::finished, this, &QCToolsManager::onMkvGenerationFinished);
    QStringList args; args << "-i" << m_filePath << "-o" << getReportPath(ReportType::MKV) << "-y";
    emit logMessage(QString("   - Lệnh: %1 %2").arg(m_qcliPath).arg(args.join(" ")));
    m_backgroundProcess->start(m_qcliPath, args);
}

void QCToolsManager::onMkvGenerationFinished(int exitCode, QProcess::ExitStatus exitStatus) {
    if (m_stopRequested) {
        emit logMessage(QString("[%1] Giai đoạn 2 (tạo .mkv) đã được người dùng dừng lại.").arg(QTime::currentTime().toString("hh:mm:ss.zzz")));
        emit analysisFinished(false);
        return;
    }

    QString msg;
    bool success = (exitCode == 0);
    if (success) {
        msg = QString("Đã tạo thành công báo cáo '%1'").arg(QFileInfo(getReportPath(ReportType::MKV)).fileName());
        emit logMessage(QString("[%1] Hoàn tất Giai đoạn 2. Đã tạo file .mkv thành công.").arg(QTime::currentTime().toString("hh:mm:ss.zzz")));
    } else {
        msg = "Lỗi: Không thể tạo báo cáo .mkv";
        emit logMessage(QString("[%1] Giai đoạn 2 (tạo .mkv) thất bại.").arg(QTime::currentTime().toString("hh:mm:ss.zzz")));
    }
    emit backgroundTaskFinished(msg);
    emit analysisFinished(success);
}

void QCToolsManager::readAnalysisOutput() {
    QProcess *source = qobject_cast<QProcess*>(sender());
    if (!source) return;
    m_processBuffer.append(source->readAll());
    
    if (!m_isGeneratingReport && m_processBuffer.contains("generating QCTools report")) {
        m_isGeneratingReport = true;
    }

    QRegularExpression re("(\\d+)\\s+of\\s+(\\d+)\\s+\\(\\s*[\\d\\.]+\\s*%\\)");
    QRegularExpressionMatchIterator i = re.globalMatch(m_processBuffer);
    QRegularExpressionMatch lastMatch;
    while (i.hasNext()) { lastMatch = i.next(); }
    
    if (lastMatch.hasMatch()) {
        int currentFrame = lastMatch.captured(1).toInt();
        int totalFrames = lastMatch.captured(2).toInt();
        
        if (m_totalFramesFromLog == 0 && totalFrames > 0 && !m_isGeneratingReport) {
            m_totalFramesFromLog = totalFrames;
        }
        if (totalFrames > 0) {
            QString phase = m_isGeneratingReport ? "Đang Tạo Báo cáo" : "Đang Phân tích Video";
            emit statusUpdated(QString("%1: %2 / %3").arg(phase).arg(currentFrame).arg(totalFrames));
            emit progressUpdated(currentFrame, totalFrames);
        }
    }
}

void QCToolsManager::parseReport(const QString &reportPath) {
    emit logMessage(QString("[%1] Bắt đầu phân tích file báo cáo: %2").arg(QTime::currentTime().toString("hh:mm:ss.zzz")).arg(QDir::toNativeSeparators(reportPath)));
    QFile reportFile(reportPath); 
    if (!reportFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        emit errorOccurred("Không thể mở file báo cáo XML."); 
        throw std::runtime_error("Cannot open XML report file.");
    }

    QXmlStreamReader xml(&reportFile);
    QList<FrameData> allFramesData;

    emit logMessage(QString("[%1]   - Đang đọc file XML...").arg(QTime::currentTime().toString("hh:mm:ss.zzz")));

    while (!xml.atEnd()) {
        xml.readNext();
        if (xml.isStartElement()) {
            if (xml.name() == QLatin1String("stream") && xml.attributes().value("codec_type") == QLatin1String("video")) {
                if (m_videoWidth == 0) {
                    const auto& attrs = xml.attributes();
                    QStringList parts = attrs.value("r_frame_rate").toString().split('/');
                    if (parts.size() == 2 && parts[1].toDouble() != 0) m_fps = parts[0].toDouble() / parts[1].toDouble();
                    m_videoWidth = attrs.value("width").toInt();
                    m_videoHeight = attrs.value("height").toInt();
                    if (attrs.hasAttribute("nb_frames")) m_totalFrames = attrs.value("nb_frames").toInt();
                }
            } else if (xml.name() == QLatin1String("frame")) {
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
    }

    if (xml.hasError()) {
        QString errorMsg = QString("Lỗi phân tích cú pháp XML: %1 (Dòng %2, Cột %3)").arg(xml.errorString()).arg(xml.lineNumber()).arg(xml.columnNumber());
        emit errorOccurred(errorMsg);
        reportFile.close();
        throw std::runtime_error(errorMsg.toStdString());
    }

    reportFile.close();

    if (m_totalFramesFromLog > 0) m_totalFrames = m_totalFramesFromLog;

    if (m_videoWidth <= 0 || m_videoHeight <= 0) {
        QString errorMsg = QString("Lỗi: Đã đọc xong file XML nhưng không tìm thấy thông tin video stream hợp lệ (width/height=%1x%2).").arg(m_videoWidth).arg(m_videoHeight);
        emit errorOccurred(errorMsg);
        throw std::runtime_error(errorMsg.toStdString());
    }
    if (allFramesData.isEmpty()) {
        QString errorMsg = QString("Lỗi: Đã đọc xong file XML nhưng không tìm thấy dữ liệu của bất kỳ frame nào.");
        emit errorOccurred(errorMsg);
        throw std::runtime_error(errorMsg.toStdString());
    }
     if (m_fps <= 0) {
        emit logMessage("      -> CẢNH BÁO: Không tìm thấy FPS, sử dụng giá trị mặc định 30.0");
        m_fps = 30.0;
    }
    
    emit logMessage(QString("[%1]     -> Đã đọc xong. Tìm thấy %2 frame. Bắt đầu tổng hợp lỗi...").arg(QTime::currentTime().toString("hh:mm:ss.zzz")).arg(allFramesData.count()));
    
    if (m_totalFrames <= 0) {
        m_totalFrames = allFramesData.size();
    }

    QList<AnalysisResult> finalResults;
    const double blackFrameThresh = m_settings.value("blackFrameThreshold", 17.0).toDouble();
    int orphanThresh = m_settings.value("orphanThreshold", 5).toInt();
    double sceneThresh = m_settings.value("sceneThreshold", 30.0).toDouble();

    QSet<int> blackFrameIndices;

    if (m_settings.value("detectBlackFrames", true).toBool()) {
        QList<FrameData> currentGroup;
        for(const auto& frame : allFramesData) {
            if (frame.yavg < blackFrameThresh) { currentGroup.append(frame); }
            else {
                if (!currentGroup.isEmpty()) {
                    double yavgSum = 0;
                    for(const auto& f : currentGroup) { yavgSum += f.yavg; blackFrameIndices.insert(f.frameNum); }
                    finalResults.append({ frameToTimecodeHHMMSSFF(currentGroup.first().frameNum, m_fps), QString::number(currentGroup.count()), "Frame Đen", QString("Frame tối (YAVG TB: %1)").arg(yavgSum / currentGroup.count(), 0, 'f', 2), currentGroup.first().frameNum });
                    currentGroup.clear();
                }
            }
        }
        if (!currentGroup.isEmpty()) {
            double yavgSum = 0;
            for(const auto& f : currentGroup) { yavgSum += f.yavg; blackFrameIndices.insert(f.frameNum); }
            finalResults.append({ frameToTimecodeHHMMSSFF(currentGroup.first().frameNum, m_fps), QString::number(currentGroup.count()), "Frame Đen", QString("Frame tối (YAVG TB: %1)").arg(yavgSum / currentGroup.count(), 0, 'f', 2), currentGroup.first().frameNum });
        }
    }
    
    emit logMessage(QString("[%1]       - Đã xử lý Frame Đen, tìm thấy %2 frame bị đánh dấu.").arg(QTime::currentTime().toString("hh:mm:ss.zzz")).arg(blackFrameIndices.count()));

    if (m_settings.value("detectBlackBorders", true).toBool()) {
        QList<BorderGroup> borderGroups;
        std::optional<BorderGroup> currentGroupOpt;

        for (const auto& frame : allFramesData) {
            if (blackFrameIndices.contains(frame.frameNum)) continue;
            
            if (frame.crop_w < 0) {
                if (currentGroupOpt.has_value()) {
                    borderGroups.append(currentGroupOpt.value());
                    currentGroupOpt.reset();
                }
                continue;
            }

            CropValues cv = CropValues::fromFrameData(frame, m_videoWidth, m_videoHeight);
            bool isBordered = cv.isValid() && cv.hasBorders();

            if (isBordered) {
                if (!currentGroupOpt.has_value()) {
                    currentGroupOpt.emplace(BorderGroup{
                        frame.frameNum,
                        frame.frameNum,
                        1,
                        cv, 
                        cv
                    });
                } else {
                    BorderGroup& currentGroup = currentGroupOpt.value();
                    currentGroup.endFrame = frame.frameNum;
                    currentGroup.count++;
                    currentGroup.minCv.top = std::min(currentGroup.minCv.top, cv.top);
                    currentGroup.maxCv.top = std::max(currentGroup.maxCv.top, cv.top);
                    currentGroup.minCv.bottom = std::min(currentGroup.minCv.bottom, cv.bottom);
                    currentGroup.maxCv.bottom = std::max(currentGroup.maxCv.bottom, cv.bottom);
                    currentGroup.minCv.left = std::min(currentGroup.minCv.left, cv.left);
                    currentGroup.maxCv.left = std::max(currentGroup.maxCv.left, cv.left);
                    currentGroup.minCv.right = std::min(currentGroup.minCv.right, cv.right);
                    currentGroup.maxCv.right = std::max(currentGroup.maxCv.right, cv.right);
                }
            } else {
                if (currentGroupOpt.has_value()) {
                    borderGroups.append(currentGroupOpt.value());
                    currentGroupOpt.reset();
                }
            }
        }
        if (currentGroupOpt.has_value()) {
            borderGroups.append(currentGroupOpt.value());
        }

        for(const auto& group : borderGroups) {
            finalResults.append({ 
                frameToTimecodeHHMMSSFF(group.startFrame, m_fps), 
                QString::number(group.count), 
                "Viền Đen", 
                formatCropDetails(group.minCv, group.maxCv, m_videoWidth, m_videoHeight), 
                group.startFrame 
            });
        }
    }

    if (m_settings.value("detectOrphanFrames", true).toBool()) {
        QList<int> scene_cuts; scene_cuts.append(0);
        for (int i = 1; i < allFramesData.size(); ++i) {
            if (blackFrameIndices.contains(allFramesData[i].frameNum)) continue;
            if (allFramesData[i].ydif > sceneThresh) {
                scene_cuts.append(allFramesData[i].frameNum);
            }
        }
        if (m_totalFrames > 0 && !scene_cuts.contains(m_totalFrames)) scene_cuts.append(m_totalFrames);
        for (int i = 0; i < scene_cuts.size() - 1; ++i) {
            int startFrame = scene_cuts[i]; int endFrame = scene_cuts[i+1];
            int duration = endFrame - startFrame;
            if (duration > 0 && duration <= orphanThresh) {
                finalResults.append({ frameToTimecodeHHMMSSFF(startFrame, m_fps), QString::number(duration), "Frame Dư", QString("Cảnh ngắn bất thường, từ frame %1>%2").arg(startFrame).arg(endFrame - 1), startFrame });
            }
        }
    }

    if (!finalResults.isEmpty()) {
        emit logMessage(QString("[%1] Tổng hợp xong. Tìm thấy %2 lỗi.").arg(QTime::currentTime().toString("hh:mm:ss.zzz")).arg(finalResults.count()));
        emit resultsReady(finalResults);
    } else {
        emit logMessage(QString("[%1] Tổng hợp xong. Không tìm thấy lỗi nào với cấu hình hiện tại.").arg(QTime::currentTime().toString("hh:mm:ss.zzz")));
        emit resultsReady({});
    }
}

QString QCToolsManager::createReportDirectory() {
    if (m_filePath.isEmpty()) return QString();
    QFileInfo fileInfo(m_filePath);
    QString dirName = fileInfo.absolutePath() + "/" + fileInfo.completeBaseName() + "_QCTools_Reports";
    QDir dir(dirName);
    if (!dir.exists()) { if (!dir.mkpath(".")) return QString(); }
    return dirName;
}

