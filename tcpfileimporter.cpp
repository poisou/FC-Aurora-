#include "tcpfileimporter.h"

#include <QFile>

TcpFileImporter::TcpFileImporter(QObject *parent)
    : BinFileImporterBase(parent)
{
}

// =====================================================================
// 以太网（TCP）单包解包算法（后续只改这里即可）
//
// 格式：6 字节包头 + n 字节数据段（n ≤ 1024）。
// 处理步骤：
//   1) 剩余长度不足 6 字节 → 数据不完整；
//   2) seek 到 offset 并读取 6 字节包头；
//   3) decodeDataCount() 解析 n（字节序见 binformat.h）；
//   4) TCP 规则中 n 直接表示数据段字节数；
//   5) 校验 n ≤ 1024 且剩余长度足够；
//   6) 只填充元数据，不读取数据段内容。
//
// 修改以太网解包算法时只改本函数，通用循环、进度上报和表格显示不变。
// =====================================================================
bool TcpFileImporter::parsePacket(QFile &file,
                                  qint64 offset,
                                  int packetIndex,
                                  qint64 remaining,
                                  BinPacket &packet,
                                  QString *error)
{
    if (remaining < BinFormat::HeaderSize) {
        if (error != nullptr) {
            *error = QStringLiteral(
                         "TCP 第 %1 包数据不完整：偏移 %2 处不足 6 字节包头。")
                         .arg(packetIndex + 1)
                         .arg(offset);
        }
        return false;
    }

    if (!file.seek(offset)) {
        if (error != nullptr) {
            *error = QStringLiteral("TCP 第 %1 包包头定位失败。")
                         .arg(packetIndex + 1);
        }
        return false;
    }

    const QByteArray header = file.read(BinFormat::HeaderSize);
    if (header.size() != BinFormat::HeaderSize) {
        if (error != nullptr) {
            *error = QStringLiteral("TCP 第 %1 包包头读取失败。")
                         .arg(packetIndex + 1);
        }
        return false;
    }

    packet.index = packetIndex;
    packet.offset = offset;
    packet.dataCount = BinFormat::decodeDataCount(header);
    packet.header = header;

    if (packet.dataCount > 1024) {
        if (error != nullptr) {
            *error = QStringLiteral("TCP 第 %1 包 n=%2，超过最大允许值 1024。")
                         .arg(packetIndex + 1)
                         .arg(packet.dataCount);
        }
        return false;
    }

    // TCP 中 n 直接表示数据段字节数。
    packet.payloadLength = packet.dataCount;

    if (packet.packetSize() > remaining) {
        if (error != nullptr) {
            *error = QStringLiteral(
                         "TCP 第 %1 包数据不完整：n=%2，需要 %3 字节，"
                         "当前只剩 %4 字节。")
                         .arg(packetIndex + 1)
                         .arg(packet.dataCount)
                         .arg(packet.packetSize())
                         .arg(remaining);
        }
        return false;
    }

    return true;
}
