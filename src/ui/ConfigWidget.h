// src/ui/configwidget.h
#ifndef CONFIGWIDGET_H
#define CONFIGWIDGET_H

#include <QWidget>
#include <QVariantMap>

class QLineEdit;
class QGroupBox;
class QSpinBox;
class QDoubleSpinBox;
class QCheckBox;
class QPushButton; // Thêm khai báo QPushButton

class ConfigWidget : public QWidget
{
    Q_OBJECT

public:
    explicit ConfigWidget(QWidget *parent = nullptr);
    QVariantMap getSettings() const;
    void setInputPath(const QString& path);
    void setSettings(const QVariantMap& settings); // Hàm mới để áp dụng cài đặt từ bên ngoài

public slots:
    void reloadSettings();

signals:
    void filePathSelected(const QString &path);
    void reportPathSelected(const QString &path);

private slots:
    void onSelectFileClicked();
    void onSelectReportClicked();
    // Slots mới cho tính năng preset
    void onSavePresetClicked();
    void onLoadPresetClicked();

private:
    void setupUI();
    void loadSettings();
    void saveSettings();

    // Input
    QLineEdit *m_videoPathEdit;

    // --- Error Groups ---
    QGroupBox* m_blackFrameBox;
    QGroupBox* m_blackBorderBox;
    QGroupBox* m_orphanFrameBox;

    // --- Preset Buttons (Mới) ---
    QPushButton* m_savePresetButton;
    QPushButton* m_loadPresetButton;
    
    // --- Thresholds ---
    // Black Frames
    QDoubleSpinBox* m_blackFrameThreshSpinBox;

    // Black Borders
    QDoubleSpinBox* m_borderThreshSpinBox;
    
    // Orphan Frames
    QSpinBox* m_orphanFrameThreshSpinBox;
    QDoubleSpinBox* m_sceneDetectThreshSpinBox;
    QCheckBox* m_hasTransitionsCheck;
};

#endif // CONFIGWIDGET_H
