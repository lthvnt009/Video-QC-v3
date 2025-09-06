// src/ui/settingsdialog.h
#ifndef SETTINGSDIALOG_H
#define SETTINGSDIALOG_H

#include <QDialog>
#include <QVariantMap>

// Forward declarations
class QSpinBox;
class QCheckBox;
class QComboBox;
class QLabel;
class QPushButton;
class QLineEdit;
class QTabWidget; // SỬA LỖI: Bổ sung forward declaration còn thiếu

class SettingsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit SettingsDialog(QWidget *parent = nullptr);
    QVariantMap getSettings() const;
    void setCacheDirectory(const QString& path);
    void openPathsTab();

private slots:
    void onClearCacheClicked();
    void onOpenCacheFolderClicked();
    void onBrowseQCTools();
    void onBrowseQCCli();

private:
    void setupUI();
    void loadSettings();
    void saveSettings();
    void updateCacheSizeLabel();
    qint64 calculateCacheSize() const;

    // UI Elements
    QTabWidget* m_tabWidget;

    // Paths Tab
    QLineEdit* m_qctoolsPathEdit;
    QLineEdit* m_qcliPathEdit;

    // Preview Tab
    QSpinBox* m_rewindFramesSpinBox;
    
    // Hardware Tab
    QCheckBox* m_hwAccelCheck;
    QComboBox* m_hwAccelTypeCombo;
    
    // Cache Tab
    QLabel* m_cacheSizeLabel;
    QPushButton* m_clearCacheButton;
    QPushButton* m_openCacheButton;
    
    // State
    QString m_cacheDir;
};

#endif // SETTINGSDIALOG_H

