#pragma once

#include <QMainWindow>

class PluginWorkspace;

// 顶层窗口。
//
// 对应界面：没有独立 mainwindow.ui；中央界面由 wid_stack.ui 构建。
// MainWindow 只负责承载 wid_stack。设备管理、数据源导入和数据回看之间的
// 页面切换全部由 wid_stack 管理，避免主窗口同时承担业务逻辑。
class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    // 构造顶层窗口。
    // @param parent Qt 父控件，通常为 nullptr，表示创建独立顶层窗口。
    explicit MainWindow(QWidget *parent = nullptr);

    // 将启动参数中的文件转交给内部 wid_stack。
    // @param filePath 待选择的 BIN 文件路径。
    // @return wid_stack 接受并切换到数据页时返回 true。
    bool loadFile(const QString &filePath);

private:
    PluginWorkspace *m_workspace;
};
