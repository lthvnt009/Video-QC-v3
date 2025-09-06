// src/ui/logdialog.h
#ifndef LOGDIALOG_H
#define LOGDIALOG_H

#include <QDialog>
#include <QStringList>

class QPlainTextEdit;
class QPushButton;

class LogDialog : public QDialog
{
    Q_OBJECT

public:
    explicit LogDialog(const QStringList& history, QWidget *parent = nullptr);
    void appendLog(const QString& message);
    void setDefaultSavePath(const QString& path);
    void setVideoFileName(const QString& name);

private slots:
    void onCopyClicked();
    void onExportClicked();
    void onClearClicked();

private:
    void setupUI();

    QPlainTextEdit* m_logEdit;
    QPushButton* m_copyButton;
    QPushButton* m_exportButton;
    QPushButton* m_clearButton;
    QString m_defaultSaveDir;
    QString m_videoFileName;
};

#endif // LOGDIALOG_H

