#ifndef AURORARECVTHREADPRO_H
#define AURORARECVTHREADPRO_H

#include <QObject>
#include <WTContrl.h>
#include <wtgl.h>
#include <IPluginV2.h>
#include <QByteArray>
#include <QThread>
#include "../WTBusInterfacepro/Aurora/WTAuroraSim/wtapi_pcie_aurora.h"
#include "../UIWidget/datadefine.h"

class Aurorarecvthreadpro : public QObject
{
    Q_OBJECT
public:
    Aurorarecvthreadpro(qint64 ipriID,int ch,QObject* parent = nullptr);
    ~Aurorarecvthreadpro();
    void setStart(bool Start);
    void setStatus(int type);
    void setControl(int type);
signals:
    void sig_recvAuroraData(RECVDATA data);
public slots:
    void dowork();
private:
    qint64 mipriID;
    int mch;//通道号
    int mtype = -1;
    bool isStart;
    WTContrl *pCtrl;
    unsigned char* ucBuffer;
    int msgid = 1;
};

#endif // AURORARECVTHREADPRO_H
