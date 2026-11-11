#pragma once

#include "auroraframeeditdialog.h"

// 以太网（TCP）数据帧编辑对话框。
//
// 对应界面：复用 auroraframeeditdialog.ui 的布局，但由本类独立实现窗口行为，
// 以太网总线不再直接使用 AuroraFrameEditDialog。
//
// 为什么是派生类：
//   Aurora 与以太网当前的参数区域排布相同（PW/PRI/TOA 参数行 + 初始值/步进行
//   + 数据值列表），因此本类继承 AuroraFrameEditDialog 复用这套实现，
//   只把“总线类型”和界面标题替换成以太网自己的：
//     - 构造时即把总线模式设为 BusMode::Tcp；
//     - 重写 parameterSectionTitle() 返回“以太网参数设置”。
//
// 后续如果要让以太网窗口与 Aurora 完全分离（不同的布局、不同的字段）：
//   1. 把 auroraframeeditdialog.ui 复制为 tcpframeeditdialog.ui，
//      并把 <class> 改成 TcpFrameEditDialog；
//   2. 在本类中改为持有自己的 Ui::TcpFrameEditDialog；
//   3. 把需要改动的界面逻辑在本类中重写（参数字段、数据值列表等）。
//   其余调用方（wid_customdatasource）无需修改，仍然只使用本类。
//
// 数据来源与保存格式沿用 AuroraFrameEditData，外层帧记录无需区分总线类型。
class TcpFrameEditDialog : public AuroraFrameEditDialog
{
    Q_OBJECT

public:
    // 构造以太网帧编辑对话框。
    // @param parent Qt 父窗口。
    explicit TcpFrameEditDialog(QWidget *parent = nullptr);

    // 销毁对话框。
    ~TcpFrameEditDialog() override;

    // ── 以太网参数字段接口 ───────────────────────────────────────
    // 与基类 addTcpParameter() / addTcpSequenceParameter() 等价，
    // 这里再提供一层以本类命名的入口，便于后续以太网单独扩展。
    // 参数含义：变量名、字节/位、起始字节、字节长/位长。
    void addTcpDataParameter(const QString &name,
                             DataParameterUnit unit,
                             int startByte,
                             int length);
    void addTcpDataSequenceParameter(const QString &name,
                                     DataParameterUnit unit,
                                     int startByte,
                                     int length);

protected:
    // 参数区域标题：以太网使用自己的标题，不再显示“Aurora参数设置”。
    QString parameterSectionTitle() const override;
};
