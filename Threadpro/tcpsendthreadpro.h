#ifndef TCPSENDTHREADPRO_H
#define TCPSENDTHREADPRO_H

#include <QAtomicInt>
#include <QByteArray>
#include <QHostAddress>
#include <QMutex>
#include <QObject>
#include <QQueue>
#include <QDataStream>
#include <QTcpSocket>
#include <QThread>

class tcpsendthreadpro : public QObject
{
    Q_OBJECT
public:
    tcpsendthreadpro(qint64 ipriID,int ch,QHostAddress targetAddress,quint64 targetPort, QObject *parent);
    ~tcpsendthreadpro();
    void prepareToStart();// 线程重新启动前清除上一次的停止标志。
    void requestStop();// 请求停止发送线程。
public slots:
    /// 连接服务器、断线重连，并持续发送队列中的消息。
    void doWork();
signals:
    /// 一条消息成功写入 TCP 套接字后通知界面。
    void messageSent(const QByteArray &data, const QString &peer);
    /// 连接或发送错误日志。
    void errorOccurred(const QString &message);
    /// 连接、断开等普通日志。
    void logMessage(const QString &message);
    /// 发送线程退出信号。
    void finished();
private:
    /// 从队列取出一条消息；队列为空时返回 false。
    bool takeMessage(QByteArray &data);
private:
    qint64 mipriID;
    QTcpSocket m_socket;
    int mch;
    quint64 m_targetPort = 0;
    QString mSourecname;
    QHostAddress m_targetAddress; // 目标服务器地址。
    QAtomicInt m_running;         // 防止 doWork() 被重复执行。
    QAtomicInt m_stopRequested;   // 线程安全的停止标志。
    QMutex m_queueMutex;          // 保护发送队列。
    QQueue<QByteArray> m_queue;   // 待发送消息队列。
};

#endif // TCPSENDTHREADPRO_H
