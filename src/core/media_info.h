// src/core/media_info.h
#ifndef MEDIA_INFO_H
#define MEDIA_INFO_H

#include <QString>
#include <QDateTime>
#include <QMetaType>
#include <QLocale>

struct MediaInfo {
    // General
    QString formatName;
    double duration = 0.0;
    qint64 size = 0;
    qint64 bitrate = 0;
    QDateTime creationTime;

    // Video Stream
    int width = 0;
    int height = 0;
    double fps = 0.0;
    QString videoCodec;
    QString pixelFormat;
    QString colorSpace;

    // Audio Stream
    QString audioCodec;
    int sampleRate = 0;
    QString channelLayout;
    
    // Helper to format the info for logging
    QString toFormattedString() const {
        QStringList info;
        QLocale loc(QLocale::English);

        info << QString("  - Định dạng File: %1").arg(formatName.isEmpty() ? "N/A" : formatName);
        if (duration > 0) {
             QTime t(0,0,0);
             t = t.addSecs(static_cast<int>(duration));
             info << QString("  - Thời lượng: %1 (%2 giây)").arg(t.toString("HH:mm:ss")).arg(duration, 0, 'f', 3);
        }
        if (size > 0) info << QString("  - Dung lượng: %1 MB").arg(loc.toString(size / (1024.0 * 1024.0), 'f', 2));
        if (bitrate > 0) info << QString("  - Bitrate Tổng: %1 kb/s").arg(loc.toString(bitrate / 1000));
        
        info << "";
        info << "  --- Video Stream ---";
        if (width > 0 && height > 0) info << QString("  - Độ phân giải: %1x%2").arg(width).arg(height);
        if (fps > 0) info << QString("  - Tốc độ Khung hình (FPS): %1").arg(fps, 0, 'f', 3);
        info << QString("  - Codec: %1").arg(videoCodec.isEmpty() ? "N/A" : videoCodec);
        info << QString("  - Định dạng Pixel: %1").arg(pixelFormat.isEmpty() ? "N/A" : pixelFormat);
        info << QString("  - Không gian màu: %1").arg(colorSpace.isEmpty() ? "N/A" : colorSpace);

        info << "";
        info << "  --- Audio Stream ---";
        info << QString("  - Codec: %1").arg(audioCodec.isEmpty() ? "N/A" : audioCodec);
        if (sampleRate > 0) info << QString("  - Tần số lấy mẫu: %1 Hz").arg(loc.toString(sampleRate));
        info << QString("  - Cấu hình Kênh: %1").arg(channelLayout.isEmpty() ? "N/A" : channelLayout);

        return info.join("\n");
    }
};

// Required for signal/slot connections
Q_DECLARE_METATYPE(MediaInfo)

#endif // MEDIA_INFO_H
