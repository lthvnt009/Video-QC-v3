// src/mainwindow.h
#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>

class VideoWidget;
class QDragEnterEvent;
class QDropEvent;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

signals:
    void fileDropped(const QString& path);

public slots:
    void updateWindowTitle(const QString& videoName);

protected:
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dropEvent(QDropEvent *event) override;

private:
    VideoWidget *m_videoWidget;
};
#endif // MAINWINDOW_H

