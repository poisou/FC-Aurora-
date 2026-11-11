#pragma once

#include "mytools/binfileimporterbase.h"

// TCP 专用 BIN 文件流式解析器。
//
// 每包结构：6 字节包头 + n 字节数据段。
// n 取自包头最后两个字节，统一由 BIN_HEADER_BIG_ENDIAN 决定字节序，
// 并且必须小于等于 1024。
class TcpFileImporter : public BinFileImporterBase
{
    Q_OBJECT

public:
    // 构造 TCP 文件导入器。
    // @param parent Qt 父对象。
    explicit TcpFileImporter(QObject *parent = nullptr);

public slots:
    // 流式解析 TCP 文件。
    // @param filePath 待解析文件路径。
    void importFile(const QString &filePath) override;
};
