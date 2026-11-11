#include "tcpsendthreadpro.h"

tcpsendthreadpro::tcpsendthreadpro(qint64 ipriID,int ch,QHostAddress targetAddress,quint64 targetPort, QObject *parent)
    : QObject(parent),
      m_running(0),
      m_stopRequested(0)
{
    m_targetPort = targetPort;
    mipriID = ipriID;
    mch = ch;
    m_targetAddress = targetAddress;
}

tcpsendthreadpro::~tcpsendthreadpro()
{

}

void tcpsendthreadpro::prepareToStart()
{
    // 线程重新启动前清除旧停止标志。
    m_stopRequested.storeRelease(0);
}

void tcpsendthreadpro::requestStop()
{
    // 设置停止标志，doWork() 会在下一次检查时退出。
    m_stopRequested.storeRelease(1);
}

bool tcpsendthreadpro::takeMessage(QByteArray &data)
{
    // 第一步：加锁后检查队列是否为空。
    QMutexLocker locker(&m_queueMutex);
    if (m_queue.isEmpty()) {
        return false;
    }

    // 第二步：取出队首消息并通过引用参数返回。
    data = m_queue.dequeue();
    return true;
}

void tcpsendthreadpro::doWork()
{
    // 第一步：防止同一个发送对象重复执行 doWork()。
    if (!m_running.testAndSetOrdered(0, 1)) {
        return;
    }
    // 第二步：在子线程中创建 TCP 套接字。
    QTcpSocket socket;

    // 第三步：该标志用于避免断线重连时反复输出相同错误。
    bool connectionErrorReported = false;

    // 第四步：持续执行连接、重连和发送，直到收到停止请求。
    while (!m_stopRequested.loadAcquire()) {

        // 第五步：如果当前未连接，则尝试连接目标服务器。
        if (socket.state() == QAbstractSocket::UnconnectedState) {
            emit logMessage(QString("TCP sender connecting to %1:%2")
                            .arg(m_targetAddress.toString())
                            .arg(m_targetPort));

            socket.connectToHost(m_targetAddress, m_targetPort);
            // 第六步：最多等待 1 秒完成连接。
            if (!socket.waitForConnected(1000)) {

                // 第七步：连续重连失败时只报告第一次错误，避免日志刷屏。
                if (!m_stopRequested.loadAcquire() && !connectionErrorReported) {
                    emit errorOccurred(QString("TCP connect failed: %1")
                                       .arg(socket.errorString()));
                    connectionErrorReported = true;
                }

                // 第八步：短暂休眠后继续下一次连接尝试。
                QThread::msleep(200);
                continue;
            }

            // 第九步：连接成功，清除错误状态并记录连接日志。
            connectionErrorReported = false;
            emit logMessage(QString("TCP sender connected to %1:%2")
                            .arg(socket.peerAddress().toString())
                            .arg(socket.peerPort()));
        }

        // 第十步：尝试从发送队列取出一条消息。
        QByteArray message;
        if (takeMessage(message)) {

            // 第十一步：构造“4 字节大端长度 + 消息内容”的完整 TCP 帧。
            QByteArray frame;
            QDataStream stream(&frame, QIODevice::WriteOnly);
            stream.setByteOrder(QDataStream::BigEndian);
            stream << static_cast<quint32>(message.size());
            stream.writeRawData(message.constData(), message.size());

            // 第十二步：写入套接字，并等待数据发送完成。
            if (socket.write(frame) != frame.size()
                    || !socket.waitForBytesWritten(1000)) {

                // 第十三步：发送失败时记录错误、关闭连接，并在下一轮重连。
                emit errorOccurred(QString("TCP send failed: %1")
                                   .arg(socket.errorString()));
                socket.abort();
                continue;
            }

            // 第十四步：发送成功后通知界面。
            emit messageSent(message,
                             QString("%1:%2")
                             .arg(socket.peerAddress().toString())
                             .arg(socket.peerPort()));
            continue;
        }

        // 第十五步：队列为空时短暂等待，同时检测服务端是否已经断开。
        if (!socket.waitForReadyRead(20)) {
            if (socket.state() != QAbstractSocket::ConnectedState) {
                emit logMessage("TCP sender disconnected.");
                socket.abort();
            }
        } else {
            socket.readAll();
        }
    }

    // 第十六步：收到停止请求后主动断开连接。
    socket.disconnectFromHost();
    if (socket.state() != QAbstractSocket::UnconnectedState) {

        // 第十七步：给尚未发送完成的数据一个短暂的刷新机会。
        socket.waitForDisconnected(200);
    }

    // 第十八步：复位运行标志，并通知主窗口结束发送线程。
    m_running.storeRelease(0);
    emit logMessage("TCP sender stopped.");
    emit finished();
}
