// 对应界面：复用 auroraframeeditdialog.ui。
// 本文件实现以太网（TCP）专用的数据帧编辑对话框。
#include "tcpframeeditdialog.h"

TcpFrameEditDialog::TcpFrameEditDialog(QWidget *parent)
    : AuroraFrameEditDialog(parent)
{
    // 以太网窗口固定使用 Tcp 模式：显示 PW/PRI/TOA 参数行与初始值/步进行，
    // 标题由 parameterSectionTitle() 定制为以太网自己的文字。
    setBusMode(BusMode::Tcp);

    setWindowTitle(QStringLiteral("数据帧信息"));
}

TcpFrameEditDialog::~TcpFrameEditDialog() = default;

void TcpFrameEditDialog::addTcpDataParameter(const QString &name,
                                             DataParameterUnit unit,
                                             int startByte,
                                             int length)
{
    // 复用基类的“参数字段显示”实现（取值为 10 进制，位置/长度可配置）。
    addDataParameter(name, unit, startByte, length, false);
}

void TcpFrameEditDialog::addTcpDataSequenceParameter(const QString &name,
                                                     DataParameterUnit unit,
                                                     int startByte,
                                                     int length)
{
    // 复用基类的“初始值/步进”字段实现（追加到 TOA 之后）。
    addSequenceParameter(name, unit, startByte, length);
}

QString TcpFrameEditDialog::parameterSectionTitle() const
{
    return QStringLiteral("以太网参数设置");
}
