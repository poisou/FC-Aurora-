#include "tcprecvpro.h"

#include <QAbstractSocket>
#include <QHostAddress>
#include <QTcpServer>
#include <QTcpSocket>

TcpRecvPro::TcpRecvPro(int ch, const QString &address, quint16 port)
    : DeviceWorkTask(ch, DeviceWork::Receive, QString())
    , m_address(address)
    , m_port(port)
{
    // 接收数据落盘：可执行文件目录/SAVADATA/<年月日时分>_TCP.bin
    // 由基类 saveReceivedData() 使用，开关为 TCP_RECV_SAVE_ENABLED；
    // 单文件上限由 setReceiveSaveMaxBytes() 设置，默认 1 GB。
    setReceiveSaveBus(2);
}

TcpRecvPro::~TcpRecvPro()
{
    for (QTcpSocket *socket : m_clients) {
        socket->abort();
        delete socket;
    }
    m_clients.clear();

    if (m_server != nullptr) {
        m_server->close();
        delete m_server;
        m_server = nullptr;
    }
}

void TcpRecvPro::processData()
{
    if (!ensureListening()) {
        return;
    }

    // 工作线程没有运行事件循环，使用等待接口主动处理新连接。
    if (m_clients.isEmpty()) {
        m_server->waitForNewConnection(10);
    }
    while (m_server->hasPendingConnections()) {
        QTcpSocket *socket = m_server->nextPendingConnection();
        if (socket == nullptr) {
            break;
        }
        m_clients.append(socket);
    }

    // 逐个读取客户端数据；收到后转发给页面写入接收容器。
    for (int index = m_clients.size() - 1; index >= 0; --index) {
        QTcpSocket *socket = m_clients.at(index);
        if (socket->state() == QAbstractSocket::ConnectedState) {
            if (socket->waitForReadyRead(10)) {
                const QByteArray data = socket->readAll();
                if (!data.isEmpty()) {
                    // 如需在本线程直接落盘 SAVADATA/*_TCP.bin，取消下一行注释。
                    // 注意：页面在 dataReceived 中也会落盘一次，二者只保留一处。
                    // saveReceivedData(data);
                    emit dataReceived(data);
                }
            }
        } else {
            m_clients.removeAt(index);
            delete socket;
        }
    }
}

bool TcpRecvPro::send(const QByteArray &data)
{
    // ── TCP 接收线程的回发接口（当前为空实现）────────────────────
    // 如需在接收流程中向客户端回发 ACK/心跳/应答数据，在 processData()
    // 里调用 send(数据)，并在本函数中把 data 写入已连接的客户端：
    //   成功 return true，失败 return false。
    // 目前不做任何发送，直接返回 true，不影响接收上报。
    Q_UNUSED(data)
    return true;
}

bool TcpRecvPro::ensureListening()
{
    if (m_server == nullptr) {
        m_server = new QTcpServer;
    }

    if (m_server->isListening()) {
        return true;
    }

    const QHostAddress address =
        m_address.isEmpty() ? QHostAddress(QHostAddress::AnyIPv4)
                            : QHostAddress(m_address);
    return m_server->listen(address, m_port);
}
