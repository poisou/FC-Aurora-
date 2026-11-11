#pragma once

#include "devicework.h"

#include <QList>
#include <QString>

class QTcpServer;
class QTcpSocket;

// 以太网接收工作对象（TCP 服务端），每个以太网设备创建一个。
//
// 由 DeviceWork 通过 moveToThread 放入子线程运行：
// 监听本地地址和端口，接受客户端连接并读取收到的数据。
//
// 参数来源（由 DeviceWork 在 startWork() 之前设置）：
//   - 本地监听地址端口：DeviceWork::setTcpLocal("本地IP", 端口)
//
// 收到的每一笔数据都会通过继承自 DeviceWorkTask 的 dataReceived 信号
// 交给页面，写入 TCP 接收容器。
//
// ── 接收数据落盘接口（继承自 DeviceWorkTask）────────────────────
//   在 processData() 中可直接调用：
//     saveReceivedData(QByteArray)             // 按 QByteArray 保存
//     saveReceivedData(const char *, qint64)   // 按 char* + 长度保存
//   数据以二进制追加写入 可执行文件目录/SAVADATA/<年月日时分>_TCP.bin，
//   单文件上限默认 1 GB（可用 setReceiveSaveMaxBytes() 调整），超过自动分卷；
//   开关为 TCP_RECV_SAVE_ENABLED（定义在 devicework.h，默认开启）。
//   构造函数中已调用 setReceiveSaveBus(2) 指定文件名后缀 _TCP。
//   注意：页面收到 dataReceived 后也会落盘一次，两处只保留一处。
// ────────────────────────────────────────────────────────────────
//
// ── 接收工作类中的 send() 说明 ──────────────────────────────────
// 接收线程通常不需要发送数据，但基类 DeviceWorkTask 提供了统一的
// send(const QByteArray &) 接口，本类保留一个空实现，便于后续在接收
// 流程中向客户端回发 ACK、心跳或应答数据：
//   1. 在 processData() 中读取到需要应答的数据后，调用 send(数据);
//   2. send() 内部向已连接的客户端写入，成功 return true，失败 return false；
//   3. 当前为空实现，直接返回 true，不影响接收与上报流程。
// 服务端收到的数据按客户端连接整体读取，并 emit dataReceived()。
// ────────────────────────────────────────────────────────────────
class TcpRecvPro : public DeviceWorkTask
{
    Q_OBJECT

public:
    // @param ch 通道号，接收数据时作为“通道号”列显示。
    // @param address 本地 IP；为空时监听所有 IPv4 地址。
    // @param port 本地端口。
    TcpRecvPro(int ch, const QString &address, quint16 port);

    ~TcpRecvPro() override;

protected:
    // 每 20ms 调用一次：接受新连接 → 读取各客户端数据 → emit dataReceived。
    void processData() override;

    // 预留的发送接口（当前为空实现）。
    // @param data 需要回发的原始数据。
    // @return true 表示发送成功；空实现直接返回 true。
    bool send(const QByteArray &data) override;

private:
    // 确保服务端已开始监听。
    bool ensureListening();

    QTcpServer *m_server = nullptr;      // 监听对象，在子线程中创建和使用。
    QList<QTcpSocket *> m_clients;       // 已连接的客户端，断开后自动释放。
    QString m_address;                   // 本地监听 IP。
    quint16 m_port;                      // 本地监听端口。
};
