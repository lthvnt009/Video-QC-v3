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
#include <QLineEdit>
#include <QFileDialog>
#include <QSettings>
#include <QMessageBox>
#include <QDesktopServices>
#include <QUrl>

const QString APP_VERSION = "2.1";

SettingsDialog::SettingsDialog(QWidget *parent)
    : QDialog(parent)
{
    setupUI();
    loadSettings();
}

void SettingsDialog::openPathsTab()
{
    if(m_tabWidget) m_tabWidget->setCurrentIndex(0);
}


void SettingsDialog::setupUI()
{
    // VIỆT HÓA: Thay đổi tiêu đề cửa sổ
    setWindowTitle("Cài đặt");
    setMinimumWidth(550);

    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    m_tabWidget = new QTabWidget();

    // --- Paths Tab ---
    QWidget *pathsTab = new QWidget();
    QFormLayout *pathsLayout = new QFormLayout(pathsTab);
    pathsLayout->setSpacing(10);
    pathsLayout->addRow(new QLabel("Lưu ý: qcli.exe phải nằm cùng thư mục với QCTools.exe."));

    m_qctoolsPathEdit = new QLineEdit(this);
    m_qctoolsPathEdit->setPlaceholderText("Chưa đặt đường dẫn");
    QPushButton *browseQCToolsButton = new QPushButton("Duyệt...");
    QHBoxLayout *qctoolsLayout = new QHBoxLayout();
    qctoolsLayout->addWidget(m_qctoolsPathEdit);
    qctoolsLayout->addWidget(browseQCToolsButton);
    pathsLayout->addRow("Đường dẫn QCTools.exe:", qctoolsLayout);

    m_qcliPathEdit = new QLineEdit(this);
    m_qcliPathEdit->setPlaceholderText("Sẽ được tự động điền");
    m_qcliPathEdit->setReadOnly(true);
    QPushButton *browseQCCliButton = new QPushButton("Duyệt...");
    browseQCCliButton->setToolTip("Chỉ sử dụng nếu qcli.exe không nằm cùng thư mục với QCTools.exe");
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
        QString("<b>Video QC Tool v%1</b><br><br>"
                "Bảng điều khiển này được thiết kế để hoạt động cùng với <b>QCTools</b>.<br>"
                "Nó sẽ ra lệnh cho qcli thực hiện phân tích tự động theo các bộ lọc đã chọn.<br><br>"
                "<b>Yêu cầu:</b> QCTools (v1.4.2 hoặc tương thích) phải được cài đặt trên hệ thống."
               ).arg(APP_VERSION)
    );
    aboutLabel->setWordWrap(true);
    aboutLayout->addWidget(aboutLabel);
    aboutLayout->addStretch();

    // --- Interaction Tab ---
    QWidget *interactionTab = new QWidget();
    QFormLayout *interactionLayout = new QFormLayout(interactionTab);
    m_rewindFramesSpinBox = new QSpinBox(this);
    m_rewindFramesSpinBox->setRange(0, 100);
    m_rewindFramesSpinBox->setToolTip("Khi double-click vào một lỗi, timecode được sao chép sẽ bị trừ đi số frame này.");
    interactionLayout->addRow("Lùi lại khi double-click (frames):", m_rewindFramesSpinBox);

    // --- Hardware Tab ---
    QWidget *hwTab = new QWidget();
    QFormLayout *hwLayout = new QFormLayout(hwTab);
    hwLayout->addRow(new QLabel("Tính năng này hiện không được hỗ trợ bởi phiên bản qcli.exe đi kèm."));
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

    m_hwAccelCheck->setEnabled(false);
    m_hwAccelTypeCombo->setEnabled(false);

    m_tabWidget->addTab(pathsTab, "Đường dẫn");
    m_tabWidget->addTab(hwTab, "Tăng tốc P.cứng");
    m_tabWidget->addTab(interactionTab, "Tương tác");
    m_tabWidget->addTab(aboutTab, "Giới thiệu");

    QDialogButtonBox *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    m_resetButton = buttonBox->addButton("Đặt lại về Mặc định", QDialogButtonBox::ResetRole);

    connect(buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(m_resetButton, &QPushButton::clicked, this, &SettingsDialog::onResetToDefaultsClicked);
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

    m_rewindFramesSpinBox->setValue(settings.value(AppConstants::K_REWIND_FRAMES, 5).toInt());
}

void SettingsDialog::saveSettings()
{
    QSettings settings(AppConstants::ORG_NAME, AppConstants::APP_NAME);
    settings.setValue(AppConstants::K_QCTOOLS_PATH, m_qctoolsPathEdit->text());
    settings.setValue(AppConstants::K_QCCLI_PATH, m_qcliPathEdit->text());

    settings.setValue(AppConstants::K_USE_HW_ACCEL, m_hwAccelCheck->isChecked());
    settings.setValue(AppConstants::K_HW_ACCEL_TYPE, m_hwAccelTypeCombo->currentText());
    settings.setValue(AppConstants::K_REWIND_FRAMES, m_rewindFramesSpinBox->value());
}

QVariantMap SettingsDialog::getSettings() const
{
    QVariantMap settings;
    settings[AppConstants::K_QCTOOLS_PATH] = m_qcliPathEdit->text();
    settings[AppConstants::K_USE_HW_ACCEL] = m_hwAccelCheck->isChecked();
    settings[AppConstants::K_HW_ACCEL_TYPE] = m_hwAccelTypeCombo->currentText();
    return settings;
}

void SettingsDialog::onBrowseQCTools()
{
    QString filePath = QFileDialog::getOpenFileName(this, "Chọn file QCTools.exe", "C:/Program Files/", "Executable (*.exe)");
    if(!filePath.isEmpty()) {
        m_qctoolsPathEdit->setText(QDir::toNativeSeparators(filePath));

        QFileInfo qctoolsInfo(filePath);
        QString cliPath = qctoolsInfo.absolutePath() + "/qcli.exe";
        if (QFile::exists(cliPath)) {
            m_qcliPathEdit->setText(QDir::toNativeSeparators(cliPath));
        } else {
            m_qcliPathEdit->clear();
            QMessageBox::warning(this, "Không tìm thấy qcli.exe",
                                 QString("Không tìm thấy file qcli.exe trong cùng thư mục với QCTools.exe.\n\n"
                                         "Đã tìm ở: %1\n\n"
                                         "Vui lòng chọn đường dẫn qcli.exe thủ công.")
                                 .arg(QDir::toNativeSeparators(cliPath)));
        }
    }
}

void SettingsDialog::onBrowseQCCli()
{
    QString filePath = QFileDialog::getOpenFileName(this, "Chọn file qcli.exe", "C:/Program Files/", "Executable (*.exe)");
    if(!filePath.isEmpty()) {
        m_qcliPathEdit->setText(QDir::toNativeSeparators(filePath));
    }
}

void SettingsDialog::onResetToDefaultsClicked()
{
    // Hộp thoại 1: Xác nhận
    QMessageBox::StandardButton reply;
    reply = QMessageBox::question(this, "Xác nhận Đặt lại",
                                  "Bạn có chắc chắn muốn đặt lại TẤT CẢ cài đặt về giá trị mặc định không?\n"
                                  "Hành động này không thể hoàn tác.",
                                  QMessageBox::Yes | QMessageBox::No);
    if (reply == QMessageBox::Yes) {
        QSettings settings(AppConstants::ORG_NAME, AppConstants::APP_NAME);
        settings.clear();
        loadSettings();

        // Hộp thoại 2: Thông báo hoàn tất
        QMessageBox::information(this, "Hoàn tất", "Đã đặt lại cài đặt về mặc định.\nCác thay đổi đã được áp dụng ngay lập tức.");
        
        // Phát tín hiệu SAU KHI các hộp thoại đã hiển thị xong
        // để các thành phần khác cập nhật và hiển thị hộp thoại thứ 3 (nếu cần)
        emit settingsReset();
    }
}

