#pragma once

#include "devicework.h"

#include <QString>

// FC 接收工作对象，每个 FC 通道各创建一个。
//
// 由 DeviceWork 通过 moveToThread 放入该通道的独立线程运行。
// 收到数据后通过 dataReceived 信号交给页面写入 FC 接收容器。
//
// 二次开发：
//   在 processData() 中轮询/读取 FC 接收接口，一旦收到数据就调用
//     emit dataReceived(数据);
//   页面会把它 append 到 FC 接收容器，并由 300ms 定时器刷新到表格。
//
// ── 接收数据落盘接口（继承自 DeviceWorkTask）────────────────────
//   在 processData() 中可直接调用：
//     saveReceivedData(QByteArray)             // 按 QByteArray 保存
//     saveReceivedData(const char *, qint64)   // 按 char* + 长度保存
//   数据以二进制追加写入 可执行文件目录/SAVADATA/<年月日时分>_FC.bin，
//   单文件上限默认 1 GB（可用 setReceiveSaveMaxBytes() 调整），超过自动分卷；
//   开关为 FC_RECV_SAVE_ENABLED（定义在 devicework.h，默认开启）。
//   构造函数中已调用 setReceiveSaveBus(0) 指定文件名后缀 _FC。
//   注意：页面收到 dataReceived 后也会落盘一次，两处只保留一处。
// ────────────────────────────────────────────────────────────────
//
// ── 接收工作类中的 send() 说明 ──────────────────────────────────
// 接收线程通常不需要发送数据，但基类 DeviceWorkTask 提供了统一的
// send(const QByteArray &) 接口，本类保留一个空实现，便于后续在接收
// 流程中回发 ACK、心跳或应答数据：
//   1. 在 processData() 中读取到需要应答的数据后，调用 send(数据);
//   2. send() 内部按 FC 总线接口实际写入，成功 return true，失败 return false；
//   3. 当前为空实现，直接返回 true，不影响接收与上报流程。
// 接收数据切分由 FC 硬件/协议接口完成，收到后整体 emit dataReceived()。
// ────────────────────────────────────────────────────────────────
class FCRecvPro : public DeviceWorkTask
{
    Q_OBJECT

public:
    // @param ch 通道号，从 1 开始；读取时用于选择对应的 FC 通道。
    FCRecvPro(int ch);

protected:
    // 每 20ms 调用一次，需在其中读取并上报接收数据。
    void processData() override;

    // 预留的发送接口（当前为空实现）。
    // @param data 需要回发的原始数据。
    // @return true 表示发送成功；空实现直接返回 true。
    bool send(const QByteArray &data) override;
};
