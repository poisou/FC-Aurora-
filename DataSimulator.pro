QT += core gui widgets sql svg network

CONFIG += c++17
TEMPLATE = app
TARGET = DataSimulator

SOURCES += \
    main.cpp \
    mainwindow.cpp \
    pluginworkspace.cpp \
    sceneparallelwidget.cpp \
    devicework.cpp \
    fcsendpro.cpp \
    fcrecvpro.cpp \
    aurorasendpro.cpp \
    aurorarecvpro.cpp \
    tcpsendpro.cpp \
    tcprecvpro.cpp \
    wid_stack.cpp \
    wid_review.cpp \
    wid_datasource.cpp \
    wid_customdatasource.cpp \
    auroraframeeditdialog.cpp \
    fcframeeditdialog.cpp \
    tcpframeeditdialog.cpp \
    datasourcefilecopyworker.cpp \
    datasourceuploaddialog.cpp \
    chunkedtablefiller.cpp \
    binformat.cpp \
    binfileimporterbase.cpp \
    aurorafileimporter.cpp \
    fcfileimporter.cpp \
    tcpfileimporter.cpp \
    vipdatamodel.cpp \
    protocolrulewidget.cpp \
    protocolruleeditdialog.cpp

HEADERS += \
    mainwindow.h \
    pluginworkspace.h \
    sceneparallelwidget.h \
    devicework.h \
    fcsendpro.h \
    fcrecvpro.h \
    aurorasendpro.h \
    aurorarecvpro.h \
    tcpsendpro.h \
    tcprecvpro.h \
    wid_stack.h \
    wid_review.h \
    wid_datasource.h \
    wid_customdatasource.h \
    auroraframeeditdialog.h \
    fcframeeditdialog.h \
    tcpframeeditdialog.h \
    ahead.h \
    datasourcefilecopyworker.h \
    datasourceuploaddialog.h \
    chunkedtablefiller.h \
    binformat.h \
    binfileimporterbase.h \
    aurorafileimporter.h \
    fcfileimporter.h \
    tcpfileimporter.h \
    vipdatamodel.h \
    protocolrulewidget.h \
    protocolruleeditdialog.h

FORMS += \
    PluginUi/plugin_wid_mainwindow.ui \
    PluginUi/plugin_wid_stack.ui \
    PluginUi/plugin_wid_rulwindow.ui \
    sceneparallelwidget.ui \
    wid_stack.ui \
    wid_review.ui \
    wid_datasource.ui \
    wid_customdatasource.ui \
    auroraframeeditdialog.ui \
    datasourceuploaddialog.ui

RESOURCES += \
    resources.qrc

msvc {
    QMAKE_CXXFLAGS += /utf-8 /permissive-
}
