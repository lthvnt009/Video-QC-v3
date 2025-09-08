// src/ui/configwidget.cpp
#include "ConfigWidget.h"
#include "core/Constants.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QLineEdit>
#include <QCheckBox>
#include <QPushButton>
#include <QLabel>
#include <QDoubleSpinBox>
#include <QSpinBox>
#include <QFileDialog>
#include <QSettings>
#include <QDir>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMessageBox>

ConfigWidget::ConfigWidget(QWidget *parent)
    : QWidget(parent)
{
    setupUI();
    loadSettings();
}

void ConfigWidget::setInputPath(const QString &path)
{
    m_videoPathEdit->setText(QDir::toNativeSeparators(path));
}

void ConfigWidget::reloadSettings()
{
    loadSettings();
}

void ConfigWidget::setupUI()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0,0,0,0);

    // --- Input Group ---
    QGroupBox *fileBox = new QGroupBox("Đầu vào");
    QHBoxLayout *fileLayout = new QHBoxLayout(fileBox);
    m_videoPathEdit = new QLineEdit;
    m_videoPathEdit->setPlaceholderText("Chưa chọn file (hỗ trợ kéo-thả vào cửa sổ)");
    m_videoPathEdit->setReadOnly(true);
    QPushButton *selectFileButton = new QPushButton("Chọn Video");
    QPushButton *selectReportButton = new QPushButton("Nhập Báo Cáo");
    connect(selectFileButton, &QPushButton::clicked, this, &ConfigWidget::onSelectFileClicked);
    connect(selectReportButton, &QPushButton::clicked, this, &ConfigWidget::onSelectReportClicked);
    fileLayout->addWidget(m_videoPathEdit, 1);
    fileLayout->addWidget(selectFileButton);
    fileLayout->addWidget(selectReportButton);

    // --- Configuration Section ---

    // CẢI TIẾN: Tạo một layout riêng cho tiêu đề và các nút preset
    QHBoxLayout *configHeaderLayout = new QHBoxLayout();
    QLabel *configTitleLabel = new QLabel("<b>Cấu hình Phát hiện Lỗi</b>");
    m_loadPresetButton = new QPushButton("Tải Cấu hình");
    m_savePresetButton = new QPushButton("Lưu Cấu hình");
    connect(m_loadPresetButton, &QPushButton::clicked, this, &ConfigWidget::onLoadPresetClicked);
    connect(m_savePresetButton, &QPushButton::clicked, this, &ConfigWidget::onSavePresetClicked);

    configHeaderLayout->addWidget(configTitleLabel);
    configHeaderLayout->addStretch();
    configHeaderLayout->addWidget(m_loadPresetButton);
    configHeaderLayout->addWidget(m_savePresetButton);


    // CẢI TIẾN: GroupBox bây giờ không có tiêu đề, chỉ dùng để tạo khung
    QGroupBox *configBox = new QGroupBox();
    QVBoxLayout *configVLayout = new QVBoxLayout(configBox);
    configVLayout->setContentsMargins(configVLayout->contentsMargins().left(), 12, configVLayout->contentsMargins().right(), configVLayout->contentsMargins().bottom());


    // --- Top Row Layout (Black Frames & Black Borders) ---
    QHBoxLayout *topLayout = new QHBoxLayout();

    // Black Frame Group
    m_blackFrameBox = new QGroupBox("Frame Đen");
    m_blackFrameBox->setCheckable(true);
    QFormLayout *blackFrameLayout = new QFormLayout(m_blackFrameBox);
    m_blackFrameThreshSpinBox = new QDoubleSpinBox();
    m_blackFrameThreshSpinBox->setRange(0.0, 255.0);
    m_blackFrameThreshSpinBox->setDecimals(1);
    m_blackFrameThreshSpinBox->setSingleStep(0.5);
    m_blackFrameThreshSpinBox->setFixedWidth(80);
    QLabel* blackFrameLabel = new QLabel("Ngưỡng (YAVG) <");
    blackFrameLabel->setToolTip(
        "Một frame có độ sáng trung bình (YAVG) thấp hơn giá trị này sẽ bị coi là 'Frame Đen'.\n\n"
        "Gợi ý:\n"
        "- 16-20: Hợp lý cho hầu hết các video.\n"
        "- < 16: Chỉ phát hiện các frame gần như đen hoàn toàn.\n"
        "- > 20: Nhạy hơn, có thể phát hiện các cảnh rất tối nhưng dễ báo nhầm.");
    m_blackFrameBox->setToolTip(blackFrameLabel->toolTip());
    m_blackFrameThreshSpinBox->setToolTip(
        "Nhập giá trị độ sáng trung bình (YAVG) từ 0 đến 255.\n"
        "Các frame có YAVG thấp hơn giá trị này sẽ bị gắn cờ là 'Frame Đen'.");
    blackFrameLayout->addRow(blackFrameLabel, m_blackFrameThreshSpinBox);
    topLayout->addWidget(m_blackFrameBox);
    connect(m_blackFrameBox, &QGroupBox::toggled, m_blackFrameThreshSpinBox, &QWidget::setEnabled);

    // Black Border Group
    m_blackBorderBox = new QGroupBox("Viền Đen");
    m_blackBorderBox->setCheckable(true);
    QFormLayout *blackBorderLayout = new QFormLayout(m_blackBorderBox);
    m_borderThreshSpinBox = new QDoubleSpinBox();
    m_borderThreshSpinBox->setRange(0.0, 100.0);
    m_borderThreshSpinBox->setDecimals(2);
    m_borderThreshSpinBox->setSuffix(" %");
    m_borderThreshSpinBox->setSingleStep(0.1);
    m_borderThreshSpinBox->setFixedWidth(80);
    QLabel* borderLabel = new QLabel("Ngưỡng >= ");
    borderLabel->setToolTip(
        "Một frame sẽ bị coi là có lỗi nếu có ít nhất một cạnh (trên, dưới, trái, phải) có viền đen lớn hơn tỉ lệ % này so với chiều cao/rộng của video.\n\n"
        "Gợi ý:\n"
        "- 0.1% - 0.5%: Rất nhạy, phát hiện các viền mỏng.\n"
        "- 1% - 5%: Mức độ thông thường.\n"
        "- > 5%: Chỉ phát hiện các viền rất dày.");
    m_blackBorderBox->setToolTip(borderLabel->toolTip());
    m_borderThreshSpinBox->setToolTip(
        "Nhập tỉ lệ % tối thiểu của một cạnh (trên, dưới, trái, phải) so với kích thước video.\n"
        "Nếu một cạnh có viền đen lớn hơn hoặc bằng tỉ lệ này, frame sẽ bị gắn cờ 'Viền Đen'.");
    blackBorderLayout->addRow(borderLabel, m_borderThreshSpinBox);
    topLayout->addWidget(m_blackBorderBox);
    connect(m_blackBorderBox, &QGroupBox::toggled, m_borderThreshSpinBox, &QWidget::setEnabled);

    // --- Bottom Row Layout (Orphan Frames) ---
    m_orphanFrameBox = new QGroupBox("Frame Dư");
    m_orphanFrameBox->setCheckable(true);
    m_orphanFrameBox->setToolTip(
        "Phát hiện các cảnh ngắn bất thường, thường là kết quả của lỗi cắt ghép video."
    );
    QGridLayout *orphanFrameLayout = new QGridLayout(m_orphanFrameBox);
    
    m_hasTransitionsCheck = new QCheckBox("Chuyển cảnh");
    m_hasTransitionsCheck->setToolTip(
        "Kích hoạt nếu video có sử dụng các hiệu ứng chuyển cảnh (mờ dần, hòa tan,...).\n"
        "Khi được kích hoạt, chương trình sẽ sử dụng một thuật toán phân tích thông minh hơn, có khả năng phân biệt giữa hiệu ứng và lỗi thật, giúp tránh báo lỗi nhầm.");

    QLabel* orphanLabel = new QLabel("Ngưỡng Cảnh ngắn <=");
    orphanLabel->setToolTip(
        "Một cảnh có thời lượng (tính bằng số frame) nhỏ hơn hoặc bằng giá trị này sẽ bị coi là 'Frame Dư'.\n\n"
        "Gợi ý:\n"
        "- 1-3: Chỉ phát hiện các cảnh cực ngắn, thường là lỗi rõ ràng.\n"
        "- 4-7: Mức độ thông thường.\n"
        "- > 7: Nhạy hơn, nhưng có thể báo nhầm.");
    m_orphanFrameThreshSpinBox = new QSpinBox();
    m_orphanFrameThreshSpinBox->setRange(1, 999);
    m_orphanFrameThreshSpinBox->setFixedWidth(80);
    m_orphanFrameThreshSpinBox->setToolTip(
        "Nhập số frame tối đa cho một cảnh bị coi là ngắn bất thường.\n"
        "Ví dụ: Nếu đặt là 5, mọi cảnh có từ 1 đến 5 frame sẽ bị gắn cờ là 'Frame Dư'.");


    orphanFrameLayout->addWidget(orphanLabel, 0, 0);
    orphanFrameLayout->addWidget(m_orphanFrameThreshSpinBox, 0, 1);
    orphanFrameLayout->addWidget(m_hasTransitionsCheck, 0, 3);
    orphanFrameLayout->setColumnStretch(2, 1);

    QLabel* sceneLabel = new QLabel("Ngưỡng Cắt Cảnh (YDIF):");
     sceneLabel->setToolTip(
        "Đo lường sự thay đổi độ sáng giữa các frame để xác định điểm cắt cảnh. Giá trị này ảnh hưởng trực tiếp đến việc 'Frame Dư' được phát hiện như thế nào.\n"
        "Chỉ có tác dụng khi KHÔNG chọn 'Chuyển cảnh'.\n\n"
        "Gợi ý:\n"
        "- 20-30: Nhạy, phù hợp cho video có nhiều cảnh cắt nhanh.\n"
        "- 30-50: Mức độ thông thường.\n"
        "- > 50: Ít nhạy.");
    m_sceneDetectThreshSpinBox = new QDoubleSpinBox();
    m_sceneDetectThreshSpinBox->setRange(0.0, 255.0);
    m_sceneDetectThreshSpinBox->setDecimals(1);
    m_sceneDetectThreshSpinBox->setSingleStep(0.5);
    m_sceneDetectThreshSpinBox->setFixedWidth(80);
    m_sceneDetectThreshSpinBox->setToolTip(
        "Nhập giá trị đo lường sự khác biệt độ sáng (YDIF) giữa hai frame liên tiếp.\n"
        "Giá trị cao hơn giúp xác định các điểm cắt cảnh rõ ràng hơn.");
    
    orphanFrameLayout->addWidget(sceneLabel, 1, 0, 1, 1, Qt::AlignLeft);
    orphanFrameLayout->addWidget(m_sceneDetectThreshSpinBox, 1, 1);

    connect(m_orphanFrameBox, &QGroupBox::toggled, this, [this](bool on){
        m_orphanFrameThreshSpinBox->setEnabled(on);
        m_sceneDetectThreshSpinBox->setEnabled(on);
        m_hasTransitionsCheck->setEnabled(on);
    });
    
    connect(m_hasTransitionsCheck, &QCheckBox::toggled, sceneLabel, &QLabel::setDisabled);
    connect(m_hasTransitionsCheck, &QCheckBox::toggled, m_sceneDetectThreshSpinBox, &QDoubleSpinBox::setDisabled);


    // --- Assemble Config Box Content ---
    configVLayout->addLayout(topLayout);
    configVLayout->addWidget(m_orphanFrameBox);

    // --- Assemble Main Layout ---
    mainLayout->addWidget(fileBox);
    mainLayout->addLayout(configHeaderLayout); // Thêm layout tiêu đề
    mainLayout->addWidget(configBox);          // Thêm group box chứa nội dung
}

void ConfigWidget::loadSettings()
{
    QSettings settings(AppConstants::ORG_NAME, AppConstants::APP_NAME);
    QVariantMap settingsMap;
    settingsMap[AppConstants::K_DETECT_BLACK_FRAMES] = settings.value(AppConstants::K_DETECT_BLACK_FRAMES, true);
    settingsMap[AppConstants::K_DETECT_BLACK_BORDERS] = settings.value(AppConstants::K_DETECT_BLACK_BORDERS, true);
    settingsMap[AppConstants::K_DETECT_ORPHAN_FRAMES] = settings.value(AppConstants::K_DETECT_ORPHAN_FRAMES, true);
    settingsMap[AppConstants::K_BORDER_THRESH] = settings.value(AppConstants::K_BORDER_THRESH, 0.2);
    settingsMap[AppConstants::K_ORPHAN_THRESH] = settings.value(AppConstants::K_ORPHAN_THRESH, 5);
    settingsMap[AppConstants::K_BLACK_FRAME_THRESH] = settings.value(AppConstants::K_BLACK_FRAME_THRESH, 17.0);
    settingsMap[AppConstants::K_SCENE_THRESH] = settings.value(AppConstants::K_SCENE_THRESH, 30.0);
    settingsMap[AppConstants::K_HAS_TRANSITIONS] = settings.value(AppConstants::K_HAS_TRANSITIONS, false);
    
    setSettings(settingsMap);
}

void ConfigWidget::setSettings(const QVariantMap &settings)
{
    m_blackFrameBox->setChecked(settings.value(AppConstants::K_DETECT_BLACK_FRAMES, true).toBool());
    m_blackBorderBox->setChecked(settings.value(AppConstants::K_DETECT_BLACK_BORDERS, true).toBool());
    m_orphanFrameBox->setChecked(settings.value(AppConstants::K_DETECT_ORPHAN_FRAMES, true).toBool());
    
    m_borderThreshSpinBox->setValue(settings.value(AppConstants::K_BORDER_THRESH, 0.2).toDouble());
    m_orphanFrameThreshSpinBox->setValue(settings.value(AppConstants::K_ORPHAN_THRESH, 5).toInt());
    m_blackFrameThreshSpinBox->setValue(settings.value(AppConstants::K_BLACK_FRAME_THRESH, 17.0).toDouble());
    m_sceneDetectThreshSpinBox->setValue(settings.value(AppConstants::K_SCENE_THRESH, 30.0).toDouble());
    m_hasTransitionsCheck->setChecked(settings.value(AppConstants::K_HAS_TRANSITIONS, false).toBool());

    bool transitionsEnabled = m_hasTransitionsCheck->isChecked();
    m_sceneDetectThreshSpinBox->setDisabled(transitionsEnabled);
}

void ConfigWidget::saveSettings()
{
    QSettings settings(AppConstants::ORG_NAME, AppConstants::APP_NAME);
    QVariantMap currentSettings = getSettings();
    for(auto it = currentSettings.constBegin(); it != currentSettings.constEnd(); ++it) {
        settings.setValue(it.key(), it.value());
    }
}

QVariantMap ConfigWidget::getSettings() const
{
    QVariantMap settings;
    settings[AppConstants::K_DETECT_BLACK_FRAMES] = m_blackFrameBox->isChecked();
    settings[AppConstants::K_DETECT_BLACK_BORDERS] = m_blackBorderBox->isChecked();
    settings[AppConstants::K_DETECT_ORPHAN_FRAMES] = m_orphanFrameBox->isChecked();
    
    settings[AppConstants::K_BORDER_THRESH] = m_borderThreshSpinBox->value();
    settings[AppConstants::K_ORPHAN_THRESH] = m_orphanFrameThreshSpinBox->value();
    settings[AppConstants::K_BLACK_FRAME_THRESH] = m_blackFrameThreshSpinBox->value();
    settings[AppConstants::K_SCENE_THRESH] = m_sceneDetectThreshSpinBox->value();
    settings[AppConstants::K_HAS_TRANSITIONS] = m_hasTransitionsCheck->isChecked();

    return settings;
}

void ConfigWidget::onSelectFileClicked()
{
    const QString videoFilter = "Video Files (*.mp4 *.mov *.avi *.mkv *.ts *.m2ts *.mxf);;All Files (*)";
    QString path = QFileDialog::getOpenFileName(this, "Chọn file video", "", videoFilter);
    if (!path.isEmpty()) {
        emit filePathSelected(path);
    }
}

void ConfigWidget::onSelectReportClicked()
{
    const QString reportFilter = "QCTools Reports (*.xml *.xml.gz *.qctools.xml *.qctools.xml.gz *.qctools.mkv);;All Files (*)";
    QString path = QFileDialog::getOpenFileName(this, "Chọn file báo cáo QCTools", "", reportFilter);
    if (!path.isEmpty()) {
        emit reportPathSelected(path);
    }
}

void ConfigWidget::onSavePresetClicked()
{
    QString filePath = QFileDialog::getSaveFileName(this, "Lưu Cấu hình Phân tích", "", "JSON Files (*.json)");
    if (filePath.isEmpty()) return;

    QFile saveFile(filePath);
    if (!saveFile.open(QIODevice::WriteOnly)) {
        QMessageBox::critical(this, "Lỗi", "Không thể ghi file cấu hình.");
        return;
    }

    QVariantMap settings = getSettings();
    QJsonObject jsonObject = QJsonObject::fromVariantMap(settings);
    QJsonDocument saveDoc(jsonObject);

    saveFile.write(saveDoc.toJson(QJsonDocument::Indented));
    saveFile.close();
    QMessageBox::information(this, "Thành công", "Đã lưu file cấu hình thành công.");
}

void ConfigWidget::onLoadPresetClicked()
{
    QString filePath = QFileDialog::getOpenFileName(this, "Tải Cấu hình Phân tích", "", "JSON Files (*.json)");
    if (filePath.isEmpty()) return;

    QFile loadFile(filePath);
    if (!loadFile.open(QIODevice::ReadOnly)) {
        QMessageBox::critical(this, "Lỗi", "Không thể đọc file cấu hình.");
        return;
    }

    QByteArray saveData = loadFile.readAll();
    QJsonDocument loadDoc(QJsonDocument::fromJson(saveData));

    if (loadDoc.isNull() || !loadDoc.isObject()) {
        QMessageBox::critical(this, "Lỗi", "File cấu hình không hợp lệ hoặc bị lỗi.");
        return;
    }

    QVariantMap settings = loadDoc.object().toVariantMap();
    setSettings(settings); 

    QMessageBox::information(this, "Thành công", "Đã tải và áp dụng cấu hình thành công.");
}

