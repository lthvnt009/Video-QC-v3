// src/ui/logdialog.cpp
#include "logdialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QApplication>
#include <QClipboard>
#include <QFileDialog>
#include <QFile>
#include <QTextStream>
#include <QMessageBox>
#include <QScrollBar> // SỬA LỖI: Thêm header cho QScrollBar

LogDialog::LogDialog(const QStringList& history, QWidget *parent)
    : QDialog(parent)
{
    setupUI();
    setWindowTitle("Nhật ký Hoạt động");
    setMinimumSize(700, 500);
    m_logEdit->appendPlainText(history.join("\n"));
}

void LogDialog::setupUI()
{
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    
    m_logEdit = new QPlainTextEdit(this);
    m_logEdit->setReadOnly(true);
    m_logEdit->setFont(QFont("Courier New", 9));

    QHBoxLayout* buttonLayout = new QHBoxLayout();
    m_copyButton = new QPushButton("Sao chép vào Clipboard");
    m_exportButton = new QPushButton("Xuất ra TXT...");
    m_clearButton = new QPushButton("Xóa Log");
    
    buttonLayout->addWidget(m_copyButton);
    buttonLayout->addWidget(m_exportButton);
    buttonLayout->addStretch();
    buttonLayout->addWidget(m_clearButton);

    mainLayout->addLayout(buttonLayout);
    mainLayout->addWidget(m_logEdit);

    connect(m_copyButton, &QPushButton::clicked, this, &LogDialog::onCopyClicked);
    connect(m_exportButton, &QPushButton::clicked, this, &LogDialog::onExportClicked);
    connect(m_clearButton, &QPushButton::clicked, this, &LogDialog::onClearClicked);
}

void LogDialog::appendLog(const QString &message)
{
    m_logEdit->appendPlainText(message);
    m_logEdit->verticalScrollBar()->setValue(m_logEdit->verticalScrollBar()->maximum());
}

void LogDialog::setDefaultSavePath(const QString &path)
{
    m_defaultSaveDir = path;
}

void LogDialog::setVideoFileName(const QString &name)
{
    m_videoFileName = name;
}

void LogDialog::onCopyClicked()
{
    QApplication::clipboard()->setText(m_logEdit->toPlainText());
    QMessageBox::information(this, "Thành công", "Đã sao chép nhật ký vào clipboard.");
}

void LogDialog::onExportClicked()
{
    QString suggestedName = m_defaultSaveDir + "/ActivityLog.txt";
    QString filePath = QFileDialog::getSaveFileName(this, "Lưu file Nhật ký", suggestedName, "Text Files (*.txt)");
    if (filePath.isEmpty()) return;

    QFile file(filePath);
    if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream out(&file);
        out.setEncoding(QStringConverter::Utf8);
        if (!m_videoFileName.isEmpty()) {
            out << "File: " << m_videoFileName << "\n\n";
        }
        out << m_logEdit->toPlainText();
        file.close();
        QMessageBox::information(this, "Thành công", "Đã xuất file nhật ký thành công.");
    } else {
        QMessageBox::critical(this, "Lỗi", "Không thể lưu file nhật ký.");
    }
}

void LogDialog::onClearClicked()
{
    m_logEdit->clear();
}
