#pragma once

#include "auroraframeeditdialog.h"

// FC 数据帧编辑对话框。
//
// 对应界面：复用 auroraframeeditdialog.ui 的布局，但由本类独立实现窗口行为，
// FC 总线不再直接使用 AuroraFrameEditDialog。
//
// 为什么是派生类：
//   FC 的参数区域是 “FC Header set”（D_ID、R_CTL … MSG_L 共 18 项），
//   与 Aurora 共用同一套数据值列表、分页和保存流程，
//   因此本类继承 AuroraFrameEditDialog，只把“总线类型”和标题换成 FC 自己的：
//     - 构造时即把总线模式设为 BusMode::Fc（显示 FC Header set 网格）；
//     - 重写 parameterSectionTitle() 返回“FC Header set”。
//
// 后续如果要让 FC 窗口完全独立（不同的布局或字段）：
//   1. 把 auroraframeeditdialog.ui 复制为 fcframeeditdialog.ui，
//      并把 <class> 改成 FcFrameEditDialog；
//   2. 在本类中改为持有自己的 Ui::FcFrameEditDialog；
//   3. 把需要改动的界面逻辑在本类中重写。
//   其余调用方（wid_customdatasource）无需修改，仍然只使用本类。
//
// 数据来源与保存格式沿用 AuroraFrameEditData，外层帧记录无需区分总线类型。
class FcFrameEditDialog : public AuroraFrameEditDialog
{
    Q_OBJECT

public:
    // 构造 FC 帧编辑对话框。
    // @param parent Qt 父窗口。
    explicit FcFrameEditDialog(QWidget *parent = nullptr);

    // 销毁对话框。
    ~FcFrameEditDialog() override;

    // ── FC 参数字段接口 ──────────────────────────────────────────
    // 在 FC Header set 网格下方追加一行参数字段（变量名 + 10 进制值）。
    // 值按下方数据值列表当前页、用字节/位位置实时计算。
    // 参数含义：变量名、字节/位、起始字节、字节长/位长。
    void addFcDataParameter(const QString &name,
                            DataParameterUnit unit,
                            int startByte,
                            int length);

    // 读取 / 回填 FC Header set 的 18 项文本。
    // 与基类接口等价，这里再提供一层以本类命名的入口，便于后续 FC 单独扩展。
    QStringList fcHeaderTexts() const;
    void setFcHeaderTexts(const QStringList &values);

protected:
    // 参数区域标题：FC 显示“FC Header set”。
    QString parameterSectionTitle() const override;
};
