#include "netsendthreadpro.h"

//暂时考虑使用UDP
netsendthreadpro::netsendthreadpro(qint64 ipriID,int ch,quint64 localport,QObject* parent)
    : QObject(parent)
{
    m_localPort = localport;
    mipriID = ipriID;
    mch = ch;
}

netsendthreadpro::~netsendthreadpro()
{

}

void netsendthreadpro::start()
{
    //创建socket
    m_socket = new QUdpSocket(this);
    //    connect(m_socket,&QUdpSocket::readyRead,this,&netsendthreadpro::onreadyRead);
    //    connect(m_socket,QOverload<QAbstractSocket::SocketError>::of(&QAbstractSocket::error),this,&netsendthreadpro::onSocketError);
}

void netsendthreadpro::dowork()
{
    //创建socket
    start();
    //发送
    while(m_running)
    {
        if(pause)
        {
//            QThread::msleep(10);
            continue;
        }
        if(mtype == 0)
        {
            //开始
            pause = false;
//            if(!sendFinish)
            {
                QByteArray cstr= "00 FF 12 FF 80 81 FF 6E 00 02 00 FA 9F 60 8E FF 00 03 07 FB 4A 33 08 12 BF A0 3B 22 00 00 00 1A 0B 2C 04 7B FF 80 81 FF 6E 00 02 00 FA 9F 60 8E FF 00 03 07 FB 4A 33 08 12 BF A0 3B 22 00 00 00 1A"
                             "FF 80 81 FF 6E 00 02 00 FA 9F 60 8E FF 00 03 07 FB 4A 33 08 12 BF A0 3B 22 00 00 00 1A FF 6E 00 02 00 FA 8E FF 00 03 07 FB 00 02 00 FA 9F 00 00 00 1A 0B 2C 04 7B FF 80 81 22 00 00 00 1A FF 22 00"
                             " 01 00 1A FF FF 00 03 07 FB 60 8E FF 00 03 07 FB 4A 33 00 00 1A FF 6E 00 02 00 FA 12 BF A0 3B 22 00 00 00 1A FF 6E A0 3B 22 00 00 00 1A 0B 00 03 07 FB 00 02 00 FA 9F 00 00 02 00 FA 9F 60 8E FF 00"
                             "03 07 8E FF 00  03 07 6E 00 02 00 FA 9F 60 8E FF 00 03 07 FB 4A 33 08 12 BF 4A 33 08 12 BF 4A 33 08 12 BF";
                QHostAddress targetAddr("192.168.1.9");
                quint16 targetPort = 6656;
                m_socket->writeDatagram(cstr,targetAddr,targetPort);
            }
        }
        if(mtype == 1)
        {
            //暂停
            pause = true;
        }
        if(mtype == 2)
        {
            //停止
            pause = true;
            index = 0;
        }
        QThread::msleep(5);
    }
    QThread::msleep(5);
}

//接收端使用****
void netsendthreadpro::onreadyRead()
{

}

void netsendthreadpro::onSocketError(QAbstractSocket::SocketError code)
{

}
//******

void netsendthreadpro::setStart(bool Start)
{
    m_running = Start;
}

void netsendthreadpro::setStatus(int type)
{
    pause = false;
    mtype = type;
}

void netsendthreadpro::setDataName(QString name)
{
    if(mSourecname == name)
    {
        sendFinish = false;
    }
    else
    {
        mSourecname = name;
        QStringList chList = PM()->GetCHLs(mipriID,"NET");
        WTContrl *pCtrl = (WTContrl *)(PM()->GetWTCtrl(chList.at(mch)));
        MsgBlock mblock = pCtrl->cMessageBlock->mapsTTabel.value(mSourecname);//获取ctrl中的发送数据源
        dataLen = mblock.iTxLen;//数据长度
        //数据段
        dataByte = mblock.sData;
        sendFinish = false;
    }
}
