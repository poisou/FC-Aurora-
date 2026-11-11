cmake_minimum_required(VERSION 3.16)

project(DataSimulator VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets Sql Svg)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets Sql Svg)

add_executable(DataSimulator
    WIN32
    main.cpp
    mainwindow.cpp
    mainwindow.h
    pluginworkspace.cpp
    pluginworkspace.h
    sceneparallelwidget.cpp
    sceneparallelwidget.h
    wid_stack.cpp
    wid_stack.h
    wid_stack.ui
    wid_review.cpp
    wid_review.h
    wid_review.ui
    wid_datasource.cpp
    wid_datasource.h
    wid_datasource.ui
    wid_customdatasource.cpp
    wid_customdatasource.h
    wid_customdatasource.ui
    auroraframeeditdialog.cpp
    auroraframeeditdialog.h
    auroraframeeditdialog.ui
    ahead.h
    datasourcefilecopyworker.cpp
    datasourcefilecopyworker.h
    datasourceuploaddialog.cpp
    datasourceuploaddialog.h
    datasourceuploaddialog.ui
    PluginUi/plugin_wid_mainwindow.ui
    PluginUi/plugin_wid_stack.ui
    PluginUi/plugin_wid_rulwindow.ui
    sceneparallelwidget.ui
    binformat.cpp
    binformat.h
    binfileimporterbase.cpp
    binfileimporterbase.h
    aurorafileimporter.cpp
    aurorafileimporter.h
    fcfileimporter.cpp
    fcfileimporter.h
    tcpfileimporter.cpp
    tcpfileimporter.h
    resources.qrc
)

target_link_libraries(DataSimulator PRIVATE
    Qt${QT_VERSION_MAJOR}::Widgets
    Qt${QT_VERSION_MAJOR}::Sql
    Qt${QT_VERSION_MAJOR}::Svg
)

if(MSVC)
    target_compile_options(DataSimulator PRIVATE /W4 /utf-8 /permissive-)
else()
    target_compile_options(DataSimulator PRIVATE -Wall -Wextra -Wpedantic)
endif()

set_target_properties(DataSimulator PROPERTIES
    WIN32_EXECUTABLE TRUE
    MACOSX_BUNDLE TRUE
    MACOSX_BUNDLE_BUNDLE_NAME "DataSimulator"
    MACOSX_BUNDLE_GUI_IDENTIFIER "com.aurora.create"
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
)

include(GNUInstallDirs)
install(TARGETS DataSimulator
    BUNDLE DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
