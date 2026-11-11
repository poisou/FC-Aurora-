#pragma once

#include "devicework.h"

#include <QString>

// Aurora 发送工作对象，每个 Aurora 通道各创建一个（共 8 个）。
//
// 由 DeviceWork 通过 moveToThread 放入该通道的独立线程运行：
// 按“6 字节包头 + n × 1024 字节数据段”从数据源读取整帧，再交给 send() 发送。
//
// 二次开发：
//   - 需要真正下发 Aurora 数据时，重写 send()，在其中调用 Aurora 总线接口；
//   - send() 返回 true 才前移读取位置，false 表示下一轮重发同一帧。
//
// 发送进度：本类继承 DeviceWorkTask 的 sendJD(sentBytes, totalBytes) 信号，
// 每成功发送一帧后由 reportSendProgress() 发出，页面据此刷新进度条。
//
// ── 发送数据流说明（二次开发必读）────────────────────────────────
// 1. 数据设置：在 GUI 线程、startWork() 之前调用
//      DeviceWork::setSourceFile("D:/MYDATA/xxx.bin");
//    文件由连续的“6 字节包头 + 数据段”组成。
//    Aurora 的数据段长度 = 包头最后两个字节 n × 1024 字节。
// 2. 线程循环：线程启动后 DeviceWorkTask::doWork() 每 20ms 调用一次
//    processData()。
// 3. 数据切分：processData() 内部调用
//      readNextPacket(frame, true);
//    从当前读取位置切出完整一帧（包头 + n × 1024 字节）放入 frame。
// 4. 调用发送：拿到 frame 后调用
//      send(frame);
//    send() 返回 true 时 processData() 才把 m_position 前移
//    frame.size()，并调用 reportSendProgress() 上报进度；
//    返回 false 时保持在原位置，下一轮重发同一帧。
// 5. 本类的 send() 目前是空实现（直接返回 true）。
//    接入真实 Aurora 总线时，只需在 aurorasendpro.cpp 的 send() 中把
//    data 交给硬件接口，并按“成功 true / 失败 false”返回即可，
//    切帧、重发、进度上报逻辑均不需要改动。
// ────────────────────────────────────────────────────────────────
class AuroraSendPro : public DeviceWorkTask
{
    Q_OBJECT

public:
    // @param ch 通道号，从 1 开始；8 个通道各自对应一个工作对象。
    // @param sourcePath 数据源文件绝对路径，由 DeviceWork::setSourceFile 传入。
    AuroraSendPro(int ch, const QString &sourcePath);

protected:
    // 每 20ms 调用一次：读一帧 → send() → 成功则前移位置。
    void processData() override;

    // 发送一帧数据（当前为空实现，便于后续接入 Aurora 总线）。
    // @param data 一帧完整数据：6 字节包头 + n × 1024 字节数据段。
    // @return true 表示发送成功；当前空实现返回 true 以便继续读帧。
    bool send(const QByteArray &data) override;
};
