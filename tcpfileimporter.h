#pragma once

#include "binfileimporterbase.h"

class QFile;

// TCP 专用 BIN 文件流式解析器。
//
// 每包结构：6 字节包头 + n 字节数据段。
// n 取自包头最后两个字节，统一由 BIN_HEADER_BIG_ENDIAN 决定字节序，
// 并且必须小于等于 1024。
//
// ★ 修改以太网解包算法时只需改本类的 parsePacket()。
class TcpFileImporter : public BinFileImporterBase
{
    Q_OBJECT

public:
    // 构造 TCP 文件导入器。
    // @param parent Qt 父对象。
    explicit TcpFileImporter(QObject *parent = nullptr);

protected:
    // TCP 单包解包：6 字节包头 + n 字节数据段。
    // @param file 已打开的文件。
    // @param offset 当前包起始偏移。
    // @param packetIndex 包序号，从 0 开始。
    // @param remaining 文件剩余字节数。
    // @param packet 输出：解析出的包元数据。
    // @param error 输出：失败原因。
    // @return true 表示解析成功。
    bool parsePacket(QFile &file,
                     qint64 offset,
                     int packetIndex,
                     qint64 remaining,
                     BinPacket &packet,
                     QString *error) override;
};
