// src/mainwindow.cpp
#include "mainwindow.h"
#include "ui/videowidget.h"
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QUrl>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    m_videoWidget = new VideoWidget(this);
    setCentralWidget(m_videoWidget);
    
    setWindowTitle("Video QC Tool");
    setMinimumSize(500, 650);
    resize(500, 750);

    setAcceptDrops(true);

    connect(m_videoWidget, &VideoWidget::videoFileChanged, this, &MainWindow::updateWindowTitle);
    connect(this, &MainWindow::fileDropped, m_videoWidget, &VideoWidget::handleFileDrop);
}

MainWindow::~MainWindow() {}

void MainWindow::updateWindowTitle(const QString& videoName)
{
    QString baseTitle = "Video QC Tool";
    if (videoName.isEmpty()) {
        setWindowTitle(baseTitle);
    } else {
        setWindowTitle(QString("%1 - [%2]").arg(baseTitle, videoName));
    }
}

void MainWindow::dragEnterEvent(QDragEnterEvent *event)
{
    if (event->mimeData()->hasUrls()) {
        event->acceptProposedAction();
    }
}

void MainWindow::dropEvent(QDropEvent *event)
{
    const QMimeData* mimeData = event->mimeData();
    if (mimeData->hasUrls()) {
        QList<QUrl> urlList = mimeData->urls();
        if (!urlList.isEmpty()) {
            QString path = urlList.first().toLocalFile();
            if (!path.isEmpty()) {
                emit fileDropped(path);
            }
        }
    }
}

