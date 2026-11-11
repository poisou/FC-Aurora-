#pragma once

#include "mytools/binfileimporterbase.h"

// Aurora 专用 BIN 文件流式解析器。
//
// 每包结构：6 字节包头 + n × 1024 字节数据段。
// n 取自包头最后两个字节，统一由 BIN_HEADER_BIG_ENDIAN 决定字节序。
class AuroraFileImporter : public BinFileImporterBase
{
    Q_OBJECT

public:
    // 构造 Aurora 文件导入器。
    // @param parent Qt 父对象。
    explicit AuroraFileImporter(QObject *parent = nullptr);

public slots:
    // 流式解析 Aurora 文件。
    // @param filePath 待解析文件路径。
    void importFile(const QString &filePath) override;
};
