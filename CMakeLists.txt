# CMakeLists.txt (v37.0 - Cải tiến Toàn diện)
cmake_minimum_required(VERSION 3.16)
project(VideoQCTool_v2.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Xml Network)

# CẢI TIẾN: Sử dụng file .qrc mới để quản lý tài nguyên
qt_add_resources(RESOURCES src/resources.qrc)

set(SOURCES
    src/main.cpp
    src/mainwindow.cpp
    src/ui/configwidget.cpp
    src/ui/resultswidget.cpp
    src/ui/settingsdialog.cpp
    src/ui/videowidget.cpp
    src/ui/logdialog.cpp
    src/qctools/QCToolsManager.cpp
    src/qctools/QCToolsController.cpp
)

set(HEADERS
    src/mainwindow.h
    src/core/types.h
    src/core/Constants.h
    src/core/media_info.h
    src/ui/configwidget.h
    src/ui/resultswidget.h
    src/ui/settingsdialog.h
    src/ui/videowidget.h
    src/ui/logdialog.h
    src/qctools/QCToolsManager.h
    src/qctools/QCToolsController.h
)

# BƯỚC 1: Định nghĩa chương trình thực thi (target)
add_executable(${PROJECT_NAME} WIN32 ${SOURCES} ${HEADERS} ${RESOURCES})

# CẢI TIẾN: Thêm icon cho file .exe trên Windows
if(WIN32)
    # Thêm file resource script (.rc) của Windows vào target
    # File này sẽ nhúng icon vào file .exe cuối cùng
    target_sources(${PROJECT_NAME} PRIVATE src/app.rc)
endif()

# BƯỚC 2: Cấu hình các đường dẫn include cho target
target_include_directories(${PROJECT_NAME} PRIVATE 
    "${CMAKE_SOURCE_DIR}/src"
)

# CẢI TIẾN: Lấy đường dẫn cài đặt Qt một cách an toàn
# Sau đó chỉ cho trình biên dịch đến thư mục chứa zlib.h của Qt
get_filename_component(QT_INSTALL_PREFIX "${Qt6_DIR}/../../../" ABSOLUTE)
target_include_directories(${PROJECT_NAME} PRIVATE "${QT_INSTALL_PREFIX}/include/QtZlib")


# BƯỚC 3: Liên kết các thư viện cần thiết
target_link_libraries(${PROJECT_NAME} PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Xml
    Qt6::Network
)

