// src/qctools/QCToolsController.cpp
#include "QCToolsController.h"
#include <QStandardPaths>
#include <QFileInfo>
#include <QDebug>
#include <QSettings>
#include <QProcess> // Thêm QProcess để sử dụng startDetached
#include "core/Constants.h"

QCToolsController::QCToolsController(QObject *parent)
    : QObject(parent)
{
    // Load saved paths on startup
    QSettings settings(AppConstants::ORG_NAME, AppConstants::APP_NAME);
    m_exePath = settings.value(AppConstants::K_QCTOOLS_PATH).toString();
    m_cliPath = settings.value(AppConstants::K_QCCLI_PATH).toString();
}

void QCToolsController::updatePaths(const QString &exePath, const QString &cliPath)
{
    m_exePath = exePath;
    m_cliPath = cliPath;
    // Save to settings immediately
    QSettings settings(AppConstants::ORG_NAME, AppConstants::APP_NAME);
    settings.setValue(AppConstants::K_QCTOOLS_PATH, m_exePath);
    settings.setValue(AppConstants::K_QCCLI_PATH, m_cliPath);
}

QString QCToolsController::getQCToolsExePath() const
{
    return m_exePath;
}


void QCToolsController::startAndOpenFile(const QString &filePath)
{
    if (m_exePath.isEmpty() || !QFileInfo::exists(m_exePath)) {
        emit controllerError("Đường dẫn tới QCTools.exe chưa được thiết lập hoặc không hợp lệ.");
        return;
    }

    QStringList args;
    if (!filePath.isEmpty()) {
        args << filePath;
    }
    
    // CẢI TIẾN: Sửa lỗi rò rỉ bộ nhớ và quản lý tiến trình
    // Gọi trực tiếp hàm tĩnh startDetached, không cần tạo đối tượng QProcess mới.
    // Điều này loại bỏ hoàn toàn nguy cơ rò rỉ bộ nhớ.
    if (!QProcess::startDetached(m_exePath, args)) {
         emit controllerError(QString("Không thể khởi chạy QCTools.exe.\n"
                                      "Vui lòng kiểm tra lại đường dẫn: %1").arg(m_exePath));
    }
}

