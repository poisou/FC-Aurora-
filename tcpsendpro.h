#pragma once

#include "devicework.h"

#include <QString>

class QTcpSocket;

// 以太网发送工作对象（TCP 客户端），每个以太网设备创建一个。
//
// 由 DeviceWork 通过 moveToThread 放入子线程运行：
// 连接对端地址后，把数据源按“6 字节包头 + n 字节数据段”的整包写入套接字。
//
// 参数来源（由 DeviceWork 在 startWork() 之前设置）：
//   - 数据源路径：DeviceWork::setSourceFile(绝对路径)
//   - 对端地址端口：DeviceWork::setTcpTarget("对端IP", 端口)
//
// 发送策略：
//   - 未连接时自动 connectToHost，超时 1 秒；
//   - 写失败或超时则 abort() 断开，下一轮自动重连并重发同一包；
//   - 写入成功才向前移动读取位置。
//
// 发送进度：本类继承 DeviceWorkTask 的 sendJD(sentBytes, totalBytes) 信号，
// 每成功写出一包后由 reportSendProgress() 发出，页面据此刷新进度条。
//
// ── 发送数据流说明（二次开发必读）────────────────────────────────
// 1. 数据设置：在 GUI 线程、startWork() 之前调用
//      work->setSourceFile("D:/MYDATA/xxx.bin");
//      work->setTcpTarget("对端IP", 端口);
// 2. 线程循环：线程启动后 DeviceWorkTask::doWork() 每 20ms 调用一次
//    processData()。
// 3. 数据切分：processData() 内部调用
//      readNextPacket(packet, false);
//    按“6 字节包头 + n 字节数据段”切出完整一帧。
// 4. 调用发送：连接成功后调用
//      send(packet);
//    当前 send() 已实现为 QTcpSocket 整包写入（本类不是空实现）；
//    返回 true 时 processData() 前移读取位置并上报进度，
//    返回 false 时断开连接并在下一轮重连重发同一帧。
// 5. 如需改用其它发送方式，只需替换 send() 内部实现，
//    切帧、重连、重发和进度上报逻辑均不需要改动。
// ────────────────────────────────────────────────────────────────
class TcpSendPro : public DeviceWorkTask
{
    Q_OBJECT

public:
    // @param ch 通道号。
    // @param sourcePath 数据源文件路径，由工作线程按帧读取。
    // @param address 对端 IP；为空时不发送。
    // @param port 对端端口；为 0 时不发送。
    TcpSendPro(int ch,
               const QString &sourcePath,
               const QString &address,
               quint16 port);

    ~TcpSendPro() override;

protected:
    // 每 20ms 调用一次：确保连接 → 读一帧 → send()。
    void processData() override;

    // 把一帧数据整包写入套接字。
    // @param data 待发送的一帧（含 6 字节包头）。
    // @return true 写入并等待发送完成；false 已断开，下一轮重连重发。
    bool send(const QByteArray &data) override;

private:
    // 确保套接字已连接；未连接时尝试重连。
    bool ensureConnected();

    QTcpSocket *m_socket = nullptr; // 套接字在子线程中创建和使用。
    QString m_address;              // 对端 IP。
    quint16 m_port;                 // 对端端口。
};
