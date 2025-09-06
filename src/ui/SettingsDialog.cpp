// src/ui/settingsdialog.cpp
#include "settingsdialog.h"
#include "core/Constants.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QTabWidget>
#include <QLabel>
#include <QDialogButtonBox>
#include <QPushButton>
#include <QSpinBox>
#include <QCheckBox>
#include <QComboBox>
#include <QLineEdit>     // CẢI TIẾN: Thêm header
#include <QFileDialog>   // CẢI TIẾN: Thêm header
#include <QSettings>
#include <QDirIterator>
#include <QMessageBox>
#include <QDesktopServices>
#include <QUrl>

const QString APP_VERSION = "7.0";

SettingsDialog::SettingsDialog(QWidget *parent)
    : QDialog(parent)
{
    setupUI();
    loadSettings();
}

void SettingsDialog::setCacheDirectory(const QString &path)
{
    m_cacheDir = path;
    updateCacheSizeLabel();
}

// CẢI TIẾN: Mở trực tiếp tab Đường dẫn khi cần
void SettingsDialog::openPathsTab()
{
    // Giả sử tab Đường dẫn là tab thứ 0
    if(m_tabWidget) m_tabWidget->setCurrentIndex(0);
}


void SettingsDialog::setupUI()
{
    setWindowTitle("Cài đặt Nâng cao");
    setMinimumWidth(550);

    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    m_tabWidget = new QTabWidget();

    // CẢI TIẾN: Thêm tab Đường dẫn
    // --- Paths Tab ---
    QWidget *pathsTab = new QWidget();
    QFormLayout *pathsLayout = new QFormLayout(pathsTab);
    pathsLayout->setSpacing(10);

    m_qctoolsPathEdit = new QLineEdit(this);
    m_qctoolsPathEdit->setPlaceholderText("Chưa đặt đường dẫn");
    QPushButton *browseQCToolsButton = new QPushButton("Duyệt...");
    QHBoxLayout *qctoolsLayout = new QHBoxLayout();
    qctoolsLayout->addWidget(m_qctoolsPathEdit);
    qctoolsLayout->addWidget(browseQCToolsButton);
    pathsLayout->addRow("Đường dẫn QCTools.exe:", qctoolsLayout);

    m_qcliPathEdit = new QLineEdit(this);
    m_qcliPathEdit->setPlaceholderText("Chưa đặt đường dẫn");
    QPushButton *browseQCCliButton = new QPushButton("Duyệt...");
    QHBoxLayout *qcliLayout = new QHBoxLayout();
    qcliLayout->addWidget(m_qcliPathEdit);
    qcliLayout->addWidget(browseQCCliButton);
    pathsLayout->addRow("Đường dẫn qcli.exe:", qcliLayout);

    connect(browseQCToolsButton, &QPushButton::clicked, this, &SettingsDialog::onBrowseQCTools);
    connect(browseQCCliButton, &QPushButton::clicked, this, &SettingsDialog::onBrowseQCCli);

    // --- About Tab ---
    QWidget *aboutTab = new QWidget();
    QVBoxLayout *aboutLayout = new QVBoxLayout(aboutTab);
    QLabel *aboutLabel = new QLabel(
        QString("<b>Video QC Tool - Control Panel v%1</b><br><br>"
                "Bảng điều khiển này được thiết kế để hoạt động cùng với <b>QCTools</b>.<br>"
                "Nó sẽ ra lệnh cho QCTools thực hiện phân tích và điều khiển trình phát video từ xa.<br><br>"
                "<b>Yêu cầu:</b> QCTools (v1.4.2 hoặc tương thích) phải được cài đặt trên hệ thống."
               ).arg(APP_VERSION)
    );
    aboutLabel->setWordWrap(true);
    aboutLayout->addWidget(aboutLabel);
    aboutLayout->addStretch();
    
    // --- Preview Tab ---
    QWidget *previewTab = new QWidget();
    QFormLayout *previewLayout = new QFormLayout(previewTab);
    m_rewindFramesSpinBox = new QSpinBox(this);
    m_rewindFramesSpinBox->setRange(0, 100);
    QLabel* rewindLabel = new QLabel("Lùi lại khi double-click (frames):");
    rewindLabel->setToolTip("Tính năng này đã bị loại bỏ. Việc double-click giờ đây sẽ sao chép timecode.");
    m_rewindFramesSpinBox->setEnabled(false);
    previewLayout->addRow(rewindLabel, m_rewindFramesSpinBox);

    // --- Hardware Tab ---
    QWidget *hwTab = new QWidget();
    QFormLayout *hwLayout = new QFormLayout(hwTab);
    m_hwAccelCheck = new QCheckBox("Sử dụng tăng tốc phần cứng", this);
    m_hwAccelCheck->setToolTip("Yêu cầu qcli.exe của QCTools hỗ trợ tham số -hwaccel.\nSử dụng GPU để tăng tốc quá trình phân tích.");
    m_hwAccelTypeCombo = new QComboBox(this);
    m_hwAccelTypeCombo->addItems({"auto", "cuda", "qsv", "dxva2", "d3d11va", "amf", "opencl"});
    m_hwAccelTypeCombo->setToolTip(
        "Chọn phương pháp giải mã phù hợp với GPU của bạn.\n\n"
        "- **NVIDIA:** Chọn 'cuda' (ưu tiên) hoặc 'dxva2'/'d3d11va'.\n"
        "- **Intel:** Chọn 'qsv' (ưu tiên) hoặc 'dxva2'/'d3d11va'.\n"
        "- **AMD:** Chọn 'amf' (ưu tiên) hoặc 'dxva2'/'d3d11va'.\n"
        "- **auto:** Để qcli tự động chọn."
    );
    hwLayout->addRow(m_hwAccelCheck);
    hwLayout->addRow("Phương pháp giải mã:", m_hwAccelTypeCombo);
    connect(m_hwAccelCheck, &QCheckBox::toggled, m_hwAccelTypeCombo, &QComboBox::setEnabled);

    // --- Cache Tab ---
    QWidget *cacheTab = new QWidget();
    QVBoxLayout *cacheLayout = new QVBoxLayout(cacheTab);
    QLabel *cacheInfoLabel = new QLabel("Chương trình lưu trữ kết quả phân tích (dưới dạng file .xml) để tăng tốc độ xem lại báo cáo. Bạn có thể xóa các file này nếu chúng chiếm quá nhiều dung lượng.");
    cacheInfoLabel->setWordWrap(true);
    
    m_cacheSizeLabel = new QLabel("Dung lượng cache hiện tại: <b>Đang tính toán...</b>");
    m_clearCacheButton = new QPushButton("Xóa toàn bộ Cache");
    m_clearCacheButton->setToolTip("Xóa tất cả các file báo cáo đã được lưu trữ tạm.");
    
    m_openCacheButton = new QPushButton("Mở thư mục Cache");
    m_openCacheButton->setToolTip("Mở thư mục chứa các file cache trong File Explorer.");
    
    QHBoxLayout *buttonCacheLayout = new QHBoxLayout();
    buttonCacheLayout->addStretch();
    buttonCacheLayout->addWidget(m_openCacheButton);
    buttonCacheLayout->addWidget(m_clearCacheButton);

    cacheLayout->addWidget(cacheInfoLabel);
    cacheLayout->addSpacing(15);
    cacheLayout->addWidget(m_cacheSizeLabel);
    cacheLayout->addLayout(buttonCacheLayout);
    cacheLayout->addStretch();
    connect(m_clearCacheButton, &QPushButton::clicked, this, &SettingsDialog::onClearCacheClicked);
    connect(m_openCacheButton, &QPushButton::clicked, this, &SettingsDialog::onOpenCacheFolderClicked);

    m_tabWidget->addTab(pathsTab, "Đường dẫn"); // Thêm tab mới
    m_tabWidget->addTab(hwTab, "Tăng tốc P.cứng");
    m_tabWidget->addTab(cacheTab, "Cache");
    m_tabWidget->addTab(previewTab, "Preview");
    m_tabWidget->addTab(aboutTab, "Giới thiệu");
    

    QDialogButtonBox *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    connect(buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(this, &QDialog::accepted, this, &SettingsDialog::saveSettings);

    mainLayout->addWidget(m_tabWidget);
    mainLayout->addWidget(buttonBox);
}

void SettingsDialog::loadSettings()
{
    QSettings settings(AppConstants::ORG_NAME, AppConstants::APP_NAME);
    m_qctoolsPathEdit->setText(settings.value(AppConstants::K_QCTOOLS_PATH, "").toString());
    m_qcliPathEdit->setText(settings.value(AppConstants::K_QCCLI_PATH, "").toString());

    m_hwAccelCheck->setChecked(settings.value(AppConstants::K_USE_HW_ACCEL, false).toBool());
    m_hwAccelTypeCombo->setCurrentText(settings.value(AppConstants::K_HW_ACCEL_TYPE, "auto").toString());
    m_hwAccelTypeCombo->setEnabled(m_hwAccelCheck->isChecked());
    
    m_rewindFramesSpinBox->setValue(settings.value("rewindFrames", 1).toInt());
}

void SettingsDialog::saveSettings()
{
    QSettings settings(AppConstants::ORG_NAME, AppConstants::APP_NAME);
    settings.setValue(AppConstants::K_QCTOOLS_PATH, m_qctoolsPathEdit->text());
    settings.setValue(AppConstants::K_QCCLI_PATH, m_qcliPathEdit->text());

    settings.setValue(AppConstants::K_USE_HW_ACCEL, m_hwAccelCheck->isChecked());
    settings.setValue(AppConstants::K_HW_ACCEL_TYPE, m_hwAccelTypeCombo->currentText());
    settings.setValue("rewindFrames", m_rewindFramesSpinBox->value());
}

QVariantMap SettingsDialog::getSettings() const
{
    QVariantMap settings;
    settings[AppConstants::K_QCTOOLS_PATH] = m_qcliPathEdit->text(); // Pass QCLI path for analysis
    settings[AppConstants::K_USE_HW_ACCEL] = m_hwAccelCheck->isChecked();
    settings[AppConstants::K_HW_ACCEL_TYPE] = m_hwAccelTypeCombo->currentText();
    return settings;
}

void SettingsDialog::onBrowseQCTools()
{
    QString filePath = QFileDialog::getOpenFileName(this, "Chọn file QCTools.exe", "C:/Program Files/", "Executable (*.exe)");
    if(!filePath.isEmpty()) {
        m_qctoolsPathEdit->setText(QDir::toNativeSeparators(filePath));
    }
}

void SettingsDialog::onBrowseQCCli()
{
    QString filePath = QFileDialog::getOpenFileName(this, "Chọn file qcli.exe", "C:/Program Files/", "Executable (*.exe)");
    if(!filePath.isEmpty()) {
        m_qcliPathEdit->setText(QDir::toNativeSeparators(filePath));
    }
}


void SettingsDialog::onClearCacheClicked()
{
    auto reply = QMessageBox::question(this, "Xác nhận Xóa Cache",
                                       "Bạn có chắc chắn muốn xóa toàn bộ cache không?\n"
                                       "Hành động này không thể hoàn tác.",
                                       QMessageBox::Yes | QMessageBox::No,
                                       QMessageBox::No);

    if (reply == QMessageBox::Yes) {
        QDir cacheDir(m_cacheDir);
        if (cacheDir.removeRecursively()) {
            cacheDir.mkpath(".");
            QMessageBox::information(this, "Thành công", "Đã xóa cache thành công.");
        } else {
            QMessageBox::critical(this, "Lỗi", "Không thể xóa thư mục cache.");
        }
        updateCacheSizeLabel();
    }
}

void SettingsDialog::onOpenCacheFolderClicked()
{
    if (!m_cacheDir.isEmpty() && QDir(m_cacheDir).exists()) {
        QDesktopServices::openUrl(QUrl::fromLocalFile(m_cacheDir));
    } else {
        QMessageBox::warning(this, "Lỗi", "Không thể tìm thấy thư mục cache hoặc đường dẫn chưa được thiết lập.");
    }
}


void SettingsDialog::updateCacheSizeLabel()
{
    if (m_cacheDir.isEmpty()) {
        m_cacheSizeLabel->setText("Dung lượng cache hiện tại: <b>Chưa xác định</b>");
        return;
    }
    
    qint64 totalSize = calculateCacheSize();
    QString sizeStr;
    if (totalSize < 1024) {
        sizeStr = QString::number(totalSize) + " B";
    } else if (totalSize < 1024 * 1024) {
        sizeStr = QString::number(totalSize / 1024.0, 'f', 2) + " KB";
    } else if (totalSize < 1024 * 1024 * 1024) {
        sizeStr = QString::number(totalSize / (1024.0 * 1024.0), 'f', 2) + " MB";
    } else {
        sizeStr = QString::number(totalSize / (1024.0 * 1024.0 * 1024.0), 'f', 2) + " GB";
    }

    m_cacheSizeLabel->setText(QString("Dung lượng cache hiện tại: <b>%1</b>").arg(sizeStr));
}

qint64 SettingsDialog::calculateCacheSize() const
{
    qint64 totalSize = 0;
    QDirIterator it(m_cacheDir, QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        it.next();
        totalSize += it.fileInfo().size();
    }
    return totalSize;
}

