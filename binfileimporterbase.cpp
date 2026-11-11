#include "binfileimporterbase.h"

#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>

BinFileImporterBase::BinFileImporterBase(QObject *parent)
    : QObject(parent)
    , m_cancelRequested(0)
{
    // 保证所有独立导入器实例都注册跨线程信号所需的元类型。
    qRegisterMetaType<BinPacket>("BinPacket");
    qRegisterMetaType<QVector<BinPacket>>("QVector<BinPacket>");
}

void BinFileImporterBase::requestCancel()
{
    m_cancelRequested.storeRelease(1);
}

// 通用逐包扫描循环。
//
// 这里不包含任何协议细节：包头长度、n 的解析、数据段长度都由
// 派生类的 parsePacket() 决定。因此更换某个总线的解包算法时，
// 只需修改该总线的 parsePacket()，本循环、进度上报和界面显示都不受影响。
void BinFileImporterBase::importFile(const QString &filePath)
{
    // 注意：这里不能调用 resetCancellation()。
    // 用户在导入线程真正开始执行前就可能点击“取消导入”，
    // 若在此清空取消标志，这次取消会被吞掉，界面会一直停在
    // “正在取消”且“选择文件”等按钮不可用。
    // 每个导入器都是新建后一次性使用，构造时标志已经是 0。
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        emit failed(QStringLiteral("无法打开文件：%1").arg(file.errorString()));
        return;
    }

    const qint64 fileSize = file.size();
    if (fileSize <= 0) {
        emit failed(QStringLiteral("文件为空。"));
        return;
    }

    QVector<BinPacket> packets;
    QElapsedTimer progressTimer;
    progressTimer.start();

    qint64 offset = 0;
    int packetIndex = 0;
    while (offset < fileSize) {
        if (isCancellationRequested()) {
            emit cancelled();
            return;
        }

        BinPacket packet;
        QString error;
        if (!parsePacket(file,
                         offset,
                         packetIndex,
                         fileSize - offset,
                         packet,
                         &error)) {
            emit failed(error);
            return;
        }

        packets.append(packet);
        offset += packet.packetSize();
        ++packetIndex;

        // 每 100ms 上报一次进度，避免信号过于频繁影响界面。
        if (progressTimer.elapsed() >= 100) {
            emit progress(offset, fileSize, packets.size());
            progressTimer.restart();
        }
    }

    emit progress(fileSize, fileSize, packets.size());
    emit finished(packets, QFileInfo(filePath).absoluteFilePath(), fileSize);
}

void BinFileImporterBase::resetCancellation()
{
    m_cancelRequested.storeRelease(0);
}

bool BinFileImporterBase::isCancellationRequested() const
{
    return m_cancelRequested.loadAcquire() != 0;
}
