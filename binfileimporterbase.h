#pragma once

#include "binformat.h"

#include <QAtomicInt>
#include <QObject>
#include <QString>
#include <QVector>

class QFile;

// BIN 文件导入器公共接口。
//
// 该基类统一实现“逐包扫描”的通用循环（打开文件、进度上报、取消、
// 结束信号），具体每个总线如何解包由派生类重写 parsePacket() 决定。
//
// ── 数据解包算法修改指引 ─────────────────────────────────────────
// 数据回看页的调用链：
//   loadFile() → handleBusTypeChanged() → beginImport()
//     → new 具体导入器 → moveToThread → importFile(文件路径)
//     → parsePacket()（每个总线一个函数，只在这里改解包算法）
//     → finished(QVector<BinPacket>) → handleImportFinished()
//     → populateCurrentPage() 显示到数据浏览表格
//
// 每个总线只需实现下面一个函数：
//   AuroraFileImporter::parsePacket()  （6 字节包头 + n × 1024 字节）
//   FcFileImporter::parsePacket()      （6 字节包头 + n 字节）
//   TcpFileImporter::parsePacket()     （6 字节包头 + n 字节）
// 包头长度、块大小、字节序常量统一在 binformat.h 中修改。
// ────────────────────────────────────────────────────────────────
class BinFileImporterBase : public QObject
{
    Q_OBJECT

public:
    // 构造导入器。
    // @param parent Qt 父对象。
    explicit BinFileImporterBase(QObject *parent = nullptr);

    // 线程安全地请求取消当前扫描。
    void requestCancel();

public slots:
    // 扫描指定文件（在导入线程中执行）。
    // 本函数只负责通用的逐包循环，不包含任何协议规则：
    //   打开文件 → 循环调用 parsePacket() → 记录 BinPacket 元数据
    //   → 定期 emit progress → 结束 emit finished / failed。
    // 修改解包算法时不需要改这里，只改派生类的 parsePacket()。
    // @param filePath 待扫描的 BIN 文件路径。
    void importFile(const QString &filePath);

protected:
    // 解析一个数据包（每个总线各自的解包算法都在这里实现）。
    //
    // 实现要求：
    //   1. 从 offset 处读取包头；
    //   2. 解析 n 并计算数据段长度 payloadLength；
    //   3. 校验 remaining 是否足够容纳整包；
    //   4. 只填充 BinPacket 的元数据，不读取数据段内容；
    //   5. 成功返回 true；失败时写 error 并返回 false。
    //
    // @param file 已打开的 BIN 文件。
    // @param offset 当前包起始偏移。
    // @param packetIndex 包序号，从 0 开始。
    // @param remaining 文件剩余字节数（含当前包头）。
    // @param packet 输出参数：解析出的包元数据。
    // @param error 输出参数：失败原因，供界面弹窗显示。
    // @return true 表示解析成功。
    virtual bool parsePacket(QFile &file,
                             qint64 offset,
                             int packetIndex,
                             qint64 remaining,
                             BinPacket &packet,
                             QString *error) = 0;

signals:
    // 扫描进度。
    // @param processedBytes 已扫描字节数。
    // @param totalBytes 文件总字节数。
    // @param packetCount 已识别包数。
    void progress(qint64 processedBytes, qint64 totalBytes, int packetCount);

    // 扫描成功。
    // @param packets 数据包元数据。
    // @param filePath 文件绝对路径。
    // @param fileSize 文件字节大小。
    void finished(QVector<BinPacket> packets, const QString &filePath, qint64 fileSize);

    // 扫描失败。
    // @param message 错误信息。
    void failed(const QString &message);

    // 用户取消扫描。
    void cancelled();

protected:
    // 重置取消标志，供每次新扫描开始时调用。
    void resetCancellation();

    // 查询是否已请求取消。
    // @return 已请求取消时返回 true。
    bool isCancellationRequested() const;

private:
    QAtomicInt m_cancelRequested;
};
