#ifndef NETSENDTHREADPRO_H
#define NETSENDTHREADPRO_H

#include <QObject>
#include <WTContrl.h>
#include <wtgl.h>
#include <IPluginV2.h>
#include <QByteArray>
#include <QThread>
#include "../UIWidget/datadefine.h"

#define MAXUDPDATALEN 1024
class netsendthreadpro : public QObject
{
    Q_OBJECT
public:
    netsendthreadpro(qint64 ipriID,int ch,quint64 localport, QObject* parent = nullptr);
    ~netsendthreadpro();
    void setDataName(QString name);
    void setStart(bool Start);
    void setStatus(int status);
signals:
    void sig_sendJD(double v);
public slots:
    void start();
private slots:
    void onreadyRead();
    void onSocketError(QAbstractSocket::SocketError code);
private:
    void dowork();
private:
    qint64 mipriID;
    int mch;
    WTContrl *pCtrl;
    QString mSourecname;
    quint64 m_localPort = 0;
    QUdpSocket* m_socket = nullptr;
    bool m_running = false;
    bool sendFinish = false;
    int dataLen;
    int mtype = -1;
    int index = 0;//判断data分段
    QByteArray dataByte;
    bool pause = false;
};
//支持BVLC、DHCP、DNS、FTP、HTTP v1.0/1.1、HTTPS v1.0/1.1、IMAP、LDAP、NTP、NTP-PMP、NNTP、POP3、RADIUS、RSH、RTSP、SDP、SNMPv1/v2/v3、SSH、Syslog、TFTP等服务协议

#endif // NETSENDTHREADPRO_H
