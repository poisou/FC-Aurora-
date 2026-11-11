#pragma once

// =============================================================================
// BIN 数据格式定义
//
// 文件由一个或多个连续数据包组成：
//   Aurora：6 字节包头 + n × 1024 字节数据段；
//   TCP：   6 字节包头 + n 字节数据段，n 最大 1024。
//
// 两种协议的 n 均由包头最后两个字节组成，字节序通过统一宏配置。
// =============================================================================

#include <QByteArray>
#include <QMetaType>
#include <QString>
#include <QVector>

// ============================================================================
// 包头字节序配置
//
// BIN_HEADER_BIG_ENDIAN = 1：
//   包头第 5、6 字节按大端序组成 n。
//   例如 03 FA -> n = 0x03FA = 1018。
//
// BIN_HEADER_BIG_ENDIAN = 0：
//   包头第 5、6 字节按小端序组成 n。
//   例如 FA 03 -> n = 0x03FA = 1018。
//
// 当前默认使用大端序。若设备协议使用小端序，只需将宏改为 0，
// Aurora 与 TCP 两个独立解析器会同时生效，无需分别修改。
// ============================================================================
#ifndef BIN_HEADER_BIG_ENDIAN
#define BIN_HEADER_BIG_ENDIAN 1
#endif

#if BIN_HEADER_BIG_ENDIAN != 0 && BIN_HEADER_BIG_ENDIAN != 1
#error "BIN_HEADER_BIG_ENDIAN must be 0 (little endian) or 1 (big endian)"
#endif

namespace BinFormat {

// 当前支持的总线数据包格式。
enum class Protocol {
    Aurora, // 6 字节包头 + n × 1024 字节数据段。
    TCP     // 6 字节包头 + n 字节数据段，n 最大 1024。
};

// 固定协议常量。
// HeaderSize：每个包的固定包头长度。
// PayloadSize：n 的一份数据块所对应的字节数。
// PacketsPerPage：界面每页最多显示的数据包数量。
constexpr int HeaderSize = 6;
constexpr int PayloadSize = 1024;
constexpr int PacketsPerPage = 10;

// 从 6 字节包头的最后两个字节读取 n。
//
// @param header 至少包含 6 个字节的包头数据。
// @return 根据 BIN_HEADER_BIG_ENDIAN 解码得到的 16 位无符号 n。
//
// 注意：
// - 大端模式读取 header[4] 作为高字节、header[5] 作为低字节；
// - 小端模式读取 header[4] 作为低字节、header[5] 作为高字节。
quint16 decodeDataCount(const QByteArray &header);

} // namespace BinFormat

// 一个数据包的轻量级元数据。
//
// 大文件导入时只为每个包保存该结构，不保存完整数据段内容。
// 数据段在需要显示或编辑时再根据 offset 从文件中按需读取。
struct BinPacket
{
    int index = 0;           // 包序号，从 0 开始，界面显示时会加 1。
    qint64 offset = 0;       // 该包在文件中的绝对起始偏移。
    quint16 dataCount = 0;   // 包头最后两个字节按当前字节序宏解析出的 n。
    qint64 payloadLength = 0; // 当前协议下的实际数据段字节数。
    QByteArray header;        // 固定的 6 字节包头原始内容。

    // 计算数据段字节数。
    // @return Aurora 为 n × 1024，TCP 为 n。
    qint64 payloadSize() const
    {
        return payloadLength;
    }

    // 计算整个包的字节数：6 字节包头 + 数据段。
    qint64 packetSize() const
    {
        return BinFormat::HeaderSize + payloadSize();
    }
};

namespace BinFormat {

// 将二进制数据转换为连续 HEX 文本。
//
// @param data       待转换的原始字节。
// @param separator  字节之间使用的分隔字符，默认是空格。
// @return           例如 {0x0A, 0xFF} 返回 "0A FF"。
QString toHex(const QByteArray &data, char separator = ' ');

// 生成传统的逐行 HEX / ASCII 文本。
//
// 每行格式为：
//   00000000  00 01 02 ... 0F  |................|
//
// @param data         待格式化的字节。
// @param bytesPerLine 每行显示字节数，默认 16。
QString toHexDump(const QByteArray &data, int bytesPerLine = 16);

} // namespace BinFormat

// 注册自定义类型，使 BinPacket 和 QVector<BinPacket> 可以安全跨越线程，
// 通过 Qt 的 queued connection 从导入线程发送到主线程。
Q_DECLARE_METATYPE(BinPacket)
Q_DECLARE_METATYPE(QVector<BinPacket>)
