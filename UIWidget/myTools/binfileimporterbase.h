#pragma once

#include "binformat.h"

#include <QAtomicInt>
#include <QObject>
#include <QString>
#include <QVector>

// BIN 文件导入器公共接口。
//
// 该基类只提供取消标志和统一信号，不包含任何协议解析逻辑。
// Aurora 和 TCP 的包头长度规则、数据段长度规则分别由派生类实现。
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
    // 扫描指定文件。
    // @param filePath 待扫描的 BIN 文件路径。
    virtual void importFile(const QString &filePath) = 0;

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
