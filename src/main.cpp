// src/main.cpp
#include "mainwindow.h"
#include <QApplication>
#include <QIcon>
#include "core/Constants.h"

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    QCoreApplication::setOrganizationName(AppConstants::ORG_NAME);
    QCoreApplication::setApplicationName(AppConstants::APP_NAME); 

    // CẢI TIẾN: Sử dụng icon từ file tài nguyên mới
    a.setWindowIcon(QIcon(":/app_icon.ico"));

    MainWindow w;
    w.show();

    return a.exec();
}

