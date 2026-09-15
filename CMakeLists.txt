cmake_minimum_required(VERSION 3.16)

project(Data_acquisition_and_analysis_system LANGUAGES CXX)

include(qt.cmake)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTORCC ON)#从资源路径加载qss必须加上这条

# 1. 引入 Qt 所需组件（增加了 Network、Sql、Charts）
find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Core)
find_package(Qt${QT_VERSION_MAJOR}
    COMPONENTS
        Core
        Gui
        Widgets
        Network
        Sql
        Charts
        SerialBus
        StateMachine 
)

qt_standard_project_setup()

#汇总所有源文件与UI文件
set(PROJECT_SOURCES
    main.cpp
    Data_acquisition_and_analysis_system.rc

    #全局配置单例
    ui/pages/ConfigManager.h

    #主窗口
    ui/MainWindow.h
    ui/MainWindow.cpp
    ui/MainWindow.ui

    #4个子页面
    ui/pages/AcquisitionPage.h
    ui/pages/AcquisitionPage.cpp
    ui/pages/AcquisitionPage.ui

    ui/pages/AnalysisPage.h
    ui/pages/AnalysisPage.cpp
    ui/pages/AnalysisPage.ui

    ui/pages/ConfigPage.h
    ui/pages/ConfigPage.cpp
    ui/pages/ConfigPage.ui

    ui/pages/LogPage.h
    ui/pages/LogPage.cpp
    ui/pages/LogPage.ui

    #网络和数据库
    network/ModbusManager.h
    network/ModbusManager.cpp
    network/StationData.h
    database/DatabaseManager.h
    database/DatabaseManager.cpp

    #资源文件
    Resource.qrc
)

qt_add_executable(${PROJECT_NAME} ${PROJECT_SOURCES})

#设置头文件搜索路径
target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/ui
    ${CMAKE_CURRENT_SOURCE_DIR}/ui/pages
    ${CMAKE_CURRENT_SOURCE_DIR}/network
    ${CMAKE_CURRENT_SOURCE_DIR}/database
)

set_target_properties(${PROJECT_NAME}
    PROPERTIES
        WIN32_EXECUTABLE FALSE
)

target_link_libraries(${PROJECT_NAME}
    PUBLIC
        Qt::Core
        Qt::Gui
        Qt::Widgets
        Qt::Network
        Qt::Sql
        Qt::Charts
        Qt::SerialBus
        Qt::StateMachine
)