#ifndef TCPRECVTHREADPRO_H
#define TCPRECVTHREADPRO_H

#include <QAbstractSocket>
#include <QDataStream>
#include <QEventLoop>
#include <QHash>
#include <QHostAddress>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTimer>
#include <QAtomicInt>
#include <QByteArray>
#include <QObject>
#include "../UIWidget/datadefine.h"

class tcprecvthreadpro : public QObject
{
    Q_OBJECT
public:
    tcprecvthreadpro(qint64 ipriID,int ch,QHostAddress targetAddress,quint64 targetPort, QObject *parent);
    ~tcprecvthreadpro();
    /// 设置本地 TCP 监听端口，必须在工作线程启动前调用。
    void setLocalPort(quint16 localPort);

    /// 线程重新启动前清除上一次的停止标志。
    void prepareToStart();

    /// 请求停止；停止定时器最多 50 毫秒后结束事件循环。
    void requestStop();
signals:
    /// 收到一条经过长度分帧的完整 TCP 消息后发送给界面。
    void messageReceived(const QByteArray &data, const QString &peer);

    /// 客户端连接、断开等普通日志。
    void logMessage(const QString &message);

    /// 监听或客户端套接字错误。
    void errorOccurred(const QString &message);

    /// 接收线程退出信号。
    void finished();
public slots:
    /// 开始监听，并持续处理客户端连接及接收事件，直到收到停止请求。
    void doWork();
private:
    qint64 mipriID;
    int mch;//通道号
    quint16 m_localPort;        // 本地监听端口。
    QAtomicInt m_running;       // 防止 doWork() 被重复执行。
    QAtomicInt m_stopRequested; // 线程安全的停止标志。

};

#endif // TCPRECVTHREADPRO_H
