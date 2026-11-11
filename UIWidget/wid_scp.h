#ifndef WID_SCP_H
#define WID_SCP_H

#include <QWidget>
#include <QTimer>
#include "wid_viewwid.h"
#include "wid_msgmonitor.h"
#include "dlog_info.h"
#include <QDir>
#include <QFileInfoList>
#include <QStringList>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDebug>
#include <QThread>
#include <WTContrl.h>
#include <wtgl.h>
#include <IPluginV2.h>
#include <QQueue>
#include <QTimer>
#include <QMutex>
#include <QDateTime>
#include "../Threadpro/fcrecvthreadpro.h"
#include "../Threadpro/fcsendthreadpro.h"
#include "../Threadpro/fcsendreadthreadpro.h"
#include "../Threadpro/aurorarecvthreadpro.h"
#include "../Threadpro/aurorasendreadthreadpro.h"
#include "../Threadpro/aurorasendthreadpro.h"
#include "../Threadpro/netsendthreadpro.h"
#include "../Threadpro/netrecvthreadpro.h"


#define MAXQUEUELEN 1000
#define LIMITQUEUELEN 100
namespace Ui {
class wid_scp;
}

class wid_scp : public QWidget
{
    Q_OBJECT

public:
    explicit wid_scp(qint64 ipriID,QWidget *parent = nullptr);
    ~wid_scp();
    void setHandle(HANDLE han);
signals:
    void sig_update(QString text,int status);
    void sig_botspeed(int ch,int speed);
public slots:
    void slot_FCOnline(int ch,int ststus);
    void slot_AuroraOnline(int ch,int ststus);
    void readDataSource(QList<QString> list);
    void readICDTable();
    void slot_FCThreadControl(int ch,int type,int rul,QString data,int status);
    void slot_AuroraThreadControl(int ch,int type,int rul,QString data,int status);
    void slot_NetThreadControl(int ch,int type,int rul,QString data,int status);
    void slot_FCrcType(int ch,int type);
    void slot_AurorarcType(int ch,int type);
    void slot_recvFCData(RECVDATA data);
    void slot_recvAuroraData(RECVDATA data);
    void slot_recvnetData(RECVDATA data);
    void slot_sendtime(int ch,int id,quint16 htime,quint32 ltime);
    void slot_Asendtime(int ch,int id,quint16 htime,quint32 ltime);
    void slot_asendtime(int ch,int id,unsigned long long time);
    void slot_FCSendData(RECVDATA data);
    void slot_AuroraSendData(RECVDATA data);
    void slot_botspeed(int ch,int speed);
    void slot_clearFCSend();
    void slot_clearFCRecv();
    void slot_clearAuroraSend();
    void slot_clearAuroraRecv();
    void slot_totaltimes(int ch,int min,int s);
    void slot_nowTimes(int ch,int min,int s,quint64 datalen);
    void slot_timestart(int ch,qint64 time);
private:
    void InitUI();
    void InitThread();
    void initTimer();
    void InitICD();
    void readICD();
    //刷新FC相关列表
    void updateFCTable();
    void updateSendTable();
    //刷新Aurora相关列表
    void updateAuroraTable();
    void updateAuroraSendTable();
    //刷新以太网相关列表
    void updatenetTable();
    void updatenetSendTable();
    //新增view
    void addView();
    //删除view
    void delView();

    QStringList findJsonFiles(const QString &floderPath);
    QJsonDocument readJsonFile(const  QString &filePath);
private:
    Ui::wid_scp *ui;
    qint64 mipriID;
    QStringList map_ICDItem;
    QVector<wid_viewWid*> vec_view;
    QMap<QString,wid_viewWid*> map_FCView;
    QMap<QString,wid_viewWid*> map_AuroraView;
    QMap<QString,wid_viewWid*> map_NETView;
    QTimer* UpdateRecvTabletimer;
    QTimer* AUpdateRecvTabletimer;
    QTimer* NUpdateRecvTabletimer;

    //FC 句柄
    HANDLE Handle;

    wid_msgmonitor* msgFC;
    wid_msgmonitor* msgAurora;
    wid_msgmonitor* msgNet;


    //FC
    bool isRun1 = false;
    bool isRun2 = false;

    //以太网
    bool isnRun = false;

    bool fcnew = false;
    bool fcsnew = false;

    bool anew = false;
    bool asnew = false;

    bool netnew = false;
    bool netsnew = false;

    //fc线程工作类
    FCSendThreadpro* fcsendthread1pro;
    FCSendThreadpro* fcsendthread2pro;
    FCRecvThreadpro* fcrecvthread1pro;
    FCRecvThreadpro* fcrecvthread2pro;
    fcsendreadthreadpro* fcsendtimepro1;
    fcsendreadthreadpro* fcsendtimepro2;

    //Aurora线程工作类
    QMap<int,Aurorasendthreadpro*> map_asendpro;
    QMap<int,Aurorarecvthreadpro*> map_arecvpro;
    QMap<int,Aurorasendreadthreadpro*> map_asendreadpro;

    //以太网线程工作类
    netsendthreadpro* netsendthread1pro;
    netsendthreadpro* netsendthread2pro;
    netrecvthreadpro* netrecvthread1pro;
    netrecvthreadpro* netrecvthread2pro;


    //Aurora线程类
    QMap<int,QThread*> map_asendthread;
    QMap<int,QThread*> map_arecvthread;
    QMap<int,QThread*> map_asendreadthread;

    //以太网线程类
    QThread* netsendthread1;
    QThread* netsendthread2;
    QThread* netrecvthread1;
    QThread* netrecvthread2;

    //fc线程类

    QThread* fcsendthread1;
    QThread* fcsendthread2;
    QThread* fcrecvthread1;
    QThread* fcrecvthread2;
    QThread* fcsendtimethread1;
    QThread* fcsendtimethread2;


    //Aurora
    QMap<int,bool> map_ArunType;

    //send线程类
    QThread* Aurorasendthread1;
    QThread* Aurorasendthread2;
    QThread* Aurorasendthread3;
    QThread* Aurorasendthread4;
    QThread* Aurorasendthread5;
    QThread* Aurorasendthread6;
    QThread* Aurorasendthread7;
    QThread* Aurorasendthread8;
    //recv
    QThread* Aurorarecvthread1;
    QThread* Aurorarecvthread2;
    QThread* Aurorarecvthread3;
    QThread* Aurorarecvthread4;
    QThread* Aurorarecvthread5;
    QThread* Aurorarecvthread6;
    QThread* Aurorarecvthread7;
    QThread* Aurorarecvthread8;
    //sendread
    QThread* Aurorasendtimethread1;
    QThread* Aurorasendtimethread2;
    QThread* Aurorasendtimethread3;
    QThread* Aurorasendtimethread4;
    QThread* Aurorasendtimethread5;
    QThread* Aurorasendtimethread6;
    QThread* Aurorasendtimethread7;
    QThread* Aurorasendtimethread8;
    //fc锁
    QMutex m_fcmutex;//接收的数据mutex
    QMutex m_fcSmutex;//发送的数据mutex
    //存储接收的FC数据帧
    QQueue<RECVDATA> queue_recvFCData;//存1000条数据
    QQueue<RECVDATA> queue_showFCData;//显示100条
    //存储发送的FC数据帧
    QQueue<RECVDATA> queue_sendFCData;//存100条数据
    QQueue<RECVDATA> queue_showsFCData;//显示发送时标对应的数据

    //aurora锁
    QMutex m_Auroramutex;//接收的数据mutex
    QMutex m_AuroraSmutex;//发送的数据mutex
    //存储接收的Aurora数据帧
    QQueue<RECVDATA> queue_recvAuroraData;//存1000条数据
    QQueue<RECVDATA> queue_showAuroraData;//显示100条
    //存储发送的Aurora数据帧
    QQueue<RECVDATA> queue_sendAuroraData;//存100条数据
    QQueue<RECVDATA> queue_showsAuroraData;//显示发送时标对应的数据

    //以太网锁
    QMutex m_netmutex;//接收的数据mutex
    QMutex m_netSmutex;//发送的数据mutex
    //存储接收的以太网数据帧
    QQueue<RECVDATA> queue_recvnetData;//存1000条数据
    QQueue<RECVDATA> queue_shownetData;//显示100条
    //存储发送的以太网数据帧
    QQueue<RECVDATA> queue_sendnetData;//存100条数据
    QQueue<RECVDATA> queue_showsnetData;//显示发送时标对应的数据
};

#endif // WID_SCP_H
