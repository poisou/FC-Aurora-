#pragma once

#include "mytools/ahead.h"
#include "auroraframeeditdialog.h"
#include "myTools/wid_fcdata.h"
#include <QByteArray>
#include <QDateTime>
#include <QString>
#include <QVector>
#include <QWidget>

namespace Ui {
class wid_customdatasource;
}

class QTableWidgetItem;

// 自定义数据源编辑页面。
//
// 对应界面文件：wid_customdatasource.ui
// 该页面完全由 wid_customdatasource.ui 构建，作为 wid_stack 内容栈中的
// 独立页面显示。点击“返回”时只发出信号，实际页面切换由 wid_stack 完成。
class wid_customdatasource : public QWidget
{
    Q_OBJECT

public:
    // 构造自定义数据源页面。
    // @param parent Qt 父控件。
    explicit wid_customdatasource(QWidget *parent = nullptr);

    // 销毁 Designer UI。
    ~wid_customdatasource() override;

signals:
    // 用户请求返回数据源管理主页面。
    void backRequested();

    // 自定义数据源文件保存成功，请求写入数据库并刷新数据源列表。
    // @param name 数据源名称。
    // @param busType 总线类型：FC、Aurora 或 以太网。
    // @param filePath MYDATA 中生成文件的绝对路径。
    // @param createdAt 数据源创建时间。
    void saveRequested(const QString &name,
                       const QString &busType,
                       const QString &filePath,
                       const QDateTime &createdAt);

private slots:
    // 返回数据源管理主页面。
    void handleBack();

    // 响应总线类型变化；当前三种类型统一使用 FC 风格页面。
    // @param index 0=FC，1=Aurora，2=以太网。
    void updateBusTypePage(int index);

    // 根据“帧个数”在帧列表中生成对应数量的空帧行。
    void generateFrames();

    // 清空帧列表中的全部帧数据，并复位到第一页。
    void clearFrames();

    // 跳到帧列表上一页。
    void showPreviousFramePage();

    // 跳到帧列表下一页。
    void showNextFramePage();

    // 根据页码输入框跳转到指定帧页。
    void jumpToFramePage();

    // 将当前页单元格修改同步到完整帧数据。
    // @param item 发生修改的帧长或值单元格。
    void handleFrameItemChanged(QTableWidgetItem *item);


    // 组合所有帧及 6 字节 AHEAD，保存为 BIN 文件并请求写入数据库。
    void saveDataSource();

private:
    // 一帧在内存中的数据。表格只显示其中的当前 50 行，
    // 完整帧数据始终保存在 m_frames 中，翻页不会丢失修改。
    struct FrameRecord {
        QString length;
        QString value;
        QByteArray dataBytes;
        AuroraFrameEditData auroraData;
    };

    // 初始化帧列表列标题、列宽和交互方式。
    void configureFrameTable();


    //初始化FC数据列表
    void configureFCTable();

    // 在指定表格行创建“编辑”和“删除”按钮。
    // @param row 当前页中的表格行号。
    void createOperationWidget(int row);

    // 按当前页码刷新最多 50 帧，并同步分页控件。
    void populateFramePage();

    // 删除完整帧数据中的指定帧。
    // @param globalIndex 帧在 m_frames 中的全局索引，从 0 开始。
    void removeFrame(int globalIndex);

    Ui::wid_customdatasource *ui;
    QVector<FrameRecord> m_frames;
    int m_framePage = 0;
};
