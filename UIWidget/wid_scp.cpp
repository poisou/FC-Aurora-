#include "wid_scp.h"
#include "ui_wid_scp.h"

wid_scp::wid_scp(qint64 ipriID,QWidget *parent) :
    QWidget(parent),
    ui(new Ui::wid_scp)
{
    //场景并行
    ui->setupUi(this);
    mipriID = ipriID;
    InitUI();
    InitThread();
    initTimer();
}

wid_scp::~wid_scp()
{
    delete ui;
}

void wid_scp::InitUI()
{
    InitICD();
    wid_viewWid* view_FC1 = new wid_viewWid(mipriID,"FC总线1",0,map_ICDItem);
    ui->gridLayout->addWidget(view_FC1,0,0,Qt::AlignLeft | Qt::AlignTop);
    map_FCView.insert("FC总线1",view_FC1);
    vec_view.append(view_FC1);
    wid_viewWid* view_FC2 = new wid_viewWid(mipriID,"FC总线2",1,map_ICDItem);
    ui->gridLayout->addWidget(view_FC2,0,1,Qt::AlignLeft | Qt::AlignTop);
    map_FCView.insert("FC总线2",view_FC2);
    vec_view.append(view_FC2);

    wid_viewWid* view_Aurora1 = new wid_viewWid(mipriID,"Aurora总线1",0,map_ICDItem);
    ui->gridLayout->addWidget(view_Aurora1,0,2,Qt::AlignLeft | Qt::AlignTop);
    connect(view_Aurora1,&wid_viewWid::sig_botspeed,this,&wid_scp::slot_botspeed);
    map_AuroraView.insert("Aurora总线1",view_Aurora1);
    vec_view.append(view_Aurora1);
    wid_viewWid* view_Aurora2 = new wid_viewWid(mipriID,"Aurora总线2",1,map_ICDItem);
    ui->gridLayout->addWidget(view_Aurora2,0,3,Qt::AlignLeft | Qt::AlignTop);
    connect(view_Aurora2,&wid_viewWid::sig_botspeed,this,&wid_scp::slot_botspeed);
    map_AuroraView.insert("Aurora总线2",view_Aurora2);
    vec_view.append(view_Aurora2);
    wid_viewWid* view_Aurora3 = new wid_viewWid(mipriID,"Aurora总线3",2,map_ICDItem);
    ui->gridLayout->addWidget(view_Aurora3,0,4,Qt::AlignLeft | Qt::AlignTop);
    connect(view_Aurora3,&wid_viewWid::sig_botspeed,this,&wid_scp::slot_botspeed);
    map_AuroraView.insert("Aurora总线3",view_Aurora3);
    vec_view.append(view_Aurora3);
    wid_viewWid* view_Aurora4 = new wid_viewWid(mipriID,"Aurora总线4",3,map_ICDItem);
    ui->gridLayout->addWidget(view_Aurora4,0,5,Qt::AlignLeft | Qt::AlignTop);
    connect(view_Aurora4,&wid_viewWid::sig_botspeed,this,&wid_scp::slot_botspeed);
    map_AuroraView.insert("Aurora总线4",view_Aurora4);
    vec_view.append(view_Aurora4);
    wid_viewWid* view_Aurora5 = new wid_viewWid(mipriID,"Aurora总线5",4,map_ICDItem);
    ui->gridLayout->addWidget(view_Aurora5,1,0,Qt::AlignLeft | Qt::AlignTop);
    connect(view_Aurora5,&wid_viewWid::sig_botspeed,this,&wid_scp::slot_botspeed);
    map_AuroraView.insert("Aurora总线5",view_Aurora5);
    vec_view.append(view_Aurora5);
    wid_viewWid* view_Aurora6 = new wid_viewWid(mipriID,"Aurora总线6",5,map_ICDItem);
    ui->gridLayout->addWidget(view_Aurora6,1,1,Qt::AlignLeft | Qt::AlignTop);
    connect(view_Aurora6,&wid_viewWid::sig_botspeed,this,&wid_scp::slot_botspeed);
    map_AuroraView.insert("Aurora总线6",view_Aurora6);
    vec_view.append(view_Aurora6);
    wid_viewWid* view_Aurora7 = new wid_viewWid(mipriID,"Aurora总线7",6,map_ICDItem);
    ui->gridLayout->addWidget(view_Aurora7,1,2,Qt::AlignLeft | Qt::AlignTop);
    connect(view_Aurora7,&wid_viewWid::sig_botspeed,this,&wid_scp::slot_botspeed);
    map_AuroraView.insert("Aurora总线7",view_Aurora7);
    vec_view.append(view_Aurora7);
    wid_viewWid* view_Aurora8 = new wid_viewWid(mipriID,"Aurora总线8",7,map_ICDItem);
    ui->gridLayout->addWidget(view_Aurora8,1,3,Qt::AlignLeft | Qt::AlignTop);
    connect(view_Aurora8,&wid_viewWid::sig_botspeed,this,&wid_scp::slot_botspeed);
    map_AuroraView.insert("Aurora总线8",view_Aurora8);
    vec_view.append(view_Aurora8);


    wid_viewWid* view_net1 = new wid_viewWid(mipriID,"以太网1",-1,map_ICDItem);
    ui->gridLayout->addWidget(view_net1,1,4,Qt::AlignLeft | Qt::AlignTop);
    map_NETView.insert("以太网1",view_net1);
    vec_view.append(view_net1);
    ui->gridLayout->setRowStretch(2,1);
    wid_viewWid* view_net2 = new wid_viewWid(mipriID,"以太网2",-1,map_ICDItem);
    ui->gridLayout->addWidget(view_net2,1,5,Qt::AlignLeft | Qt::AlignTop);
    map_NETView.insert("以太网2",view_net2);
    vec_view.append(view_net2);
    ui->gridLayout->setRowStretch(2,1);

    view_net2->hide();


    //填充界面
    msgFC = new wid_msgmonitor(mipriID,0);
    connect(msgFC,&wid_msgmonitor::sig_clearSend,this,&wid_scp::slot_clearFCSend);
    connect(msgFC,&wid_msgmonitor::sig_clearRecv,this,&wid_scp::slot_clearFCRecv);
    ui->gridLayout_fc->addWidget(msgFC);
    msgAurora = new wid_msgmonitor(mipriID,1);
    connect(msgAurora,&wid_msgmonitor::sig_clearSend,this,&wid_scp::slot_clearAuroraSend);
    connect(msgAurora,&wid_msgmonitor::sig_clearRecv,this,&wid_scp::slot_clearAuroraRecv);
    ui->gridLayout_aurora->addWidget(msgAurora);
    msgNet = new wid_msgmonitor(mipriID,2);
    ui->gridLayout_net->addWidget(msgNet);

    //信号槽
    //fc
    connect(view_FC1,&wid_viewWid::sig_FCthreadControl,this,&wid_scp::slot_FCThreadControl);
    connect(view_FC2,&wid_viewWid::sig_FCthreadControl,this,&wid_scp::slot_FCThreadControl);
    connect(view_FC1,&wid_viewWid::sig_FCrcType,this,&wid_scp::slot_FCrcType);
    connect(view_FC2,&wid_viewWid::sig_FCrcType,this,&wid_scp::slot_FCrcType);
    connect(view_FC1,&wid_viewWid::sig_timestart,this,&wid_scp::slot_timestart);
    connect(view_FC2,&wid_viewWid::sig_timestart,this,&wid_scp::slot_timestart);

    //aurora
    connect(view_Aurora1,&wid_viewWid::sig_AurorathreadControl,this,&wid_scp::slot_AuroraThreadControl);
    connect(view_Aurora1,&wid_viewWid::sig_AurorarcType,this,&wid_scp::slot_AurorarcType);
    connect(view_Aurora2,&wid_viewWid::sig_AurorathreadControl,this,&wid_scp::slot_AuroraThreadControl);
    connect(view_Aurora2,&wid_viewWid::sig_AurorarcType,this,&wid_scp::slot_AurorarcType);
    connect(view_Aurora3,&wid_viewWid::sig_AurorathreadControl,this,&wid_scp::slot_AuroraThreadControl);
    connect(view_Aurora3,&wid_viewWid::sig_AurorarcType,this,&wid_scp::slot_AurorarcType);
    connect(view_Aurora4,&wid_viewWid::sig_AurorathreadControl,this,&wid_scp::slot_AuroraThreadControl);
    connect(view_Aurora4,&wid_viewWid::sig_AurorarcType,this,&wid_scp::slot_AurorarcType);
    connect(view_Aurora5,&wid_viewWid::sig_AurorathreadControl,this,&wid_scp::slot_AuroraThreadControl);
    connect(view_Aurora5,&wid_viewWid::sig_AurorarcType,this,&wid_scp::slot_AurorarcType);
    connect(view_Aurora6,&wid_viewWid::sig_AurorathreadControl,this,&wid_scp::slot_AuroraThreadControl);
    connect(view_Aurora6,&wid_viewWid::sig_AurorarcType,this,&wid_scp::slot_AurorarcType);
    connect(view_Aurora7,&wid_viewWid::sig_AurorathreadControl,this,&wid_scp::slot_AuroraThreadControl);
    connect(view_Aurora7,&wid_viewWid::sig_AurorarcType,this,&wid_scp::slot_AurorarcType);
    connect(view_Aurora8,&wid_viewWid::sig_AurorathreadControl,this,&wid_scp::slot_AuroraThreadControl);
    connect(view_Aurora8,&wid_viewWid::sig_AurorarcType,this,&wid_scp::slot_AurorarcType);
    //net
    connect(view_net1,&wid_viewWid::sig_NetthreadControl,this,&wid_scp::slot_NetThreadControl);
    connect(view_net1,&wid_viewWid::sig_NetthreadControl,this,&wid_scp::slot_NetThreadControl);

}

void wid_scp::InitThread()
{
    //创建任务类
    //FC************
    //fc-send
    fcsendthread1pro = new FCSendThreadpro(mipriID,0);
    connect(fcsendthread1pro,&FCSendThreadpro::sig_sendJD,this,[=](double v)
    {
        wid_viewWid* view_FC1 = map_FCView.value("FC总线1");
        view_FC1->setJD(v);
    });

    connect(fcsendthread1pro,&FCSendThreadpro::sig_finish,this,[=]
    {
        fcsendthread1pro->setStart(false);
        fcsendthread1->quit();
        isRun1 = false;
    });
    connect(fcsendthread1pro,&FCSendThreadpro::sig_sendData,this,&wid_scp::slot_FCSendData);
    connect(fcsendthread1pro,&FCSendThreadpro::sig_totalTimes,this,&wid_scp::slot_totaltimes);
    connect(fcsendthread1pro,&FCSendThreadpro::sig_nowTimes,this,&wid_scp::slot_nowTimes);

    fcsendthread2pro = new FCSendThreadpro(mipriID,1);
    connect(fcsendthread2pro,&FCSendThreadpro::sig_sendJD,this,[=](double v)
    {
        wid_viewWid* view_FC2 = map_FCView.value("FC总线2");
        view_FC2->setJD(v);
    });

    connect(fcsendthread2pro,&FCSendThreadpro::sig_finish,this,[=]
    {
        fcsendthread2pro->setStart(false);
        fcsendthread2->quit();
        isRun2 = false;
    });
    connect(fcsendthread2pro,&FCSendThreadpro::sig_sendData,this,&wid_scp::slot_FCSendData);
    connect(fcsendthread2pro,&FCSendThreadpro::sig_totalTimes,this,&wid_scp::slot_totaltimes);
    connect(fcsendthread2pro,&FCSendThreadpro::sig_nowTimes,this,&wid_scp::slot_nowTimes);

    //fc-recv
    fcrecvthread1pro = new FCRecvThreadpro(mipriID,0);
    connect(fcrecvthread1pro,&FCRecvThreadpro::sig_recvFCData,this,&wid_scp::slot_recvFCData,Qt::QueuedConnection);
    fcrecvthread2pro = new FCRecvThreadpro(mipriID,1);
    connect(fcrecvthread2pro,&FCRecvThreadpro::sig_recvFCData,this,&wid_scp::slot_recvFCData,Qt::QueuedConnection);
    //fc-sendtime
    fcsendtimepro1 = new fcsendreadthreadpro(mipriID,0);
    connect(fcsendtimepro1,&fcsendreadthreadpro::sig_sendInfo,this,&wid_scp::slot_sendtime);
    fcsendtimepro2 = new fcsendreadthreadpro(mipriID,1);
    connect(fcsendtimepro2,&fcsendreadthreadpro::sig_sendInfo,this,&wid_scp::slot_sendtime);
    //*****************
    //Aurora******************
    //创建线程
    //send
    Aurorasendthread1 = new QThread(this);
    Aurorasendthread2 = new QThread(this);
    Aurorasendthread3 = new QThread(this);
    Aurorasendthread4 = new QThread(this);
    Aurorasendthread5 = new QThread(this);
    Aurorasendthread6 = new QThread(this);
    Aurorasendthread7 = new QThread(this);
    Aurorasendthread8 = new QThread(this);
    map_asendthread.insert(0,Aurorasendthread1);
    map_asendthread.insert(1,Aurorasendthread2);
    map_asendthread.insert(2,Aurorasendthread3);
    map_asendthread.insert(3,Aurorasendthread4);
    map_asendthread.insert(4,Aurorasendthread5);
    map_asendthread.insert(5,Aurorasendthread6);
    map_asendthread.insert(6,Aurorasendthread7);
    map_asendthread.insert(7,Aurorasendthread8);
    //recv
    Aurorarecvthread1 = new QThread(this);
    Aurorarecvthread2 = new QThread(this);
    Aurorarecvthread3 = new QThread(this);
    Aurorarecvthread4 = new QThread(this);
    Aurorarecvthread5 = new QThread(this);
    Aurorarecvthread6 = new QThread(this);
    Aurorarecvthread7 = new QThread(this);
    Aurorarecvthread8 = new QThread(this);
    map_arecvthread.insert(0,Aurorarecvthread1);
    map_arecvthread.insert(1,Aurorarecvthread2);
    map_arecvthread.insert(2,Aurorarecvthread3);
    map_arecvthread.insert(3,Aurorarecvthread4);
    map_arecvthread.insert(4,Aurorarecvthread5);
    map_arecvthread.insert(5,Aurorarecvthread6);
    map_arecvthread.insert(6,Aurorarecvthread7);
    map_arecvthread.insert(7,Aurorarecvthread8);
    //sendread
    Aurorasendtimethread1 = new QThread(this);
    Aurorasendtimethread2 = new QThread(this);
    Aurorasendtimethread3 = new QThread(this);
    Aurorasendtimethread4 = new QThread(this);
    Aurorasendtimethread5 = new QThread(this);
    Aurorasendtimethread6 = new QThread(this);
    Aurorasendtimethread7 = new QThread(this);
    Aurorasendtimethread8 = new QThread(this);
    map_asendreadthread.insert(0,Aurorasendtimethread1);
    map_asendreadthread.insert(1,Aurorasendtimethread2);
    map_asendreadthread.insert(2,Aurorasendtimethread3);
    map_asendreadthread.insert(3,Aurorasendtimethread4);
    map_asendreadthread.insert(4,Aurorasendtimethread5);
    map_asendreadthread.insert(5,Aurorasendtimethread6);
    map_asendreadthread.insert(6,Aurorasendtimethread7);
    map_asendreadthread.insert(7,Aurorasendtimethread8);
    for(int i = 0;i < map_AuroraView.count();i++)
    {
        //send
        Aurorasendthreadpro* aurorasendthreadpro = new Aurorasendthreadpro(mipriID,i);
        aurorasendthreadpro->moveToThread(map_asendthread.value(i));
        connect(map_asendthread.value(i),&QThread::started,aurorasendthreadpro,&Aurorasendthreadpro::dowork);
        connect(aurorasendthreadpro,&Aurorasendthreadpro::sig_sendJD,this,[=](double v)
        {
            wid_viewWid* view = map_AuroraView.value(QString("Aurora总线%1").arg(i+1));
            view->setJD(v);
        });
        connect(aurorasendthreadpro,&Aurorasendthreadpro::sig_finish,this,[=]
        {
            aurorasendthreadpro->setStart(false);
            QThread* t = map_asendthread.value(i);
            t->quit();
        });
        connect(aurorasendthreadpro,&Aurorasendthreadpro::sig_sendData,this,&wid_scp::slot_AuroraSendData);
        map_asendpro.insert(i,aurorasendthreadpro);
        //recv
        Aurorarecvthreadpro* aurorarecvthreadpro = new Aurorarecvthreadpro(mipriID,i);
        aurorarecvthreadpro->moveToThread(map_arecvthread.value(i));
        connect(map_arecvthread.value(i),&QThread::started,aurorarecvthreadpro,&Aurorarecvthreadpro::dowork);
        connect(aurorarecvthreadpro,&Aurorarecvthreadpro::sig_recvAuroraData,this,&wid_scp::slot_recvAuroraData,Qt::QueuedConnection);
        map_arecvpro.insert(i,aurorarecvthreadpro);
        //sendread
        Aurorasendreadthreadpro* aurorasendreadpro = new Aurorasendreadthreadpro(mipriID,i);
        aurorasendreadpro->moveToThread(map_asendreadthread.value(i));
        connect(map_asendreadthread.value(i),&QThread::started,aurorasendreadpro,&Aurorasendreadthreadpro::dowork);
        connect(aurorasendreadpro,&Aurorasendreadthreadpro::sig_sendInfo,this,&wid_scp::slot_asendtime);
        map_asendreadpro.insert(i,aurorasendreadpro);
        bool isrun = false;
        map_ArunType.insert(i,isrun);
    }
    //以太网******************
    netsendthread1pro = new netsendthreadpro(mipriID,8888,0);
    connect(netsendthread1pro,&netsendthreadpro::sig_sendJD,this,[=](double v)
    {
        wid_viewWid* view_net1 = map_NETView.value("以太网1");
        view_net1->setJD(v);
    });

    netrecvthread1pro = new netrecvthreadpro(mipriID,6589,0);
    connect(netrecvthread1pro,&netrecvthreadpro::datagramReceived,this,&wid_scp::slot_recvnetData,Qt::QueuedConnection);

    //创建线程类************
    //fc-sendThread
    fcsendthread1 = new QThread(this);
    fcsendthread1pro->moveToThread(fcsendthread1);
    connect(fcsendthread1,&QThread::started,fcsendthread1pro,&FCSendThreadpro::dowork);
    fcsendthread2 = new QThread(this);
    fcsendthread2pro->moveToThread(fcsendthread2);
    connect(fcsendthread2,&QThread::started,fcsendthread2pro,&FCSendThreadpro::dowork);

    //fc-recvThread
    fcrecvthread1 = new QThread(this);
    fcrecvthread1pro->moveToThread(fcrecvthread1);
    connect(fcrecvthread1,&QThread::started,fcrecvthread1pro,&FCRecvThreadpro::dowork);
    fcrecvthread2 = new QThread(this);
    fcrecvthread2pro->moveToThread(fcrecvthread2);
    connect(fcrecvthread2,&QThread::started,fcrecvthread2pro,&FCRecvThreadpro::dowork);

    //fc-sendtime
    fcsendtimethread1 = new QThread(this);
    fcsendtimepro1->moveToThread(fcsendtimethread1);
    connect(fcsendtimethread1,&QThread::started,fcsendtimepro1,&fcsendreadthreadpro::dowork);
    fcsendtimethread2 = new QThread(this);
    fcsendtimepro2->moveToThread(fcsendtimethread2);
    connect(fcsendtimethread2,&QThread::started,fcsendtimepro2,&fcsendreadthreadpro::dowork);
}


void wid_scp::slot_botspeed(int ch, int speed)
{
    emit sig_botspeed(ch,speed);
}

void wid_scp::slot_AurorarcType(int ch, int type)
{
    //根据通道设置收发使能
    QStringList chList = PM()->GetCHLs(mipriID,"Aurora");
    WTContrl *pCtrl = (WTContrl *)(PM()->GetWTCtrl(chList.at(ch)));//使用对应通道的ctrl
    QVariant mCmd = type;
    int result = pCtrl->Contrl("setRSType",mCmd);
    if(result == 0)
    {
        Dlog_info* dlog = new Dlog_info(2,"通道使能设置成功！");
        dlog->exec();
        return;
    }
    else
    {
        Dlog_info* dlog = new Dlog_info(2,"通道使能设置失败！");
        dlog->exec();
        return;
    }
}

void wid_scp::slot_FCrcType(int ch, int type)
{
    //根据通道设置收发使能
    QStringList chList = PM()->GetCHLs(mipriID,"FC");
    WTContrl *pCtrl = (WTContrl *)(PM()->GetWTCtrl(chList.at(ch)));//使用对应通道的ctrl
    QVariant mCmd = type;
    int result = pCtrl->Contrl("setRSType",mCmd);
    if(result == 0)
    {
        Dlog_info* dlog = new Dlog_info(2,"通道使能设置成功！");
        dlog->exec();
        return;
    }
    else
    {
        Dlog_info* dlog = new Dlog_info(2,"通道使能设置失败！");
        dlog->exec();
        return;
    }
}

void wid_scp::slot_clearFCSend()
{
    queue_sendFCData.clear();
    queue_showsFCData.clear();
}

void wid_scp::slot_clearFCRecv()
{
    queue_recvFCData.clear();
    queue_showFCData.clear();
}

void wid_scp::slot_clearAuroraSend()
{
    queue_sendAuroraData.clear();
    queue_showsAuroraData.clear();
}

void wid_scp::slot_clearAuroraRecv()
{
    queue_recvAuroraData.clear();
    queue_showAuroraData.clear();
}

void wid_scp::slot_timestart(int ch, qint64 time)
{
    if(ch == 0)
    {
        fcsendthread1pro->setStartTime(time);
    }
    else
    {
        fcsendthread2pro->setStartTime(time);
    }
}

void wid_scp::slot_FCThreadControl(int ch, int type, int rul, QString data, int status)
{
    if(ch == 0)
    {
        if(status == 0)//开始
        {
            if(isRun1)
            {
                return;
            }
            //设置通道1收发使能
            fcsendthread1pro->setSendRecvControl(type);
            if(type == 0)
            {
                //收发模式
                //发送线程
                fcsendthread1pro->setStatus(status);
                fcsendthread1pro->setStart(true);
                fcsendthread1pro->setDataName(data);//设置发送data
                if(!fcsendthread1->isRunning())
                {
                    fcsendthread1->start();
                }
                //接收线程
                fcrecvthread1pro->setStatus(status);
                fcrecvthread1pro->setStart(true);
                if(!fcrecvthread1->isRunning())
                {
                    fcrecvthread1->start();
                }
                //发送时标线程
                fcsendtimepro1->setStatus(status);
                fcsendtimepro1->setStart(true);
//                fcsendtimepro1->setHandle(Handle);
                if(!fcsendtimethread1->isRunning())
                {
                    fcsendtimethread1->start();
                }
                //---------启动刷新界面接收列表的定时器
                if(!UpdateRecvTabletimer->isActive())
                    UpdateRecvTabletimer->start(1000);
                isRun1 = true;
            }
            if(type == 1)
            {
                //仅发送
                //发送
                fcsendthread1pro->setStatus(status);
                fcsendthread1pro->setStart(true);
                fcsendthread1pro->setDataName(data);//设置发送data
                //发送时标线程
                fcsendtimepro1->setStatus(status);
                fcsendtimepro1->setStart(true);
                if(!fcsendtimethread1->isRunning())
                {
                    fcsendtimethread1->start();
                }
                if(!fcsendthread1->isRunning())
                {
                    fcsendthread1->start();
                    isRun1 = true;
                }
            }
            if(type == 2)
            {
                //仅接收
                //接收
                fcrecvthread1pro->setStatus(0);
                fcrecvthread1pro->setStart(true);
                if(!fcrecvthread1->isRunning())
                {
                    fcrecvthread1->start();
                }
                isRun1 = true;
                if(!UpdateRecvTabletimer->isActive())
                    UpdateRecvTabletimer->start(1000);
            }

        }
        if(status == 1)//暂停
        {
            fcsendthread1pro->setStatus(status);
            isRun1 = false;
        }
        if(status == 2)//停止
        {
            //收发都停
            //发送
            fcsendthread1pro->setSendRecvControl(-1);
            fcsendthread1pro->setStatus(status);
            fcsendthread1pro->setStart(false);
            if(fcsendthread1->isRunning())
            {
                fcsendthread1->quit();
            }
            //接收
            //------刷新定时器停止即可
            fcrecvthread1pro->setStatus(-1);
            if(UpdateRecvTabletimer->isActive())
                UpdateRecvTabletimer->stop();
            isRun1 = false;
            //发送时标线程
            fcsendtimepro1->setStatus(status);
            fcsendtimepro1->setStart(false);
            if(fcsendtimethread1->isRunning())
            {
                fcsendtimethread1->quit();
            }
            //清空
            queue_sendFCData.clear();
            queue_showsFCData.clear();
            queue_recvFCData.clear();
            queue_showFCData.clear();
        }
    }
    if(ch == 1)
    {
        if(status == 0)//开始
        {
            if(isRun2)
            {
                return;
            }
            fcsendthread2pro->setSendRecvControl(type);
            if(type == 0)
            {
                //收发
                //发送
                fcsendthread2pro->setStatus(status);
                fcsendthread2pro->setStart(true);
                fcsendthread2pro->setDataName(data);//设置发送data
                if(!fcsendthread2->isRunning())
                {
                    fcsendthread2->start();
                }
                //接收
                fcrecvthread2pro->setStatus(status);
                fcrecvthread2pro->setStart(true);
                if(!fcrecvthread2->isRunning())
                {
                    fcrecvthread2->start();
                }
                //发送时标线程
                fcsendtimepro2->setStatus(status);
//                fcsendtimepro2->setHandle(Handle);
                fcsendtimepro2->setStart(true);
                if(!fcsendtimethread2->isRunning())
                {
                    fcsendtimethread2->start();
                }
                //---------启动刷新界面接收列表的定时器
                if(!UpdateRecvTabletimer->isActive())
                    UpdateRecvTabletimer->start(1000);
                isRun2 = true;
            }
            if(type == 1)
            {
                //仅发送
                //发送
                fcsendthread2pro->setStatus(status);
                fcsendthread2pro->setStart(true);
                fcsendthread2pro->setDataName(data);//设置发送data
                if(!fcsendthread2->isRunning())
                {
                    fcsendthread2->start();
                    isRun2 = true;
                }
                //发送时标线程
                fcsendtimepro2->setStatus(status);
                fcsendtimepro2->setStart(true);
                if(!fcsendtimethread2->isRunning())
                {
                    fcsendtimethread2->start();
                }
            }
            if(type == 2)
            {
                //仅接收
                //接收
                fcrecvthread2pro->setStatus(0);
                fcrecvthread2pro->setStart(true);
                if(!fcrecvthread2->isRunning())
                {
                    fcrecvthread2->start();
                }

                isRun2 = true;
                if(!UpdateRecvTabletimer->isActive())
                    UpdateRecvTabletimer->start(1000);
            }
        }
        if(status == 1)//暂停
        {
            fcsendthread2pro->setStatus(status);
            isRun2 = false;
        }
        if(status == 1)//停止
        {
            //收发都停
            //发送
            fcsendthread2pro->setSendRecvControl(-1);
            fcsendthread2pro->setStatus(status);
            fcsendthread2pro->setStart(false);
            fcsendthread2->quit();
            //接收
            //------刷新定时器停止即可
            fcrecvthread2pro->setStatus(-1);
            if(UpdateRecvTabletimer->isActive())
                UpdateRecvTabletimer->stop();
            isRun2 = false;
            //发送时标线程
            fcsendtimepro2->setStatus(status);
            fcsendtimepro2->setStart(false);
            if(!fcsendtimethread2->isRunning())
            {
                fcsendtimethread2->quit();
            }
            //清空
            queue_recvFCData.clear();
            queue_showFCData.clear();
        }
    }
}

void wid_scp::slot_AuroraThreadControl(int ch, int type, int rul, QString data, int status)
{
    if(status == 0)//开始
    {
        if(map_ArunType.value(ch))
        {
            return;
        }
        map_asendpro.value(ch)->setSendRecvControl(type);//设置收发使能
        if(type == 0)
        {
            //收发模式
            //发送线程
            map_asendpro.value(ch)->setStatus(status);
            map_asendpro.value(ch)->setStart(true);
            map_asendpro.value(ch)->setDataName(data);//设置发送data
            if(!map_asendthread.value(ch)->isRunning())
            {
                map_asendthread.value(ch)->start();
            }
            //接收线程
            map_arecvpro.value(ch)->setStatus(status);
            map_arecvpro.value(ch)->setStart(true);
            if(!map_arecvthread.value(ch)->isRunning())
            {
                map_arecvthread.value(ch)->start();
            }
#if 1
            //发送时标线程
            map_asendreadpro.value(ch)->setStatus(status);
            map_asendreadpro.value(ch)->setStart(true);
            if(!map_asendreadthread.value(ch)->isRunning())
            {
                map_asendreadthread.value(ch)->start();
            }
#endif
            //-----启动刷新界面接收列表的定时器---
            if(!AUpdateRecvTabletimer->isActive())
                AUpdateRecvTabletimer->start(1000);
            map_ArunType.insert(ch,true);
        }
        if(type == 1)
        {
            //仅发送
            //发送
            map_asendpro.value(ch)->setStatus(status);
            map_asendpro.value(ch)->setStart(true);
            map_asendpro.value(ch)->setDataName(data);
            if(!map_asendthread.value(ch)->isRunning())
            {
                map_asendthread.value(ch)->start();
            }
            map_arecvpro.value(ch)->setStart(false);
            //发送时标线程
            map_asendreadpro.value(ch)->setStatus(status);
            map_asendreadpro.value(ch)->setStart(true);
            if(!map_asendreadthread.value(ch)->isRunning())
            {
                map_asendreadthread.value(ch)->start();
            }
            map_ArunType.insert(ch,true);
        }
        if(type == 2)
        {
            //仅接收
            map_asendpro.value(ch)->setStatus(0);
            map_arecvpro.value(ch)->setStart(true);
            if(!map_arecvthread.value(ch)->isRunning())
            {
                map_arecvthread.value(ch)->start();
            }
        }
    }
    if(status == 1)//暂停
    {
        map_asendpro.value(ch)->setStatus(status);
        map_ArunType.insert(ch,false);
    }
    if(status == 2)//停止
    {
        //收发都停
        map_asendpro.value(ch)->setSendRecvControl(-1);
        map_asendpro.value(ch)->setStatus(status);
        map_asendpro.value(ch)->setStart(false);
        if(map_asendthread.value(ch)->isRunning())
        {
            map_asendthread.value(ch)->quit();
        }
        //接收
        //------刷新定时器停止即可
        map_arecvpro.value(ch)->setStatus(-1);
        if(AUpdateRecvTabletimer->isActive())
            AUpdateRecvTabletimer->stop();
        map_ArunType.insert(ch,false);
        //发送时标线程
        map_asendreadpro.value(ch)->setStatus(status);
        map_asendreadpro.value(ch)->setStart(false);
        if(map_asendreadthread.value(ch)->isRunning())
        {
            map_asendreadthread.value(ch)->quit();
        }
        //清空
        queue_recvAuroraData.clear();
        queue_showAuroraData.clear();
        queue_sendAuroraData.clear();
        queue_showsAuroraData.clear();
    }
}

void wid_scp::slot_NetThreadControl(int ch, int type, int rul, QString data, int status)
{
    if(status == 0)//开始
    {
        if(isnRun)
        {
            return;
        }
        if(type == 0)
        {
            //收发模式
            //发送线程
            netsendthread1pro->setStatus(status);
            netsendthread1pro->setStart(true);
            netsendthread1pro->setDataName(data);
            if(!netsendthread1->isRunning())
            {
                netsendthread1->start();
            }
            //接收线程
            netrecvthread1pro->setStatus(status);
            netrecvthread1pro->setStart(true);
            if(netrecvthread1->isRunning())
            {
                netrecvthread1->start();
            }
            //---------启动刷新界面接收列表的定时器
            if(!NUpdateRecvTabletimer->isActive())
                NUpdateRecvTabletimer->start(1000);
            isnRun = true;
        }
        if(type == 1)
        {
            //仅发送
            //发送线程
            netsendthread1pro->setStatus(status);
            netsendthread1pro->setStart(true);
            netsendthread1pro->setDataName(data);
            if(!netsendthread1->isRunning())
            {
                netsendthread1->start();
            }
            if(netrecvthread1->isRunning())
            {
                netrecvthread1->quit();
            }
        }
        if(type == 2)
        {
            //仅接收
            netrecvthread1pro->setStatus(status);
            netrecvthread1pro->setStart(true);
            if(netrecvthread1->isRunning())
            {
                netrecvthread1->start();
            }
            if(!netsendthread1->isRunning())
            {
                netsendthread1->quit();
            }
        }
    }
    if(status == 1)//暂停
    {
        netsendthread1pro->setStatus(status);
        isnRun = false;
    }
    if(status == 2)//停止
    {
        //收发都停

    }
}

void wid_scp::initTimer()
{
    //初始化定时器
    UpdateRecvTabletimer = new QTimer();
    AUpdateRecvTabletimer = new QTimer();
    NUpdateRecvTabletimer = new QTimer();
    connect(UpdateRecvTabletimer,&QTimer::timeout,this,[=]
    {
        //FC
        updateFCTable();
        updateSendTable();
    });
    connect(AUpdateRecvTabletimer,&QTimer::timeout,this,[=]
    {
        //Aurora
        updateAuroraTable();
        updateAuroraSendTable();
    });
    connect(NUpdateRecvTabletimer,&QTimer::timeout,this,[=]
    {
        //Aurora
        updatenetTable();
        updatenetSendTable();
    });
}


void wid_scp::InitICD()
{
    readICD();
}

void wid_scp::readICD()
{
    //读取ICD配置文件
    map_ICDItem.clear();
    QString ICDPath = QApplication::applicationDirPath() + "/prj/WTBTS/ICD";
    QStringList FileList = findJsonFiles(ICDPath);
    for(int fid = 0;fid < FileList.count();fid++)
    {
        QString path = FileList.at(fid);
        QJsonDocument doc = readJsonFile(path);
        if(!doc.isNull())
        {
            if(doc.isObject())
            {
                QJsonObject obj = doc.object();
                QStringList keys = obj.keys();
                //存储到缓存
                for(int i = 0;i < keys.count();i++)
                {
                    QString nametext = keys.at(i);
                    map_ICDItem.append(nametext);
                }
            }
        }
    }
}

void wid_scp::readDataSource(QList<QString> list)
{
    //读取sqlite中的数据表，获取数据源
    for(int i = 0;i < vec_view.count();i++)
    {
        wid_viewWid* v = vec_view.at(i);
        v->setDataSource(list);
    }
}

void wid_scp::slot_totaltimes(int ch, int min, int s)
{
    if(ch == 0)
    {
        wid_viewWid* v = map_FCView.value("FC总线1");
        v->setTotal(min,s);
    }
    else
    {
        wid_viewWid* v1 = map_FCView.value("FC总线2");
        v1->setTotal(min,s);
    }
}

QStringList wid_scp::findJsonFiles(const QString &floderPath)
{
    QStringList jsonFiles;
    QDir dir(floderPath);
    if(!dir.exists())
    {
        jsonFiles.clear();
        return jsonFiles;
    }
    //过滤.json文件
    QStringList filters;
    filters << "*.json";

    QFileInfoList fileList = dir.entryInfoList(filters,QDir::Files | QDir::NoDotAndDotDot,QDir::Name);
    for(const QFileInfo &fileInfo : fileList)
    {
        jsonFiles.append(fileInfo.absoluteFilePath());
    }

    return jsonFiles;
}

QJsonDocument wid_scp::readJsonFile(const  QString &filePath)
{
    QFile file(filePath);
    if(!file.open(QIODevice::ReadOnly))
    {
        return QJsonDocument();
    }
    QByteArray data = file.readAll();
    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(data,&error);
    if(error.error != QJsonParseError::NoError)
    {
        file.close();
        return QJsonDocument();
    }
    file.close();
    return doc;
}

void wid_scp::readICDTable()
{
    readICD();
    for(int i = 0;i < vec_view.count();i++)
    {
        wid_viewWid* v = vec_view.at(i);
        v->setICDSource(map_ICDItem);
    }
}

void wid_scp::slot_AuroraOnline(int ch, int ststus)
{
    ch += 1;
    QStringList views = map_AuroraView.keys();
    for(int i = 0;i < views.count();i++)
    {
        QString str = views.at(i);
        if(str.contains(QString("%1").arg(ch)))
        {
            wid_viewWid* v = map_AuroraView.value(str);
            v->setStatus(ststus);
        }
    }
}

void wid_scp::slot_FCOnline(int ch,int ststus)
{
    ch += 1;
    QStringList views = map_FCView.keys();
    for(int i = 0;i < views.count();i++)
    {
        QString str = views.at(i);
        if(str.contains(QString("%1").arg(ch)))
        {
            wid_viewWid* v = map_FCView.value(str);
            v->setStatus(ststus);
        }
    }
#if 0
    //0812
    QStringList Aviews = map_AuroraView.keys();
    for(int i = 0;i < Aviews.count();i++)
    {
        QString str = Aviews.at(i);
//        if(str.contains(QString("%1").arg(ch)))
//        {
            wid_viewWid* v = map_AuroraView.value(str);
            v->setStatus(ststus);
//        }
    }
    QStringList nviews = map_NETView.keys();
    for(int i = 0;i < nviews.count();i++)
    {
        QString nstr = nviews.at(i);
//        if(str.contains(QString("%1").arg(ch)))
//        {
            wid_viewWid* nv = map_NETView.value(nstr);
            nv->setStatus(ststus);
//        }
    }
#endif
}

void wid_scp::slot_Asendtime(int ch, int id, quint16 htime, quint32 ltime)
{
    //发送时标-存储到发送的结构体中
    m_AuroraSmutex.lock();
    for(int i = 0;i < queue_sendAuroraData.count();i++)
    {
        RECVDATA mdata = queue_sendAuroraData.at(i);
        if(ch == mdata.ch  && id == mdata.msgIdx)
        {
            int n = qMin(queue_showsAuroraData.size(),LIMITQUEUELEN);
            if(n > LIMITQUEUELEN)
            {
                queue_showsAuroraData.dequeue();
            }
            else
            {
                queue_showsAuroraData.enqueue(mdata);
            }
            asnew = true;
        }
    }
    m_AuroraSmutex.unlock();
}

void wid_scp::slot_asendtime(int ch, int id, unsigned long long time)
{
    m_AuroraSmutex.lock();
    for(int i = 0;i < queue_sendAuroraData.count();i++)
    {
        RECVDATA mdata = queue_sendAuroraData.at(i);
        if(ch == mdata.ch   && id == mdata.msgIdx)
        {
            mdata.atime = time;
            int n = qMin(queue_showsAuroraData.size(),LIMITQUEUELEN);
            if(n > LIMITQUEUELEN)
            {
                queue_showsAuroraData.dequeue();
            }
            else
            {
                queue_showsAuroraData.enqueue(mdata);
            }
            asnew = true;
        }
    }
    m_AuroraSmutex.unlock();
}

void wid_scp::slot_sendtime(int ch, int id, quint16 htime, quint32 ltime)
{
    //发送时标-存储到发送的结构体中
    m_fcSmutex.lock();
    for(int i = 0;i < queue_sendFCData.count();i++)
    {
        RECVDATA mdata = queue_sendFCData.at(i);
        if(ch == mdata.ch   && id == mdata.msgIdx)
        {
            mdata.time.sethigh16(htime);
            mdata.time.setLow32(ltime);
            int n = qMin(queue_showsFCData.size(),LIMITQUEUELEN);
            if(n > LIMITQUEUELEN)
            {
                queue_showsFCData.dequeue();
            }
            else
            {
                queue_showsFCData.enqueue(mdata);
            }
            fcsnew = true;
        }
    }
    m_fcSmutex.unlock();
}

void wid_scp::slot_AuroraSendData(RECVDATA data)
{
    //存储发送的数据（1000条上限）
    m_AuroraSmutex.lock();
    queue_sendAuroraData.enqueue(data);
    if(queue_sendAuroraData.size() > LIMITQUEUELEN)
    {
        queue_sendAuroraData.dequeue();
    }
    m_AuroraSmutex.unlock();
}

void wid_scp::slot_FCSendData(RECVDATA data)
{
    //存储发送的数据（1000条上限）
    m_fcSmutex.lock();
    queue_sendFCData.enqueue(data);
    if(queue_sendFCData.size() > LIMITQUEUELEN)
    {
        queue_sendFCData.dequeue();
    }
    m_fcSmutex.unlock();
}

void wid_scp::slot_recvAuroraData(RECVDATA data)
{
    //接收到数据
#if 1
    m_Auroramutex.lock();
    queue_recvAuroraData.enqueue(data);
    if(queue_recvAuroraData.size() > MAXQUEUELEN)
    {
        queue_recvAuroraData.dequeue();
    }
    m_Auroramutex.unlock();
    anew = true;
#endif
}

void wid_scp::slot_recvFCData(RECVDATA data)
{
    //接收到数据
#if 1
    m_fcmutex.lock();
    queue_recvFCData.enqueue(data);
    if(queue_recvFCData.size() > MAXQUEUELEN)
    {
        queue_recvFCData.dequeue();
    }
    m_fcmutex.unlock();
    fcnew = true;
#endif
}

void wid_scp::slot_recvnetData(RECVDATA data)
{
    m_netmutex.lock();
    queue_recvnetData.enqueue(data);
    if(queue_recvnetData.size() > MAXQUEUELEN)
    {
        queue_recvnetData.dequeue();
    }
    m_netmutex.unlock();
    netnew = true;
}

//qbytarray 转 unsigned char*
//const unsigned char* p = reinterpret_cast<const unsigned char*>(ba.constData());
void wid_scp::updateFCTable()
{
    //刷新接收数据到表格中
    if(!fcnew)
    {
        return;
    }
    m_fcmutex.lock();
    //此时总的数据队列个数
    int allCount = queue_recvFCData.count();
    int n = qMin(allCount,LIMITQUEUELEN);
    int startIndex = allCount - n;
    for(int i = startIndex;i < allCount;i++)
    {
        queue_showFCData.enqueue(queue_recvFCData[i]);
    }
    m_fcmutex.unlock();
    //取出数据并显示
    msgFC->setRecvShow(queue_showFCData);
    fcnew = false;
}

void wid_scp::updateSendTable()
{
    //刷新发送数据到表格中
    if(!fcsnew)
    {
        return;
    }
   msgFC->setSendShow(queue_showsFCData);
   fcsnew = false;
}

void wid_scp::updateAuroraTable()
{
    //刷新接收数据到表格
    if(!anew)
    {
        return;
    }
    m_Auroramutex.lock();
    //此时总的数据队列个数
    int allCount = queue_recvAuroraData.count();
    int n = qMin(allCount,LIMITQUEUELEN);
    int startIndex = allCount - n;
    for(int i = startIndex;i < allCount;i++)
    {
        queue_showAuroraData.enqueue(queue_recvAuroraData[i]);
    }
    m_Auroramutex.unlock();
    //取出数据并显示
    msgAurora->setARecvShow(queue_showAuroraData);
    anew = false;
}

void wid_scp::updateAuroraSendTable()
{
    //刷新发送数据到表格中
    if(!asnew)
    {
        return;
    }
    msgAurora->setASendShow(queue_showsAuroraData);
    asnew = false;
}

void wid_scp::updatenetTable()
{

}

void wid_scp::updatenetSendTable()
{

}


void wid_scp::slot_nowTimes(int ch, int min, int s,quint64 datalen)
{
    if(ch == 0)
    {
        wid_viewWid* v = map_FCView.value("FC总线1");
        v->setNow(min,s,datalen);
    }
    else
    {
        wid_viewWid* v = map_FCView.value("FC总线2");
        v->setNow(min,s,datalen);
    }
}

void wid_scp::setHandle(HANDLE han)
{
    Handle = han;
}

void wid_scp::addView()
{

}

void wid_scp::delView()
{

}
