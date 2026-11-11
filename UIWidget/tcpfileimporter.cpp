#include "tcpfileimporter.h"

#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>

TcpFileImporter::TcpFileImporter(QObject *parent)
    : BinFileImporterBase(parent)
{
}

void TcpFileImporter::importFile(const QString &filePath)
{
    resetCancellation();

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        emit failed(QStringLiteral("无法打开 TCP 文件：%1").arg(file.errorString()));
        return;
    }

    const qint64 fileSize = file.size();
    if (fileSize <= 0) {
        emit failed(QStringLiteral("TCP 文件为空。"));
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

        const qint64 remaining = fileSize - offset;
        if (remaining < BinFormat::HeaderSize) {
            emit failed(
                QStringLiteral("TCP 第 %1 包数据不完整：偏移 %2 处不足 6 字节包头。")
                    .arg(packetIndex + 1)
                    .arg(offset));
            return;
        }

        if (!file.seek(offset)) {
            emit failed(
                QStringLiteral("TCP 第 %1 包包头定位失败。").arg(packetIndex + 1));
            return;
        }

        const QByteArray header = file.read(BinFormat::HeaderSize);
        if (header.size() != BinFormat::HeaderSize) {
            emit failed(
                QStringLiteral("TCP 第 %1 包包头读取失败。").arg(packetIndex + 1));
            return;
        }

        BinPacket packet;
        packet.index = packetIndex;
        packet.offset = offset;
        packet.dataCount = BinFormat::decodeDataCount(header);
        packet.header = header;

        if (packet.dataCount > 1024) {
            emit failed(
                QStringLiteral("TCP 第 %1 包 n=%2，超过最大允许值 1024。")
                    .arg(packetIndex + 1)
                    .arg(packet.dataCount));
            return;
        }

        // TCP 中 n 直接表示数据段字节数。
        packet.payloadLength = packet.dataCount;
        if (packet.packetSize() > remaining) {
            emit failed(
                QStringLiteral("TCP 第 %1 包数据不完整：n=%2，需要 %3 字节，"
                               "当前只剩 %4 字节。")
                    .arg(packetIndex + 1)
                    .arg(packet.dataCount)
                    .arg(packet.packetSize())
                    .arg(remaining));
            return;
        }

        packets.append(packet);
        offset += packet.packetSize();
        ++packetIndex;

        if (progressTimer.elapsed() >= 100) {
            emit progress(offset, fileSize, packets.size());
            progressTimer.restart();
        }
    }

    emit progress(fileSize, fileSize, packets.size());
    emit finished(packets, QFileInfo(filePath).absoluteFilePath(), fileSize);
}
