#include "netrecvthreadpro.h"

//暂时考虑使用UDP
netrecvthreadpro::netrecvthreadpro(qint64 ipriID,int ch,quint64 listenport,QObject* parent)
    : QObject(parent)
{
    m_listenPort = listenport;
    mipriID = ipriID;
    mch = ch;
}

netrecvthreadpro::~netrecvthreadpro()
{
    m_socket->close();
}

void netrecvthreadpro::start()
{
    //创建socket
    m_socket = new QUdpSocket(this);
    bool ok = m_socket->bind(QHostAddress::AnyIPv4,m_listenPort,QUdpSocket::ShareAddress | QUdpSocket::ReuseAddressHint);
    if(!ok)
    {
        m_socket->deleteLater();
        m_socket = nullptr;
        return;
    }
    connect(m_socket,&QUdpSocket::readyRead,this,&netrecvthreadpro::onreadyRead);
}

void netrecvthreadpro::onreadyRead()
{
    if(!m_running)
    {
        return;
    }
    while (m_socket && m_socket->hasPendingDatagrams())
    {
        const qint64 datagramSize = m_socket->pendingDatagramSize();
        if (datagramSize < 0) {
            break;
        }
        //根据数据包大小动态分配内存
        QByteArray buffer(static_cast<int>(datagramSize), Qt::Uninitialized);
        const qint64 bytesRead = m_socket->readDatagram(buffer.data(),
                                                     buffer.size(),
                                                     &m_senderAddress,
                                                     &m_listenPort);
        if (bytesRead < 0) {
                break;
        }
        //按实际读取长度修正数据，并将数据发送给界面线程。
        buffer.resize(static_cast<int>(bytesRead));
//        emit datagramReceived(buffer,mch);
    }
}


void netrecvthreadpro::setStart(bool Start)
{
    m_running = Start;
}

void netrecvthreadpro::setStatus(int type)
{
    pause = false;
    mtype = type;
}
