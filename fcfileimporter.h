#pragma once

#include "binfileimporterbase.h"

class QFile;

// FC 专用 BIN 文件流式解析器。
//
// 当前 FC 协议暂时与 TCP 相同：
// - 每包由 6 字节包头 + n 字节数据段组成；
// - n 取自包头最后两个字节，字节序由 BIN_HEADER_BIG_ENDIAN 决定；
// - n 必须小于等于 1024。
//
// ★ 后续 FC 协议变化时只需修改本类的 parsePacket()，
//   不会影响 TcpFileImporter，也不影响界面表格显示。
class FcFileImporter : public BinFileImporterBase
{
    Q_OBJECT

public:
    // 构造 FC 文件导入器。
    // @param parent Qt 父对象。
    explicit FcFileImporter(QObject *parent = nullptr);

protected:
    // FC 单包解包：6 字节包头 + n 字节数据段。
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
