#pragma once

#include "mytools/binfileimporterbase.h"

// FC 专用 BIN 文件流式解析器。
//
// 当前 FC 协议暂时与 TCP 相同：
// - 每包由 6 字节包头 + n 字节数据段组成；
// - n 取自包头最后两个字节，字节序由 BIN_HEADER_BIG_ENDIAN 决定；
// - n 必须小于等于 1024。
//
// 该类独立实现解析循环，后续 FC 协议变化时可直接修改本类，
// 不会影响独立的 TcpFileImporter。
class FcFileImporter : public BinFileImporterBase
{
    Q_OBJECT

public:
    // 构造 FC 文件导入器。
    // @param parent Qt 父对象。
    explicit FcFileImporter(QObject *parent = nullptr);

public slots:
    // 流式解析 FC 文件。
    // @param filePath 待解析文件路径。
    void importFile(const QString &filePath) override;
};
