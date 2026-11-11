#ifndef FCSENDREADTHREADPRO_H
#define FCSENDREADTHREADPRO_H

#include <QObject>
#include <WTContrl.h>
#include <wtgl.h>
#include <IPluginV2.h>
#include <QByteArray>
#include <QThread>
#include "../WTBusInterfacepro/FC/WTFCSim/WTAPI_TE_DSIM.h"
#include "../UIWidget/datadefine.h"

class fcsendreadthreadpro : public QObject
{
    Q_OBJECT
public:
    fcsendreadthreadpro(qint64 ipriID,int ch,QObject* parent = nullptr);
    ~fcsendreadthreadpro();
    void setStart(bool Start);
    void setStatus(int type);

signals:
    void sig_sendInfo(int ch,int id,quint16 htime,quint32 ltime);
public slots:
    void dowork();
private:
    qint64 mipriID;
    int mch;//通道号
    int mtype = -1;
    WTContrl *pCtrl;
    WTAPI_TE_DSIM_SEND_TIME* sendInfo;
    bool isStart;

};

#endif // FCSENDREADTHREADPRO_H
