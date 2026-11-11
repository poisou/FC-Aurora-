#include "fcsendpro.h"

FCSendPro::FCSendPro(int ch, const QString &sourcePath)
    : DeviceWorkTask(ch, DeviceWork::Send, sourcePath)
{
}

void FCSendPro::processData()
{
    QByteArray packet;
    if (!readNextPacket(packet, false)) {
        return;
    }

    // 只有发送成功才前移位置，失败时下一轮重发同一包。
    // send() 的实际 FC 总线发送接口由后续实现补充。
    if (send(packet)) {
        m_position += packet.size();
        // 上报发送进度：sendJD(m_position, m_totalBytes)。
        reportSendProgress();
    }
}

bool FCSendPro::send(const QByteArray &data)
{
    // ── FC 总线发送接口接入位置 ──────────────────────────────────
    // 当前为空实现，不做任何实际发送，直接返回 true 以便线程继续读帧。
    // 接入真实 FC 硬件后，把 data（6 字节包头 + n 字节数据段）写入
    // 对应通道的发送接口，并按以下约定返回：
    //   - 发送成功：return true;  processData() 会前移读取位置并上报进度；
    //   - 发送失败：return false; 下一轮重发同一帧。
    Q_UNUSED(data)
    return true;
}
