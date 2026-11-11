#include "wid_viewwid.h"
#include "ui_wid_viewwid.h"

wid_viewWid::wid_viewWid(qint64 ipriID,QString text,int ch,QStringList ICDList,QWidget *parent) :
    QWidget(parent),
    ui(new Ui::wid_viewWid)
{
    ui->setupUi(this);
    stext = text;
    mipriID = ipriID;
    mch = ch;
    ui->label_title->setText(text);
    ui->label_4->hide();
    ui->lineEdit_status->setEnabled(false);
    ui->progressBarA_jd->setFormat("%p%");
    ui->comboBox_rul->addItems(ICDList);
    if(!text.contains("Aurora"))
    {
        ui->spinBox->hide();
        ui->label_bot->hide();
    }
    if(!text.contains("FC"))
    {
        ui->widget->hide();
        ui->horizontalSlider->hide();
        ui->lab_totaltime->hide();
        ui->lab_nowtime->hide();
    }
    connect(ui->horizontalSlider,&QSlider::valueChanged,this,&wid_viewWid::snapToPresetValue);

    //默认获取第一个数据的时间
    InitTimes();
}

wid_viewWid::~wid_viewWid()
{
    delete ui;
}

void wid_viewWid::setValue(QStringList rul, QStringList data)
{
    ui->comboBox_rul->addItems(rul);
    ui->comboBox_data->addItems(data);
}

void wid_viewWid::setDataSource(QList<QString> list)
{
    ui->comboBox_data->clear();
    ui->comboBox_data->addItems(list);
}

void wid_viewWid::setJD(double v)
{
    ui->progressBarA_jd->setValue(v);
}

void wid_viewWid::setICDSource(QList<QString> list)
{
    ui->comboBox_rul->clear();
    ui->comboBox_rul->addItems(list);
}

void wid_viewWid::setStatus(int status)
{
    if(status == 0)
    {
        ui->lineEdit_status->setText("离线");
        ui->lineEdit_status->setStyleSheet(
                    "background-color: rgb(255, 255, 255);"
                    "border:2px solid rgb(70,90,120);"
                    "border-radius:10px;"
                    "color: rgb(255,0,0);");
    }
    else
    {
        ui->lineEdit_status->setText("在线");
        ui->lineEdit_status->setStyleSheet(
                    "background-color: rgb(255, 255, 255);"
                    "border:2px solid rgb(70,90,120);"
                    "border-radius:10px;"
                    "color: rgb(0,255,0);");
    }
}

void wid_viewWid::on_comboBox_SRType_currentIndexChanged(int index)
{
    if(index == 1)
    {
        ui->comboBox_rul->setEnabled(false);
    }
    else
    {
        ui->comboBox_rul->setEnabled(true);
    }
    if(stext.contains("FC"))//先用FC模拟测试，实际调试修改
    {
        emit sig_FCrcType(mch,index);
    }
    if(stext.contains("Aurora"))
    {
        emit sig_AurorarcType(mch,index);
    }

}

void wid_viewWid::on_pbtn_run_clicked()
{
    //开始
    ui->progressBarA_jd->setValue(0);
    int type = ui->comboBox_SRType->currentIndex();//收发模式 0:收发 1:send 2:recv
    int rul = ui->comboBox_rul->currentIndex();
    QString data = ui->comboBox_data->currentText();
    int status = 0;//0开始 1暂停 2停止
    if(stext.contains("FC"))
    {
        emit sig_FCthreadControl(mch,type,rul,data,status);
    }
    if(stext.contains("Aurora"))
    {
        emit sig_AurorathreadControl(mch,type,rul,data,status);
    }
    if(stext.contains("以太网"))
    {
        emit sig_NetthreadControl(mch,type,rul,data,status);
    }
}

void wid_viewWid::on_pbtn_pause_clicked()
{
    //暂停
    int type = ui->comboBox_SRType->currentIndex();//收发模式
    int rul = ui->comboBox_rul->currentIndex();
    QString data = ui->comboBox_data->currentText();
    int status = 1;//0开始 1暂停 2停止
#if 1
    if(stext.contains("FC"))
    {
        emit sig_FCthreadControl(mch,type,rul,data,status);
    }
    if(stext.contains("Aurora"))
    {
        emit sig_AurorathreadControl(mch,type,rul,data,status);
    }
#elif 0
    emit sig_FCthreadControl(mch,type,rul,data,status);
#endif
}

void wid_viewWid::on_pbtn_stop_clicked()
{
    //停止
//    ui->progressBarA_jd->setValue(0);
    int type = ui->comboBox_SRType->currentIndex();//收发模式 0:收发 1:send 2:recv
    int rul = ui->comboBox_rul->currentIndex();
    QString data = ui->comboBox_data->currentText();
    ui->progressBarA_jd->setValue(0);
    int status = 2;//0开始 1暂停 2停止
    if(stext.contains("FC"))
    {
        emit sig_FCthreadControl(mch,type,rul,data,status);
    }
    if(stext.contains("Aurora"))
    {
        emit sig_AurorathreadControl(mch,type,rul,data,status);
    }
    if(stext.contains("以太网"))
    {
        emit sig_NetthreadControl(mch,type,rul,data,status);
    }
}

void wid_viewWid::on_spinBox_valueChanged(int arg1)
{
    //修改波特率-Aurora
    emit sig_botspeed(mch,arg1);
}

void wid_viewWid::setTotal(int min,int s)
{
//    ui->lab_totaltime->setText(QString("%1:%2").arg(min).arg(s));
}

void wid_viewWid::setNow(int min, int s,quint64 datalen)
{
    ui->lab_nowtime->setText(QString("%1:%2").arg(min).arg(s));
    ui->label_4->show();
    quint64 currlen = ui->lab_nowtlen->text().toUInt();
    if(showmode == 0)
    {
        ui->lab_nowtlen->setText(QString("%1").arg(currlen + datalen));
    }
    if(showmode == 1)
    {
        double d = currlen + datalen/1024;
        ui->lab_nowtlen->setText(QString("%1").arg(d));
    }
    if(showmode == 2)
    {
        double d = currlen + datalen/1024/1024;
        ui->lab_nowtlen->setText(QString("%1").arg(d));
    }
}

void wid_viewWid::snapToPresetValue(int currentValue)
{
    if(presetValues.isEmpty())
        return;
    qint64 closestValue = presetValues.first();
    qint64 minDiff = std::abs(currentValue - closestValue);

    for(int val : presetValues)
    {
        qint64 diff = std::abs(currentValue - val);
        if(diff < minDiff)
        {
            minDiff = diff;
            closestValue = val;
        }
    }

    if(closestValue != currentValue)
    {
        ui->horizontalSlider->setValue(closestValue);
    }
    //发送到具体下发的处理
    quint64 nowSeconds = closestValue * 0.0000001;
    int min = nowSeconds / 60;
    int seconds = nowSeconds % 60;
    ui->lab_nowtime->setText(QString("%1:%2").arg(min).arg(seconds));
    emit sig_timestart(mch,closestValue);
}

void wid_viewWid::on_comboBox_data_currentIndexChanged(int index)
{
    //获取当前数据的内容
    InitTimes();
}

void wid_viewWid::InitTimes()
{
    QString data = ui->comboBox_data->currentText();
    QStringList chList = PM()->GetCHLs(mipriID,"FC");
    WTContrl *pCtrl = (WTContrl *)(PM()->GetWTCtrl(chList.at(0)));
    MsgBlock mblock = pCtrl->cMessageBlock->mapsTTabel.value(data);//获取ctrl中的发送数据源
    int group = mblock.ITxCnt;//组数
    qint64 dataLen = mblock.iTxLen;//数据长度
    qint64 startTime = mblock.iTotal;//初始时标
    qint64 timestep = mblock.period;//时标步进
    quint64 total = (startTime + group * timestep);
    quint64 totalSeconds = total * 0.0000001;
    int min = totalSeconds / 60;
    int seconds = totalSeconds % 60;
    for(int i = 0;i < group;i++)
    {
        presetValues.append(startTime);
        startTime += timestep;
    }
    ui->horizontalSlider->setRange(0,total);
    ui->lab_totaltime->setText(QString("%1:%2").arg(min).arg(seconds));
    ui->lab_nowtime->setText(QString("%1:%2").arg(0).arg(0));
    if(showmode == 0)
    {
        ui->lab_totalen->setText(QString("%1").arg(dataLen));
    }
    if(showmode == 1)
    {
        double d = dataLen/1024;
        ui->lab_totalen->setText(QString("%1").arg(d));
    }
    if(showmode == 2)
    {
        double d = dataLen/1024/1024;
        ui->lab_totalen->setText(QString("%1").arg(d));
    }
    ui->label_4->show();
}

void wid_viewWid::on_comboBox_currentIndexChanged(int index)
{
    qint64 alllen = ui->lab_totalen->text().toInt();
    qint64 nowlen = ui->lab_nowtlen->text().toInt();
    if(index == 0)
    {
        if(showmode == 1)
        {
            double d = alllen*1024;
            ui->lab_totalen->setText(QString("%1").arg(d));
        }
        if(showmode == 2)
        {
            double d = alllen*1024*1024;
            ui->lab_totalen->setText(QString("%1").arg(d));
        }
        showmode = 0;
    }
    if(index == 1)
    {
        if(showmode == 0)
        {
            double d = alllen/1024;
            ui->lab_totalen->setText(QString("%1").arg(d));
        }
        if(showmode == 2)
        {
            double d = alllen * 1024;
            ui->lab_totalen->setText(QString("%1").arg(d));
        }
        showmode = 1;
    }
    if(index == 2)
    {
        if(showmode == 0)
        {
            double d = alllen/1024/1024;
            ui->lab_totalen->setText(QString("%1").arg(d));
        }
        if(showmode == 2)
        {
            double d = alllen/1024;
            ui->lab_totalen->setText(QString("%1").arg(d));
        }
        showmode = 2;
    }
}
