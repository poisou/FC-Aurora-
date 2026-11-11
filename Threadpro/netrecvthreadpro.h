#ifndef NETRECVTHREADPRO_H
#define NETRECVTHREADPRO_H

#include <QObject>
#include <WTContrl.h>
#include <wtgl.h>
#include <IPluginV2.h>
#include <QByteArray>
#include <QThread>
#include "../UIWidget/datadefine.h"

class netrecvthreadpro : public QObject
{
    Q_OBJECT
public:
    netrecvthreadpro(qint64 ipriID,int ch,quint64 localport, QObject* parent = nullptr);
    ~netrecvthreadpro();
    void setStart(bool Start);
    void setStatus(int status);
signals:
    /// 成功接收到一个完整 UDP 数据报后，将数据和发送方信息发送给界面。
    void datagramReceived(RECVDATA data);
public slots:
    void start();
private slots:
    void onreadyRead();
private:
    qint64 mipriID;
    int mch;//通道号
    QUdpSocket* m_socket = nullptr;
    int mtype = -1;
    bool pause = false;
    bool m_running = false;
    quint16 m_listenPort;
    QHostAddress m_senderAddress;
    bool isStart;
    WTContrl *pCtrl;
    unsigned char* ucBuffer;
    int msgid = 1;//总接收数
};

#endif // NETRECVTHREADPRO_H
