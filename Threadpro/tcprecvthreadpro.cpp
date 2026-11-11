#include "tcprecvthreadpro.h"


namespace {
// 单条 TCP 消息最大允许 32 MB，防止异常长度导致过大的内存分配。
const quint32 MaxTcpMessageSize = 32u * 1024u * 1024u;
}

tcprecvthreadpro::tcprecvthreadpro(qint64 ipriID,int ch,QHostAddress targetAddress,quint64 targetPort, QObject *parent)
    : QObject(parent),
      m_running(0),
      m_stopRequested(0)
{

}

tcprecvthreadpro::~tcprecvthreadpro()
{

}

void tcprecvthreadpro::setLocalPort(quint16 localPort)
{
    // 保存界面传入的 TCP 监听端口。
    m_localPort = localPort;
}

void tcprecvthreadpro::prepareToStart()
{
    // 线程重新启动前清除旧停止标志。
    m_stopRequested.storeRelease(0);
}

void tcprecvthreadpro::requestStop()
{
    // 使用原子操作设置停止标志，允许界面线程直接调用。
    m_stopRequested.storeRelease(1);
}

void tcprecvthreadpro::doWork()
{
    // 第一步：防止同一个接收对象重复启动 doWork()。
    if (!m_running.testAndSetOrdered(0, 1)) {
        return;
    }

    // 第二步：在子线程中创建 TCP 服务器对象。
    // QTcpServer 及后续接受的客户端套接字都只能在本线程内访问。
    QTcpServer server;

    // 第三步：监听所有 IPv4 网卡上的指定端口。
    if (!server.listen(QHostAddress::AnyIPv4, m_localPort)) {

        // 监听失败时通知界面，复位状态并结束接收线程。
        emit errorOccurred(QString("TCP receive listen failed: %1")
                           .arg(server.errorString()));
        m_running.storeRelease(0);
        emit finished();
        return;
    }

    // 第四步：监听成功后记录日志。
    emit logMessage(QString("TCP receiver listening on 0.0.0.0:%1")
                    .arg(m_localPort));

    // 第五步：准备嵌套事件循环。
    // doWork() 本身没有返回，因此使用局部事件循环处理 QTcpServer 和套接字信号。
    QEventLoop eventLoop;

    // 第六步：定时检查停止标志，收到停止请求后退出嵌套事件循环。
    QTimer stopTimer;

    // 第七步：为每个客户端保存独立的接收缓冲区，解决 TCP 粘包和拆包问题。
    QHash<QTcpSocket *, QByteArray> receiveBuffers;

    // 第八步：每 50 毫秒检查一次停止标志。
    connect(&stopTimer, &QTimer::timeout, this, [this, &eventLoop]() {
        if (m_stopRequested.loadAcquire()) {
            eventLoop.quit();
        }
    });

    // 第九步：处理新客户端连接。
    connect(&server, &QTcpServer::newConnection, this,
            [this, &server, &receiveBuffers]() {

        // 第十步：一次接受所有已经到达的挂起连接。
        while (server.hasPendingConnections()) {
            QTcpSocket *client = server.nextPendingConnection();

            // 第十一步：为新连接建立空缓冲区。
            receiveBuffers.insert(client, QByteArray());

            // 第十二步：记录客户端地址和端口。
            emit logMessage(QString("TCP client connected: %1:%2")
                            .arg(client->peerAddress().toString())
                            .arg(client->peerPort()));

            // 第十三步：处理该客户端收到的数据。
            connect(client, &QTcpSocket::readyRead, this,
                    [this, &receiveBuffers, client]() {

                // 第十四步：把新到达的字节追加到该客户端自己的缓冲区。
                QByteArray &buffer = receiveBuffers[client];
                buffer.append(client->readAll());

                // 第十五步：只要缓冲区至少有 4 字节，就尝试解析长度前缀。
                while (buffer.size() >= 4) {

                    // 第十六步：按大端序读取 4 字节消息长度。
                    QDataStream header(buffer.left(4));
                    header.setByteOrder(QDataStream::BigEndian);

                    quint32 messageSize = 0;
                    header >> messageSize;

                    // 第十七步：长度超过上限时断开客户端，避免恶意分配内存。
                    if (messageSize > MaxTcpMessageSize) {
                        emit errorOccurred(QString("TCP message from %1:%2 is too large.")
                                           .arg(client->peerAddress().toString())
                                           .arg(client->peerPort()));
                        client->abort();
                        return;
                    }

                    // 第十八步：计算完整帧长度。数据不足时等待下一次 readyRead。
                    const int frameSize = 4 + static_cast<int>(messageSize);
                    if (buffer.size() < frameSize) {
                        break;
                    }

                    // 第十九步：从缓冲区提取消息内容，不包含长度前缀。
                    const QByteArray message =
                            buffer.mid(4, static_cast<int>(messageSize));

                    // 第二十步：只移除当前完整帧，保留后续消息的字节。
                    buffer.remove(0, frameSize);

                    // 第二十一步：把完整消息和客户端信息发送给界面。
                    emit messageReceived(message,
                                         QString("%1:%2")
                                         .arg(client->peerAddress().toString())
                                         .arg(client->peerPort()));
                }
            });

            // 第二十二步：客户端断开时移除对应缓冲区并释放套接字。
            connect(client, &QTcpSocket::disconnected, this,
                    [this, &receiveBuffers, client]() {
                emit logMessage(QString("TCP client disconnected: %1:%2")
                                .arg(client->peerAddress().toString())
                                .arg(client->peerPort()));
                receiveBuffers.remove(client);

                // 使用 deleteLater()，由当前线程的事件循环安全释放对象。
                client->deleteLater();
            });

            // 第二十三步：把客户端套接字错误转发到界面日志。
            connect(client,
                    static_cast<void (QAbstractSocket::*)(QAbstractSocket::SocketError)>(
                        &QAbstractSocket::error),
                    this,
                    [this, client](QAbstractSocket::SocketError) {
                emit errorOccurred(QString("TCP client socket error: %1")
                                   .arg(client->errorString()));
            });
        }
    });

    // 第二十四步：启动停止检查定时器，并进入局部事件循环。
    stopTimer.start(50);
    eventLoop.exec();

    // 第二十五步：退出事件循环后停止定时器并关闭服务器。
    stopTimer.stop();
    server.close();

    // 第二十六步：关闭停止时仍然保持连接的客户端，并回收套接字。
    const QList<QTcpSocket *> clients = receiveBuffers.keys();
    for (QTcpSocket *client : clients) {
        disconnect(client, Q_NULLPTR, this, Q_NULLPTR);
        client->abort();
        delete client;
    }
    receiveBuffers.clear();

    // 第二十七步：复位运行状态，并通知主窗口结束接收线程。
    m_running.storeRelease(0);
    emit logMessage("TCP receiver stopped.");
    emit finished();
}
