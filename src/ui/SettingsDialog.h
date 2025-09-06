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
class QTabWidget;

class SettingsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit SettingsDialog(QWidget *parent = nullptr);
    QVariantMap getSettings() const;
    void openPathsTab();

private slots:
    void onBrowseQCTools();
    void onBrowseQCCli();

private:
    void setupUI();
    void loadSettings();
    void saveSettings();

    // UI Elements
    QTabWidget* m_tabWidget;

    // Paths Tab
    QLineEdit* m_qctoolsPathEdit;
    QLineEdit* m_qcliPathEdit;

    // Interaction Tab
    QSpinBox* m_rewindFramesSpinBox;
    
    // Hardware Tab
    QCheckBox* m_hwAccelCheck;
    QComboBox* m_hwAccelTypeCombo;
};

#endif // SETTINGSDIALOG_H

