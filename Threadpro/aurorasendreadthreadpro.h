#ifndef AURORASENDREADTHREADPRO_H
#define AURORASENDREADTHREADPRO_H

#include <QObject>
#include <WTContrl.h>
#include <wtgl.h>
#include <IPluginV2.h>
#include <QByteArray>
#include <QThread>
#include "../WTBusInterfacepro/Aurora/WTAuroraSim/wtapi_pcie_aurora.h"
#include "../UIWidget/datadefine.h"

class Aurorasendreadthreadpro : public QObject
{
    Q_OBJECT
public:
    Aurorasendreadthreadpro(qint64 ipriID,int ch,QObject* parent = nullptr);
    ~Aurorasendreadthreadpro();
    void setStart(bool Start);
    void setStatus(int type);
signals:
    void sig_sendInfo(int ch,int id, unsigned long long time);
public slots:
    void dowork();
private:
    qint64 mipriID;
    int mch;//通道号
    int mtype = -1;
    WTContrl *pCtrl;
    bool isStart;
    AURORATIMEDATA* sendInfo;
};

#endif // AURORASENDREADTHREADPRO_H
