// src/qctools/QCToolsController.h
#ifndef QCTOOLSCONTROLLER_H
#define QCTOOLSCONTROLLER_H

#include <QObject>
#include <QProcess> // Giữ lại để Q_OBJECT có thể xử lý tín hiệu
#include <memory>   // CẢI TIẾN: Thêm thư viện con trỏ thông minh

class QCToolsController : public QObject
{
    Q_OBJECT

public:
    explicit QCToolsController(QObject *parent = nullptr);

    // CẢI TIẾN: Thay đổi chữ ký hàm để nhận cả hai đường dẫn
    void updatePaths(const QString &exePath, const QString &cliPath);
    QString getQCToolsExePath() const;

signals:
    void controllerError(const QString &message);

public slots:
    // CẢI TIẾN: Hàm này giờ chỉ cần file video, vì đường dẫn đã được lưu
    void startAndOpenFile(const QString &filePath = QString());

private:
    // CẢI TIẾN: Lưu trữ cả hai đường dẫn quan trọng
    QString m_exePath;
    QString m_cliPath;
};

#endif // QCTOOLSCONTROLLER_H

