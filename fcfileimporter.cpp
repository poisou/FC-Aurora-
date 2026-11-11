#include "fcfileimporter.h"

#include <QFile>

FcFileImporter::FcFileImporter(QObject *parent)
    : BinFileImporterBase(parent)
{
}

// =====================================================================
// FC 单包解包算法（后续只改这里即可）
//
// 当前 FC 协议沿用 TCP 规则：6 字节包头 + n 字节数据段（n ≤ 1024）。
// 处理步骤：
//   1) 剩余长度不足 6 字节 → 数据不完整；
//   2) seek 到 offset 并读取 6 字节包头；
//   3) decodeDataCount() 解析 n（字节序见 binformat.h）；
//   4) TCP 规则中 n 直接表示数据段字节数；
//   5) 校验 n ≤ 1024 且剩余长度足够；
//   6) 只填充元数据，不读取数据段内容。
//
// 若 FC 协议与 TCP 不同（包头更长、数据段按其它公式计算），
// 修改本函数的第 2、4 步，并同步 binformat.h 的 HeaderSize。
// 通用循环、进度上报和表格显示都不需要改动。
// =====================================================================
bool FcFileImporter::parsePacket(QFile &file,
                                 qint64 offset,
                                 int packetIndex,
                                 qint64 remaining,
                                 BinPacket &packet,
                                 QString *error)
{
    if (remaining < BinFormat::HeaderSize) {
        if (error != nullptr) {
            *error = QStringLiteral(
                         "FC 第 %1 包数据不完整：偏移 %2 处不足 6 字节包头。")
                         .arg(packetIndex + 1)
                         .arg(offset);
        }
        return false;
    }

    if (!file.seek(offset)) {
        if (error != nullptr) {
            *error = QStringLiteral("FC 第 %1 包包头定位失败。")
                         .arg(packetIndex + 1);
        }
        return false;
    }

    const QByteArray header = file.read(BinFormat::HeaderSize);
    if (header.size() != BinFormat::HeaderSize) {
        if (error != nullptr) {
            *error = QStringLiteral("FC 第 %1 包包头读取失败。")
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
            *error = QStringLiteral("FC 第 %1 包 n=%2，超过最大允许值 1024。")
                         .arg(packetIndex + 1)
                         .arg(packet.dataCount);
        }
        return false;
    }

    // 当前 FC 暂时复用 TCP 的规则：n 直接表示数据段字节数。
    packet.payloadLength = packet.dataCount;

    if (packet.packetSize() > remaining) {
        if (error != nullptr) {
            *error = QStringLiteral(
                         "FC 第 %1 包数据不完整：n=%2，需要 %3 字节，"
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
