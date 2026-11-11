#include "tcpsendpro.h"

#include <QAbstractSocket>
#include <QTcpSocket>

TcpSendPro::TcpSendPro(int ch,
                       const QString &sourcePath,
                       const QString &address,
                       quint16 port)
    : DeviceWorkTask(ch, DeviceWork::Send, sourcePath)
    , m_address(address)
    , m_port(port)
{
}

TcpSendPro::~TcpSendPro()
{
    if (m_socket != nullptr) {
        m_socket->abort();
        delete m_socket;
        m_socket = nullptr;
    }
}

void TcpSendPro::processData()
{
    // 未配置对端地址或端口时不发送，避免无意义的重连。
    if (m_address.isEmpty() || m_port == 0) {
        return;
    }

    if (!ensureConnected()) {
        return;
    }

    QByteArray packet;
    if (!readNextPacket(packet, false)) {
        return;
    }

    // 整包写入套接字，成功后才前移读取位置。
    if (send(packet)) {
        m_position += packet.size();
        // 上报发送进度：sendJD(m_position, m_totalBytes)。
        reportSendProgress();
    }
}

bool TcpSendPro::send(const QByteArray &data)
{
    if (m_socket == nullptr
        || m_socket->state() != QAbstractSocket::ConnectedState) {
        return false;
    }

    // 直接发送整包数据；写入不完整或超时则断开，下一轮重连重发。
    if (m_socket->write(data) != data.size()
        || !m_socket->waitForBytesWritten(1000)) {
        m_socket->abort();
        return false;
    }

    return true;
}

bool TcpSendPro::ensureConnected()
{
    if (m_socket != nullptr
        && m_socket->state() == QAbstractSocket::ConnectedState) {
        return true;
    }

    if (m_socket == nullptr) {
        m_socket = new QTcpSocket;
    } else if (m_socket->state() != QAbstractSocket::UnconnectedState) {
        m_socket->abort();
    }

    m_socket->connectToHost(m_address, m_port);
    if (!m_socket->waitForConnected(1000)) {
        m_socket->abort();
        return false;
    }

    return true;
}
