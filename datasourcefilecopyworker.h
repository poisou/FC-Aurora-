#pragma once

#include <QAtomicInt>
#include <QObject>
#include <QString>

// 数据源文件后台复制器。
//
// 该对象被移动到独立 QThread 中运行，按固定大小分块复制文件。
// 复制大文件时主线程仍可处理界面事件，并可通过 requestCancel()
// 请求中止复制。取消或失败时会删除目标路径下的残留文件。
class DataSourceFileCopyWorker : public QObject
{
    Q_OBJECT

public:
    // 构造数据源文件复制器。
    // @param parent Qt 父对象；通常不指定，以便 moveToThread()。
    explicit DataSourceFileCopyWorker(QObject *parent = nullptr);

    // 线程安全地请求取消当前复制任务。
    void requestCancel();

public slots:
    // 将源文件分块复制到目标路径。
    // @param sourcePath 用户选择的原始文件绝对路径。
    // @param destinationPath MYDATA 目录中的目标文件绝对路径。
    void copyFile(const QString &sourcePath, const QString &destinationPath);

signals:
    // 文件复制成功。
    // @param destinationPath 已复制完成的文件绝对路径。
    void finished(const QString &destinationPath);

    // 文件复制失败。
    // @param message 可直接展示给用户的错误信息。
    void failed(const QString &message);

    // 用户关闭程序或取消任务，文件复制已中止。
    void cancelled();

private:
    // 取消标志：0 表示继续复制，1 表示请求取消。
    QAtomicInt m_cancelRequested;
};
