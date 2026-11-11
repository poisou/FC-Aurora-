#include "wid_mainwindow.h"
#include "ui_wid_mainwindow.h"



wid_mainwindow::wid_mainwindow(qint64 ipriID,QWidget *parent) :
    QWidget(parent),
    ui(new Ui::wid_mainwindow)
{
    mipriID = ipriID;
    ui->setupUi(this);
    qRegisterMetaType<RECVDATA>();
    Init();
}

wid_mainwindow::~wid_mainwindow()
{
    delete ui;
}

void wid_mainwindow::Init()
{
    InitUI();
    InitTimer();
    InitDev();
}

void wid_mainwindow::InitUI()
{
    mwid_dev = new wid_stack(mipriID,0);
    mwid_rul = new wid_stack(mipriID,1);
    mwid_scp = new wid_stack(mipriID,6);
    mwid_datacontrol = new wid_stack(mipriID,4);
    mwid_reviewset = new wid_stack(mipriID,5);
//    mwid_datamonitor = new wid_stack(mipriID,7);
    //列表同步
    connect(mwid_dev,&wid_stack::sig_topItemClick,this,&wid_mainwindow::slot_topItemClick);
    connect(this,&wid_mainwindow::sig_TOPClick,mwid_dev,&wid_stack::slot_TOPClick);
    connect(mwid_dev,&wid_stack::sig_updateItem,this,&wid_mainwindow::slot_update);
    connect(mwid_dev,&wid_stack::sig_updateAItem,this,&wid_mainwindow::slot_updateA);
    connect(this,&wid_mainwindow::sig_update,mwid_dev,&wid_stack::slot_updateitem);
    connect(this,&wid_mainwindow::sig_updateA,mwid_dev,&wid_stack::slot_updateAitem);

    connect(mwid_rul,&wid_stack::sig_topItemClick,this,&wid_mainwindow::slot_topItemClick);
    connect(this,&wid_mainwindow::sig_TOPClick,mwid_rul,&wid_stack::slot_TOPClick);
    connect(mwid_rul,&wid_stack::sig_updateItem,this,&wid_mainwindow::slot_update);
    connect(this,&wid_mainwindow::sig_update,mwid_rul,&wid_stack::slot_updateitem);

    connect(mwid_datacontrol,&wid_stack::sig_topItemClick,this,&wid_mainwindow::slot_topItemClick);
    connect(this,&wid_mainwindow::sig_TOPClick,mwid_datacontrol,&wid_stack::slot_TOPClick);
    connect(mwid_datacontrol,&wid_stack::sig_updateItem,this,&wid_mainwindow::slot_update);
    connect(this,&wid_mainwindow::sig_update,mwid_datacontrol,&wid_stack::slot_updateitem);

    connect(mwid_reviewset,&wid_stack::sig_topItemClick,this,&wid_mainwindow::slot_topItemClick);
    connect(this,&wid_mainwindow::sig_TOPClick,mwid_reviewset,&wid_stack::slot_TOPClick);
    connect(mwid_reviewset,&wid_stack::sig_updateItem,this,&wid_mainwindow::slot_update);
    connect(this,&wid_mainwindow::sig_update,mwid_reviewset,&wid_stack::slot_updateitem);

    connect(mwid_scp,&wid_stack::sig_topItemClick,this,&wid_mainwindow::slot_topItemClick);
    connect(this,&wid_mainwindow::sig_TOPClick,mwid_scp,&wid_stack::slot_TOPClick);
    connect(mwid_scp,&wid_stack::sig_updateItem,this,&wid_mainwindow::slot_update);
    connect(this,&wid_mainwindow::sig_update,mwid_scp,&wid_stack::slot_updateitem);

    //同步设备状态
    connect(mwid_dev,&wid_stack::sig_FCOnline,mwid_scp,&wid_stack::slot_FCOnline);
    connect(mwid_dev,&wid_stack::sig_AuroraOnline,mwid_scp,&wid_stack::slot_AuroraOnline);
    //同步数据源状态
    connect(mwid_datacontrol,&wid_stack::sig_scpShow,mwid_scp,&wid_stack::slot_scpDs);
    //同步ICD列表信息
    connect(mwid_rul,&wid_stack::sig_ICDShow,mwid_scp,&wid_stack::slot_ICDShow);

    //显示数据源数据库列表

    ui->glay_dev->addWidget(mwid_dev);
    ui->glay_rul->addWidget(mwid_rul);
    ui->glay_datacontrol->addWidget(mwid_datacontrol);
    ui->glay_reviewset->addWidget(mwid_reviewset);
    ui->glay_scp->addWidget(mwid_scp);
    ui->tabWidget->setCurrentIndex(0);

    ui->label_2->hide();
    ui->label_3->hide();
    ui->pushButton->hide();
}

void wid_mainwindow::InitTimer()
{
    date_timer = new QTimer(this);
    connect(date_timer,&QTimer::timeout,this,&wid_mainwindow::updateDate);
    date_timer->start(150);

}


void wid_mainwindow::InitDev()
{
    //FC
    QStringList chList = PM()->GetCHLs(mipriID,"FC");
    for(int chindex = 0;chindex < chList.count();chindex++)
    {
        WTContrl *pCtrl = (WTContrl *)(PM()->GetWTCtrl(chList.at(chindex)));
        QVariant mCmd;
        pCtrl->Contrl("Init",mCmd);
    }
    //Aurora
    QStringList achList = PM()->GetCHLs(mipriID,"Aurora");
    for(int achindex = 0;achindex < achList.count();achindex++)
    {
        WTContrl *paCtrl = (WTContrl *)(PM()->GetWTCtrl(achList.at(achindex)));
        QVariant mCmd;
        paCtrl->Contrl("Init",mCmd);
    }
}


void wid_mainwindow::updateDate()
{
    QDateTime current = QDateTime::currentDateTime();
    int year = current.date().year();
    int month = current.date().month();
    int day = current.date().day();
    int hour = current.time().hour();
    int min = current.time().minute();
    int s = current.time().second();
    QString time = QString("时间:%1-%2-%3|%4:%5:%6").arg(year).arg(month,2,10,QChar('0')).arg(day,2,10,QChar('0')).arg(hour,2,10,QChar('0')).arg(min,2,10,QChar('0')).arg(s,2,10,QChar('0'));
    ui->lab_date->setText(time);
}


void wid_mainwindow::slot_topItemClick(bool top,int row)
{
    emit sig_TOPClick(top,row);
}

void wid_mainwindow::slot_update(int fcstatus)
{
    emit sig_update(fcstatus);
}

void wid_mainwindow::slot_updateA(int astatus)
{
    emit sig_updateA(astatus);
}
