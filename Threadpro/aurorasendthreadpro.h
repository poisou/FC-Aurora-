#ifndef AURORASENDTHREADPRO_H
#define AURORASENDTHREADPRO_H

#include <QObject>
#include <WTContrl.h>
#include <wtgl.h>
#include <IPluginV2.h>
#include <QByteArray>
#include <QThread>
#include "../UIWidget/datadefine.h"
#include "../WTBusInterfacepro/Aurora/WTAuroraSim/wtapi_pcie_aurora.h"

class Aurorasendthreadpro : public QObject
{
    Q_OBJECT
public:
    Aurorasendthreadpro(qint64 ipriID,int ch,QObject* parent = nullptr);
    ~Aurorasendthreadpro();
    void setStart(bool Start);
    void setStatus(int status);
    void setDataName(QString name);
    void setSendRecvControl(int type);

signals:
    void sig_sendJD(double v);
    void sig_finish();
    void sig_sendData(RECVDATA data);
    void sig_totalTimes(int ch,int min,int s);
    void sig_nowTimes(int ch,int min,int s,quint64 datalen);

public slots:
    void dowork();
private:
    qint64 mipriID;
    int mch;//通道号
    WTContrl *pCtrl;
    QString mSourecname;
    QThread* mthread;
    bool sendFinish = false;
    int mtype = -1;
    bool isStart;
    quint64 startTime;
    QByteArray dataByte;
    bool timesSend = false;
    int timestep;
    int dataLen;
    int groups;
    bool pause = false;
    int m_msgid = 0;
    int index = 0;//判断data分段

    //数据
    AURORADATA* a_data;
    qint64 nowTimestart = -1;

};

#endif // AURORASENDTHREADPRO_H
