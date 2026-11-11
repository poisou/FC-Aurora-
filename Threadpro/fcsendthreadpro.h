#ifndef FCSENDTHREADPRO_H
#define FCSENDTHREADPRO_H

#include <QObject>
#include <WTContrl.h>
#include <wtgl.h>
#include <IPluginV2.h>
#include <QByteArray>
#include <QThread>
#include <QRandomGenerator>
#include "../WTBusInterfacepro/FC/WTFCSim/WTAPI_TE_DSIM.h"
#include "../UIWidget/datadefine.h"

class FCSendThreadpro : public QObject
{
    Q_OBJECT
public:
    FCSendThreadpro(qint64 ipriID,int ch,QObject* parent = nullptr);
    ~FCSendThreadpro();
    void setStart(bool Start);
    void setStatus(int status);
    void setStartTime(qint64 time);
    void setDataName(QString name);
    void setSendRecvControl(int type);
signals:
    void sig_sendJD(double v);
    void sig_finish();
    void sig_sendData(RECVDATA data);
    void sig_totalTimes(int ch,int min,int s);
    void sig_nowTimes(int ch,int min,int s,quint64 datalen);
//    void sig_sendInfo(int ch,int datalen,QByteArray bytebuffer,QVariant info);
public slots:
    void dowork();
private:

private:
    qint64 mipriID;
    int mch;//通道号
    WTContrl *pCtrl;
    QString mSourecname;
    QThread* mthread;
    int mtype = -1;
    bool isStart;
    bool sendFinish = false;
    WTAPI_TE_DSIM_SEND_HEAD* mwthead = nullptr;
    WTAPI_TE_DSIM_FCHEAD* mfchead = nullptr;
    QByteArray dataByte;
    quint64 startTime;
    qint64 nowTimestart = -1;
    bool timesSend = false;
    int timestep;
    int dataLen;
    int groups;
    int index = 0;//判断data分段
    int bb = 6;
    int m_msgid = 0;
    bool pause = false;

    int errCount = -1;
};

#endif // FCSENDTHREADPRO_H
