#ifndef FCRECVTHREADPRO_H
#define FCRECVTHREADPRO_H

#include <QObject>
#include <WTContrl.h>
#include <wtgl.h>
#include <IPluginV2.h>
#include <QByteArray>
#include <QThread>
#include "../WTBusInterfacepro/FC/WTFCSim/WTAPI_TE_DSIM.h"
#include "../UIWidget/datadefine.h"

#define WTDEFINE 0XBCB55656

class FCRecvThreadpro : public QObject
{
    Q_OBJECT
public:
    FCRecvThreadpro(qint64 ipriID,int ch,QObject* parent = nullptr);
    ~FCRecvThreadpro();
    void setStart(bool Start);
    void setStatus(int type);
    void setControl(int type);

signals:
    void sig_updateData(unsigned char* data,int dataLen);
    void sig_recvFCData(RECVDATA data);
public slots:
    void dowork();
private:
    qint64 mipriID;
    int mch;//通道号
    int mtype = -1;
    bool isStart;
    WTContrl *pCtrl;
    unsigned char* ucBuffer;
    int msgid = 1;//总接收数

};

#endif // FCRECVTHREADPRO_H
