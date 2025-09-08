// src/main.cpp
#include "mainwindow.h"
#include <QApplication>
#include <QIcon>
#include <QFile>        // Thêm thư viện để làm việc với file
#include <QTextStream>  // Thêm thư viện để đọc file text
#include "core/Constants.h"

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    QCoreApplication::setOrganizationName(AppConstants::ORG_NAME);
    QCoreApplication::setApplicationName(AppConstants::APP_NAME);

    // CẢI TIẾN: Đọc và áp dụng stylesheet toàn cục
    QFile f(":/ui/stylesheet.qss");
    if (f.open(QFile::ReadOnly | QFile::Text)) {
        QTextStream in(&f);
        QString style = in.readAll();
        a.setStyleSheet(style);
        f.close();
    }
    // Kết thúc CẢI TIẾN

    // CẢI TIẾN: Sử dụng icon từ file tài nguyên mới
    a.setWindowIcon(QIcon(":/app_icon.ico"));

    MainWindow w;
    w.show();

    return a.exec();
}
