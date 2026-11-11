// 对应界面：无独立 .ui；中央显示 wid_stack.ui。
// MainWindow 只负责窗口级属性赋值，不承载业务页面。
#include "mainwindow.h"

#include "pluginworkspace.h"

#include <QIcon>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_workspace(new PluginWorkspace(this))
{
    // 窗口标题：如需修改软件名称，改这里。
    setWindowTitle(QStringLiteral("DataSimulator - 数据模拟器软件 V1.0"));
    // 应用图标资源路径：图标文件在 resources.qrc 中注册。
    setWindowIcon(QIcon(QStringLiteral(":/icons/app.svg")));
    // 使用插件 wid_mainwindow + wid_stack 的页签式工作区。
    setCentralWidget(m_workspace);
    // 启动时窗口尺寸和允许缩到的最小尺寸。
    resize(1500, 1000);
    setMinimumSize(1260, 900);
}

bool MainWindow::loadFile(const QString &filePath)
{
    return m_workspace->loadFile(filePath);
}
