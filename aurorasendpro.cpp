#include "aurorasendpro.h"

AuroraSendPro::AuroraSendPro(int ch, const QString &sourcePath)
    : DeviceWorkTask(ch, DeviceWork::Send, sourcePath)
{
}

void AuroraSendPro::processData()
{
    QByteArray frame;
    if (!readNextPacket(frame, true)) {
        return;
    }

    // 只有发送成功才前移位置，避免丢帧。
    // send() 的实际 Aurora 总线发送接口由后续实现补充。
    if (send(frame)) {
        m_position += frame.size();
        // 上报发送进度：sendJD(m_position, m_totalBytes)。
        reportSendProgress();
    }
}

bool AuroraSendPro::send(const QByteArray &data)
{
    // ── Aurora 总线发送接口接入位置 ──────────────────────────────
    // 当前为空实现，不做任何实际发送，直接返回 true 以便线程继续读帧。
    // 接入真实 Aurora 硬件后，把 data（6 字节包头 + n × 1024 字节数据段）
    // 写入对应通道的发送接口，并按以下约定返回：
    //   - 发送成功：return true;  processData() 会前移读取位置并上报进度；
    //   - 发送失败：return false; 下一轮重发同一帧。
    Q_UNUSED(data)
    return true;
}
