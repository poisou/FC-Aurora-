// 对应界面：复用 auroraframeeditdialog.ui。
// 本文件实现 FC 专用的数据帧编辑对话框。
#include "fcframeeditdialog.h"

FcFrameEditDialog::FcFrameEditDialog(QWidget *parent)
    : AuroraFrameEditDialog(parent)
{
    // FC 窗口固定使用 Fc 模式：显示 FC Header set 网格，
    // 隐藏 Aurora 的 PW/PRI/TOA 参数行与初始值/步进行。
    setBusMode(BusMode::Fc);

    setWindowTitle(QStringLiteral("数据帧信息"));
}

FcFrameEditDialog::~FcFrameEditDialog() = default;

void FcFrameEditDialog::addFcDataParameter(const QString &name,
                                           DataParameterUnit unit,
                                           int startByte,
                                           int length)
{
    // 复用基类的“参数字段显示”实现：
    // fcLayout = true 表示把标签与值追加到 FC Header set 网格下方。
    addDataParameter(name, unit, startByte, length, true);
}

QStringList FcFrameEditDialog::fcHeaderTexts() const
{
    return fcHeaderValues();
}

void FcFrameEditDialog::setFcHeaderTexts(const QStringList &values)
{
    setFcHeaderValues(values);
}

QString FcFrameEditDialog::parameterSectionTitle() const
{
    return QStringLiteral("FC Header set");
}
