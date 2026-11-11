#include "aurorafileimporter.h"

#include <QFile>

AuroraFileImporter::AuroraFileImporter(QObject *parent)
    : BinFileImporterBase(parent)
{
}

// =====================================================================
// Aurora 单包解包算法（后续只改这里即可）
//
// 格式：6 字节包头 + n × 1024 字节数据段。
// 处理步骤：
//   1) 剩余长度不足 6 字节 → 数据不完整；
//   2) seek 到 offset 并读取 HeaderSize(6) 字节包头；
//   3) decodeDataCount() 解析 n（字节序见 binformat.h）；
//   4) payloadLength = n × PayloadSize(1024)；
//   5) 校验剩余长度 ≥ 6 + payloadLength；
//   6) 只填充元数据（offset / dataCount / payloadLength / header），
//      不读取数据段内容——大文件不占内存的关键。
//
// 若 Aurora 协议变化（包头变长、数据段不再是 n×1024），
// 修改本函数的第 2、4 步，并同步 binformat.h 的 HeaderSize/PayloadSize。
// 通用的逐包循环、取消检查、进度上报都在基类 importFile() 中，
// 表格显示在界面层完成，均无需改动。
// =====================================================================
bool AuroraFileImporter::parsePacket(QFile &file,
                                     qint64 offset,
                                     int packetIndex,
                                     qint64 remaining,
                                     BinPacket &packet,
                                     QString *error)
{
    if (remaining < BinFormat::HeaderSize) {
        if (error != nullptr) {
            *error = QStringLiteral(
                         "Aurora 第 %1 包数据不完整：偏移 %2 处不足 6 字节包头。")
                         .arg(packetIndex + 1)
                         .arg(offset);
        }
        return false;
    }

    if (!file.seek(offset)) {
        if (error != nullptr) {
            *error = QStringLiteral("Aurora 第 %1 包包头定位失败。")
                         .arg(packetIndex + 1);
        }
        return false;
    }

    const QByteArray header = file.read(BinFormat::HeaderSize);
    if (header.size() != BinFormat::HeaderSize) {
        if (error != nullptr) {
            *error = QStringLiteral("Aurora 第 %1 包包头读取失败。")
                         .arg(packetIndex + 1);
        }
        return false;
    }

    packet.index = packetIndex;
    packet.offset = offset;
    packet.dataCount = BinFormat::decodeDataCount(header);
    packet.header = header;
    packet.payloadLength =
        static_cast<qint64>(packet.dataCount) * BinFormat::PayloadSize;

    if (packet.packetSize() > remaining) {
        if (error != nullptr) {
            *error = QStringLiteral(
                         "Aurora 第 %1 包数据不完整：n=%2，需要 %3 字节，"
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
