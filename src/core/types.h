// src/core/types.h
#ifndef TYPES_H
#define TYPES_H

#include <QString>
#include <QMetaType>
#include <QVariantMap>
#include <atomic>

struct AnalysisResult {
    QString timecode;
    QString duration;
    QString errorType;
    QString details;
    int startFrame = 0;

    // CẢI TIẾN: Bộ đếm ID cho kết quả, đảm bảo ID là duy nhất trong một lần chạy
    static inline std::atomic<int> nextId = 0;
    int id = nextId++;

    // CẢI TIẾN: Hàm tĩnh để reset bộ đếm ID mỗi khi bắt đầu một phiên phân tích mới
    static void resetIdCounter() {
        nextId = 0;
    }
};
Q_DECLARE_METATYPE(AnalysisResult)

#endif // TYPES_H
