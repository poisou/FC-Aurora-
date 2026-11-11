#pragma once

#include <QWidget>

namespace Ui {
class wid_mainwindow;
}

class WidReview;
class wid_customdatasource;
class wid_datasource;
class ProtocolRuleWidget;

// 插件式主工作区。
//
// 对应插件 UI：UIWidget/wid_mainwindow.ui。
// 使用插件主窗口的五个页签，并嵌入 DataSimulator 现有的数据源管理、
// 数据回看逻辑，以及插件版设备管理、协议规则和场景并行界面。
class PluginWorkspace : public QWidget
{
    Q_OBJECT

public:
    // 构造插件式主工作区。
    // @param parent Qt 父控件。
    explicit PluginWorkspace(QWidget *parent = nullptr);

    // 销毁界面。
    ~PluginWorkspace() override;

    // 打开文件并切换到数据回看页。
    // @param filePath BIN 文件路径。
    // @return 文件被接受时返回 true。
    bool loadFile(const QString &filePath);

private:
    Ui::wid_mainwindow *ui;
    wid_datasource *m_dataSourceWidget = nullptr;
    wid_customdatasource *m_customDataSourceWidget = nullptr;
    WidReview *m_reviewWidget = nullptr;
    ProtocolRuleWidget *m_protocolRuleWidget = nullptr;
};
