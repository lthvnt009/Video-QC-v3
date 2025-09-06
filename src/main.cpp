// src/main.cpp
#include "mainwindow.h"
#include <QApplication>
#include <QIcon>
#include "core/Constants.h" // BƯỚC 1.4: Thêm file header mới

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    // BƯỚC 1.4: Sử dụng hằng số thay vì "magic strings"
    QCoreApplication::setOrganizationName(AppConstants::ORG_NAME);
    QCoreApplication::setApplicationName(AppConstants::APP_NAME); 

    a.setWindowIcon(QIcon(":/magnifying-glass.ico"));

    MainWindow w;
    w.show();

    return a.exec();
}

