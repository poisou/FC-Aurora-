#include "aurorarecvpro.h"

AuroraRecvPro::AuroraRecvPro(int ch)
    : DeviceWorkTask(ch, DeviceWork::Receive, QString())
{
    // 接收数据落盘：可执行文件目录/SAVADATA/<年月日时分>_Aurora.bin
    // 由基类 saveReceivedData() 使用，开关为 AURORA_RECV_SAVE_ENABLED；
    // 单文件上限由 setReceiveSaveMaxBytes() 设置，默认 1 GB。
    setReceiveSaveBus(1);
}

void AuroraRecvPro::processData()
{
    // Aurora 接收接口待接入：收到数据后按需执行下面两步。
    //
    // 1) 保存为 .bin 文件（继承自 DeviceWorkTask 的落盘接口）：
    //      saveReceivedData(frame);            // QByteArray 版本
    //      saveReceivedData(buffer, length);   // char* + 长度版本
    //    位置 SAVADATA/<年月日时分>_Aurora.bin，单文件上限默认 1 GB，
    //    超过自动分卷，开关 AURORA_RECV_SAVE_ENABLED；
    //    上限可用 setReceiveSaveMaxBytes() 调整。
    //
    // 2) 上报界面表格：
    //      emit dataReceived(frame);
    //
    // 注意：场景并行页面在收到 dataReceived 时也会落盘一次，
    //       两处只保留一处，避免同一笔数据被写两遍。
}

bool AuroraRecvPro::send(const QByteArray &data)
{
    // ── Aurora 接收线程的回发接口（当前为空实现）────────────────
    // 如需在接收流程中回发 ACK/应答数据，在 processData() 里调用
    // send(数据)，并在本函数中把 data 交给 Aurora 总线发送接口：
    // 成功 return true，失败 return false。
    // 目前不做任何发送，直接返回 true，不影响接收上报。
    Q_UNUSED(data)
    return true;
}
