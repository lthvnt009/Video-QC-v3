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

class ConfigWidget : public QWidget
{
    Q_OBJECT

public:
    explicit ConfigWidget(QWidget *parent = nullptr);
    QVariantMap getSettings() const;
    void setInputPath(const QString& path);

signals:
    void filePathSelected(const QString &path);
    void reportPathSelected(const QString &path);

private slots:
    void onSelectFileClicked();
    void onSelectReportClicked();

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

