# CMakeLists.txt (v37.0 - Cải tiến Toàn diện & Thêm file mới)
cmake_minimum_required(VERSION 3.16)
project(VideoQCTool_v2.2)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Xml Network)

qt_add_resources(RESOURCES src/resources.qrc)

set(SOURCES
    src/main.cpp
    src/mainwindow.cpp
    src/ui/configwidget.cpp
    src/ui/resultswidget.cpp
    src/ui/settingsdialog.cpp
    src/ui/videowidget.cpp
    src/ui/logdialog.cpp
    src/ui/clickableheaderview.cpp # THÊM FILE MỚI
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
    src/ui/clickableheaderview.h # THÊM FILE MỚI
    src/qctools/QCToolsManager.h
    src/qctools/QCToolsController.h
)

add_executable(${PROJECT_NAME} WIN32 ${SOURCES} ${HEADERS} ${RESOURCES})

if(WIN32)
    target_sources(${PROJECT_NAME} PRIVATE src/app.rc)
endif()

target_include_directories(${PROJECT_NAME} PRIVATE 
    "${CMAKE_SOURCE_DIR}/src"
)

get_filename_component(QT_INSTALL_PREFIX "${Qt6_DIR}/../../../" ABSOLUTE)
target_include_directories(${PROJECT_NAME} PRIVATE "${QT_INSTALL_PREFIX}/include/QtZlib")


target_link_libraries(${PROJECT_NAME} PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Xml
    Qt6::Network
)
