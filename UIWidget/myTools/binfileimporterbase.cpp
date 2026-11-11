#include "binfileimporterbase.h"

BinFileImporterBase::BinFileImporterBase(QObject *parent)
    : QObject(parent)
    , m_cancelRequested(0)
{
    // BinPacket 和 QVector<BinPacket> 会通过 queued signal 从工作线程
    // 发送到界面线程。必须在任何导入任务启动前注册元类型，否则
    // finished(...) 无法跨线程投递，界面会一直停留在“解析中…”。
    qRegisterMetaType<BinPacket>("BinPacket");
    qRegisterMetaType<QVector<BinPacket>>("QVector<BinPacket>");
}

void BinFileImporterBase::requestCancel()
{
    m_cancelRequested.storeRelease(1);
}

void BinFileImporterBase::resetCancellation()
{
    m_cancelRequested.storeRelease(0);
}

bool BinFileImporterBase::isCancellationRequested() const
{
    return m_cancelRequested.loadAcquire() != 0;
}
