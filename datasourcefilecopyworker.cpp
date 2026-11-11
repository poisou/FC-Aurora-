#include "datasourcefilecopyworker.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>

namespace {

// 每次最多复制 4 MB，既能提高吞吐量，也能及时响应取消请求。
constexpr qint64 CopyBufferSize = 4 * 1024 * 1024;

} // namespace

DataSourceFileCopyWorker::DataSourceFileCopyWorker(QObject *parent)
    : QObject(parent)
    , m_cancelRequested(0)
{
}

void DataSourceFileCopyWorker::requestCancel()
{
    m_cancelRequested.storeRelease(1);
}

void DataSourceFileCopyWorker::copyFile(const QString &sourcePath,
                                        const QString &destinationPath)
{
    // 窗口可能在任务真正开始前就已经请求取消。
    if (m_cancelRequested.loadAcquire() != 0) {
        emit cancelled();
        return;
    }

    QFile source(sourcePath);
    if (!source.open(QIODevice::ReadOnly)) {
        emit failed(QStringLiteral("无法打开源文件：%1").arg(source.errorString()));
        return;
    }

    // 目标目录理论上已由主线程创建，这里再次检查以防目录被外部删除。
    const QFileInfo destinationInfo(destinationPath);
    if (!QDir().mkpath(destinationInfo.absolutePath())) {
        emit failed(QStringLiteral("无法创建 MYDATA 目录。"));
        return;
    }

    QFile destination(destinationPath);
    if (destination.exists()) {
        emit failed(QStringLiteral("目标文件已存在：%1").arg(destinationPath));
        return;
    }
    // Qt 5.9 没有 QIODevice::NewOnly；前面已检查文件不存在，
    // 目标名称由主线程生成且保证唯一，因此这里使用截断写入即可。
    if (!destination.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        emit failed(QStringLiteral("无法创建目标文件：%1")
                        .arg(destination.errorString()));
        return;
    }

    QByteArray buffer(static_cast<int>(CopyBufferSize), Qt::Uninitialized);
    while (!source.atEnd()) {
        if (m_cancelRequested.loadAcquire() != 0) {
            destination.close();
            destination.remove();
            emit cancelled();
            return;
        }

        const qint64 readSize = source.read(buffer.data(), buffer.size());
        if (readSize < 0) {
            const QString error = source.errorString();
            destination.close();
            destination.remove();
            emit failed(QStringLiteral("读取源文件失败：%1").arg(error));
            return;
        }
        if (readSize == 0) {
            break;
        }

        qint64 writtenTotal = 0;
        while (writtenTotal < readSize) {
            const qint64 written = destination.write(
                buffer.constData() + writtenTotal, readSize - writtenTotal);
            if (written <= 0) {
                const QString error = destination.errorString();
                destination.close();
                destination.remove();
                emit failed(QStringLiteral("写入目标文件失败：%1").arg(error));
                return;
            }
            writtenTotal += written;
        }
    }

    if (!destination.flush()) {
        const QString error = destination.errorString();
        destination.close();
        destination.remove();
        emit failed(QStringLiteral("刷新目标文件失败：%1").arg(error));
        return;
    }

    destination.close();
    emit finished(QFileInfo(destinationPath).absoluteFilePath());
}
