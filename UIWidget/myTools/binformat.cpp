#include "binformat.h"

// =============================================================================
// BIN 格式通用工具实现
//
// 本文件只负责与 UI 无关的字节格式化，不进行文件读取和线程调度。
// =============================================================================

#include <QChar>

namespace {

// 将单个字节转换为两位大写十六进制文本。
// @param value 待格式化的单字节值。
// @return 固定两位的 HEX 文本，例如 0x0A 返回 "0A"。
QString byteToHex(char value)
{
    return QString::number(static_cast<quint8>(value), 16)
        .rightJustified(2, QLatin1Char('0'))
        .toUpper();
}

} // namespace

quint16 BinFormat::decodeDataCount(const QByteArray &header)
{
    // 包头至少需要 6 字节，最后两个字节分别位于索引 4 和 5。
    if (header.size() < HeaderSize) {
        return 0;
    }

    const quint16 byte4 = static_cast<quint8>(header.at(4));
    const quint16 byte5 = static_cast<quint8>(header.at(5));

#if BIN_HEADER_BIG_ENDIAN
    // 大端：先出现的高位字节在内存中位于前。
    return static_cast<quint16>((byte4 << 8) | byte5);
#else
    // 小端：先出现的低位字节在内存中位于前。
    return static_cast<quint16>(byte4 | (byte5 << 8));
#endif
}

// 将字节数组转换为带分隔符的 HEX 文本。
// 预留 3 倍长度是因为每个字节通常占用两个 HEX 字符和一个分隔符。
QString BinFormat::toHex(const QByteArray &data, char separator)
{
    // 预留足够的字符空间，减少频繁扩容。
    QString result;
    result.reserve(data.size() * 3);

    for (int index = 0; index < data.size(); ++index) {
        if (index > 0) {
            result.append(QLatin1Char(separator));
        }
        result.append(byteToHex(data.at(index)));
    }

    return result;
}

// 生成逐行 HEX / ASCII 文本。
//
// 处理步骤如下：
//   1. 每行输出从 0 开始的相对偏移；
//   2. 输出最多 bytesPerLine 个字节的 HEX；
//   3. 在第 8 个字节后增加一个额外空格，方便视觉分组；
//   4. 对每个字节生成 ASCII 区域，不可打印字符显示为 '.'。
QString BinFormat::toHexDump(const QByteArray &data, int bytesPerLine)
{
    if (data.isEmpty()) {
        return QString();
    }

    if (bytesPerLine <= 0) {
        bytesPerLine = 16;
    }

    QString result;
    result.reserve(data.size() * 4);

    // 每行依次输出偏移、HEX 字节和对应的可打印 ASCII 字符。
    for (int offset = 0; offset < data.size(); offset += bytesPerLine) {
        const int lineSize = qMin(bytesPerLine, data.size() - offset);
        result.append(QStringLiteral("%1  ")
                          .arg(offset, 8, 16, QLatin1Char('0'))
                          .toUpper());

        // ASCII 区域只追加当前行实际存在的字节。
        QString ascii;
        ascii.reserve(lineSize);

        for (int index = 0; index < bytesPerLine; ++index) {
            if (index < lineSize) {
                const uchar value = static_cast<uchar>(data.at(offset + index));
                result.append(byteToHex(static_cast<char>(value)));
                result.append(QLatin1Char(' '));
                // 32-126 是可打印 ASCII 范围，其余字符统一显示为点。
                ascii.append(value >= 32 && value <= 126 ? QChar(value) : QLatin1Char('.'));
            } else {
                result.append(QStringLiteral("   "));
            }

            if (index == 7) {
                // 每 8 字节插入额外空格，方便快速定位数据位置。
                result.append(QLatin1Char(' '));
            }
        }

        result.append(QStringLiteral(" |"));
        result.append(ascii);
        result.append(QStringLiteral("|\n"));
    }

    return result;
}
