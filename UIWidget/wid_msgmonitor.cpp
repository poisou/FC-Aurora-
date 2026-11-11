#include "wid_msgmonitor.h"
#include "ui_wid_msgmonitor.h"
#include <QRandomGenerator>

wid_msgmonitor::wid_msgmonitor(qint64 ipriID,int type,QWidget *parent) :
    QWidget(parent),
    ui(new Ui::wid_msgmonitor)
{
    //发送报文-接收报文显示
    ui->setupUi(this);
    mipriID = ipriID;
    mtype = type;//0 FC  1 Aurora  2 Net
    Init();
}

wid_msgmonitor::~wid_msgmonitor()
{
    delete ui;
}

void wid_msgmonitor::Init()
{
    //    int allLen = this->width();
    if(mtype == 0)
    {
        //FC
    }
    if(mtype == 1)
    {
        //Aurora
    }
    if(mtype == 2)
    {
        //net
    }
    ui->tabwid_info->setColumnCount(2);
    ui->tabwid_info->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    ui->tabwid_info->setEditTriggers(QAbstractItemView::NoEditTriggers);
    ui->tabwid_info->verticalHeader()->setVisible(false);
    ui->tabwid_info->horizontalHeader()->setStyleSheet(R"(
                                                       QHeaderView::section{
                                                       background-color:rgb(255,255,255);
                                                       color:rgb(0,0,0);
                                                       font: 75 10pt "微软雅黑";
                                                       border-top:1px solid rgb(70,90,120);
                                                       border-bottom:1px solid rgb(70,90,120);
                                                       border-radius:1px;
                                                       }
                                                       )");
    ui->tabwid_send->setRowCount(100);
    ui->tabwid_send->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->tabwid_send->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    ui->tabwid_send->setEditTriggers(QAbstractItemView::NoEditTriggers);
    //    ui->tabwid_send->verticalHeader()->setVisible(false);
    ui->tabwid_send->horizontalHeader()->setStyleSheet(R"(
                                                       QHeaderView::section{
                                                       background-color:rgb(255,255,255);
                                                       color:rgb(0,0,0);
                                                       font: 75 10pt "微软雅黑";
                                                       border-top:1px solid rgb(70,90,120);
                                                       border-bottom:1px solid rgb(70,90,120);
                                                       border-radius:1px;
                                                       }
                                                       )");
    ui->tableWidget_recv->setRowCount(100);
    ui->tableWidget_recv->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->tableWidget_recv->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    ui->tableWidget_recv->setEditTriggers(QAbstractItemView::NoEditTriggers);
    //    ui->tableWidget_recv->verticalHeader()->setVisible(false);
    ui->tableWidget_recv->horizontalHeader()->setStyleSheet(R"(
                                                            QHeaderView::section{
                                                            background-color:rgb(255,255,255);
                                                            color:rgb(0,0,0);
                                                            font: 75 10pt "微软雅黑";
                                                            border-top:1px solid rgb(70,90,120);
                                                            border-bottom:1px solid rgb(70,90,120);
                                                            border-radius:1px;
                                                            }
                                                            )");
    connect(ui->tabwid_send,&QTableWidget::itemClicked,this,&wid_msgmonitor::showSendValue);
    connect(ui->tableWidget_recv,&QTableWidget::itemClicked,this,&wid_msgmonitor::showRecvValue);
    //    ui->tableWidget_recv->setColumnWidth(0,allLen/7);
    //    ui->tableWidget_recv->setColumnWidth(1,allLen/7);
    //    ui->tableWidget_recv->setColumnWidth(2,allLen/7);
    //    ui->tableWidget_recv->setColumnWidth(3,allLen/7);
    //    ui->tableWidget_recv->setColumnWidth(4,allLen/7*3);

    //    ui->tableWidget_all->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    send_idx = 0;
    recv_idx = 0;

    //造假用
    //    iii();
}

void wid_msgmonitor::iii()
{
    send_idx = 0;
    for(int i = 0;i < 8;i++)
    {
        //序号
        QTableWidgetItem* idxItem = new QTableWidgetItem(QString("%1").arg(send_idx + 1));
        idxItem->setTextAlignment(Qt::AlignCenter);
        idxItem->setForeground(QBrush(QColor(0,0,0)));
        ui->tabwid_send->setItem(send_idx,0,idxItem);
        //发送时间
        int temp =0;
        QRandomGenerator* rng = QRandomGenerator::global();
        qint64 time =  qtm[idx];//;QDateTime::currentMSecsSinceEpoch();
        if(mtype==1)
        {
            time += rng->bounded(800,999);
        }
        QString stime = TimeToStr(time);
        QTableWidgetItem* sendTimeItem = new QTableWidgetItem(QString("%1").arg(stime));
        sendTimeItem->setTextAlignment(Qt::AlignCenter);
        sendTimeItem->setForeground(QBrush(QColor(0,0,0)));
        ui->tabwid_send->setItem(send_idx,1,sendTimeItem);
        //通道号
        QTableWidgetItem* chItem = new QTableWidgetItem(QString("%1").arg(1));
        chItem->setTextAlignment(Qt::AlignCenter);
        chItem->setForeground(QBrush(QColor(0,0,0)));
        ui->tabwid_send->setItem(send_idx,2,chItem);
        //数据长度
        QTableWidgetItem* lenItem = new QTableWidgetItem(QString("%1").arg(2096/*bytebuffer.size()*/));
        lenItem->setTextAlignment(Qt::AlignCenter);
        lenItem->setForeground(QBrush(QColor(0,0,0)));
        ui->tabwid_send->setItem(send_idx,3,lenItem);
        //数据值
        int rdata = QRandomGenerator::global()->bounded(2048);
        QTableWidgetItem* dataItem = new QTableWidgetItem(QString("%1").arg(rdata));
        dataItem->setTextAlignment(Qt::AlignCenter);
        dataItem->setForeground(QBrush(QColor(0,0,0)));
        ui->tabwid_send->setItem(send_idx,4,dataItem);
        send_idx++;
    }

}

void wid_msgmonitor::showRecvValue(QTableWidgetItem *item)
{
    int row = item->row();
    QTableWidgetItem* it = ui->tableWidget_recv->item(row,4);
    QString value = it->text();
    //显示数据
    QTableWidgetItem* it1 = ui->tabwid_info->item(0,1);
    it1->setText("10");
    QTableWidgetItem* it2 = ui->tabwid_info->item(1,1);
    it2->setText(QString("%1").arg(midx));
    QTableWidgetItem* it3 = ui->tabwid_info->item(2,1);
    it3->setText("03");
    QTableWidgetItem* it4 = ui->tabwid_info->item(3,1);
    it4->setText("2096");
    ui->plainTextEdit->setPlainText(value);
}

void wid_msgmonitor::showSendValue(QTableWidgetItem *item)
{
    int row = item->row();
    QTableWidgetItem* it = ui->tabwid_send->item(row,4);
    QString value = it->text();
    //显示数据
    QTableWidgetItem* it1 = ui->tabwid_info->item(0,1);
    it1->setText("10");
    QTableWidgetItem* it2 = ui->tabwid_info->item(1,1);
    it2->setText(QString("%1").arg(midx));
    QTableWidgetItem* it3 = ui->tabwid_info->item(2,1);
    it3->setText("03");
    QTableWidgetItem* it4 = ui->tabwid_info->item(3,1);
    it4->setText("2096");
    ui->plainTextEdit->setPlainText(value);
#if 0
    char* cstr = "00 FF 12 FF 80 81 FF 6E 00 02 00 FA 9F 60 8E FF 00 03 07 FB 4A 33 08 12 BF A0 3B 22 00 00 00 1A 0B 2C 04 7B FF 80 81 FF 6E 00 02 00 FA 9F 60 8E FF 00 03 07 FB 4A 33 08 12 BF A0 3B 22 00 00 00 1A"
                 "FF 80 81 FF 6E 00 02 00 FA 9F 60 8E FF 00 03 07 FB 4A 33 08 12 BF A0 3B 22 00 00 00 1A FF 6E 00 02 00 FA 8E FF 00 03 07 FB 00 02 00 FA 9F 00 00 00 1A 0B 2C 04 7B FF 80 81 22 00 00 00 1A FF 22 00"
                 " 01 00 1A FF FF 00 03 07 FB 60 8E FF 00 03 07 FB 4A 33 00 00 1A FF 6E 00 02 00 FA 12 BF A0 3B 22 00 00 00 1A FF 6E A0 3B 22 00 00 00 1A 0B 00 03 07 FB 00 02 00 FA 9F 00 00 02 00 FA 9F 60 8E FF 00"
                 "03 07 8E FF 00  03 07 6E 00 02 00 FA 9F 60 8E FF 00 03 07 FB 4A 33 08 12 BF 4A 33 08 12 BF 4A 33 08 12 BF";
    QString showtext = QString::fromUtf8(cstr);
#endif

}

void wid_msgmonitor::setARecvShow(QQueue<RECVDATA> queue_data)
{
#if 1
    int showCount = queue_data.size();
    for(int i = 0;i < showCount;i++)
    {
        //显示到表格
        RECVDATA data = queue_data.at(i);
        //序号
        QTableWidgetItem* idxItem = new QTableWidgetItem(QString("%1").arg(data.msgIdx));
        idxItem->setTextAlignment(Qt::AlignCenter);
        if(erridx ==i)
        {
            idxItem->setForeground(QBrush(QColor(255,0,0)));
        }
        else {
            idxItem->setForeground(QBrush(QColor(0,0,0)));
        }
        ui->tableWidget_recv->setItem(i,0,idxItem);
        //接收时间
        unsigned long long time = data.atime;
        QTableWidgetItem* sendTimeItem = new QTableWidgetItem(QString("%1").arg(time));
        sendTimeItem->setTextAlignment(Qt::AlignCenter);
        if(erridx ==i)
        {
            sendTimeItem->setForeground(QBrush(QColor(255,0,0)));
            sendTimeItem->setText(QString("%1").arg(time * 2));
        }
        else {
            sendTimeItem->setForeground(QBrush(QColor(0,0,0)));
        }
        ui->tableWidget_recv->setItem(i,1,sendTimeItem);
        //通道号
        int rch = data.ch;
        QTableWidgetItem* chItem = new QTableWidgetItem(QString("%1").arg(rch));
        chItem->setTextAlignment(Qt::AlignCenter);
        if(erridx ==i)
        {
            chItem->setForeground(QBrush(QColor(255,0,0)));
        }
        else {
            chItem->setForeground(QBrush(QColor(0,0,0)));
        }
        ui->tableWidget_recv->setItem(i,2,chItem);
        //数据长度
        int rlen = data.mdataLen;
        QTableWidgetItem* lenItem = new QTableWidgetItem(QString("%1").arg(rlen));
        lenItem->setTextAlignment(Qt::AlignCenter);
        if(erridx ==i)
        {
            lenItem->setForeground(QBrush(QColor(255,0,0)));
        }
        else {
            lenItem->setForeground(QBrush(QColor(0,0,0)));
        }
        ui->tableWidget_recv->setItem(i,3,lenItem);
        //数据值
        QTableWidgetItem* dataItem = new QTableWidgetItem();
        QString dateHex = data.data.toHex(' ').toUpper();
        dataItem->setText(dateHex);
        dataItem->setTextAlignment(Qt::AlignCenter);
        if(erridx ==i)
        {
            dataItem->setForeground(QBrush(QColor(255,0,0)));
        }
        else {
            dataItem->setForeground(QBrush(QColor(0,0,0)));
        }
        ui->tableWidget_recv->setItem(i,4,dataItem);
    }
    QModelIndex index = ui->tableWidget_recv->model()->index(showCount - 1,0);
    ui->tableWidget_recv->setCurrentIndex(index);
    ui->tableWidget_recv->scrollTo(index);
#endif
}

void wid_msgmonitor::setASendShow(QQueue<RECVDATA> queue_data)
{
#if 1
    erridx = -1;
    ui->tabwid_send->setRowCount(0);
    ui->tabwid_send->setRowCount(100);
    int showCount = queue_data.size();
    for(int i = 0;i < showCount;i++)
    {
        //显示到表格
        RECVDATA data = queue_data.at(i);
        //序号
        QTableWidgetItem* idxItem = new QTableWidgetItem(QString("%1").arg(data.msgIdx + 1));
        idxItem->setTextAlignment(Qt::AlignCenter);
        if(data.err)
        {
            erridx = data.errc;
            idxItem->setForeground(QBrush(QColor(255,0,0)));
        }
        else {
            idxItem->setForeground(QBrush(QColor(0,0,0)));
        }
        ui->tabwid_send->setItem(i,0,idxItem);
        //发送时间
        unsigned long long  time = data.atime;
        QTableWidgetItem* sendTimeItem = new QTableWidgetItem(QString("%1").arg(time));
        sendTimeItem->setTextAlignment(Qt::AlignCenter);
        if(data.err)
        {
            sendTimeItem->setForeground(QBrush(QColor(255,0,0)));
            sendTimeItem->setText(QString("%1").arg(time * 2));
        }
        else {
            sendTimeItem->setForeground(QBrush(QColor(0,0,0)));
        }
        ui->tabwid_send->setItem(i,1,sendTimeItem);
        //通道号
        int rch = data.ch;
        QTableWidgetItem* chItem = new QTableWidgetItem(QString("%1").arg(rch));
        chItem->setTextAlignment(Qt::AlignCenter);
        if(data.err)
        {
            chItem->setForeground(QBrush(QColor(255,0,0)));
        }
        else {
            chItem->setForeground(QBrush(QColor(0,0,0)));
        }
        ui->tabwid_send->setItem(i,2,chItem);
        //数据长度
        int rlen = data.mdataLen;
        QTableWidgetItem* lenItem = new QTableWidgetItem(QString("%1").arg(rlen));
        lenItem->setTextAlignment(Qt::AlignCenter);
        if(data.err)
        {
            lenItem->setForeground(QBrush(QColor(255,0,0)));
        }
        else {
            lenItem->setForeground(QBrush(QColor(0,0,0)));
        }
        ui->tabwid_send->setItem(i,3,lenItem);
        //数据值
        QTableWidgetItem* dataItem = new QTableWidgetItem();
        QString dateHex = data.mdata.toHex(' ').toUpper();
        dataItem->setText(dateHex);
        dataItem->setTextAlignment(Qt::AlignCenter);
        if(data.err)
        {
            dataItem->setForeground(QBrush(QColor(255,0,0)));
        }
        else {
            dataItem->setForeground(QBrush(QColor(0,0,0)));
        }
        ui->tabwid_send->setItem(i,4,dataItem);
        if(erridx != -1)
        {
            Dlog_info* mlog = new Dlog_info(2,"数据帧时间戳异常!");
            mlog->show();
            break;
        }
    }
    QModelIndex index = ui->tabwid_send->model()->index(showCount - 1,0);
    ui->tabwid_send->setCurrentIndex(index);
    ui->tabwid_send->scrollTo(index);
#endif
}

void wid_msgmonitor::setSendShow(QQueue<RECVDATA> queue_data)
{
#if 1
    erridx = -1;
    ui->tabwid_send->setRowCount(0);
    ui->tabwid_send->setRowCount(100);
    int showCount = queue_data.size();
    for(int i = 0;i < showCount;i++)
    {
        //显示到表格
        RECVDATA data = queue_data.at(i);
        //序号
        QTableWidgetItem* idxItem = new QTableWidgetItem(QString("%1").arg(data.msgIdx));
        idxItem->setTextAlignment(Qt::AlignCenter);
        if(data.err)
        {
            erridx = data.errc;
            idxItem->setForeground(QBrush(QColor(255,0,0)));
        }
        else {
            idxItem->setForeground(QBrush(QColor(0,0,0)));
        }
        ui->tabwid_send->setItem(i,0,idxItem);
        //接收时间
        qint64 time = data.time.value();
        QTableWidgetItem* sendTimeItem = new QTableWidgetItem(QString("%1").arg(time));
        sendTimeItem->setTextAlignment(Qt::AlignCenter);
        if(data.err)
        {
            sendTimeItem->setForeground(QBrush(QColor(255,0,0)));
            sendTimeItem->setText(QString("%1").arg(time * 2));
        }
        else {
            sendTimeItem->setForeground(QBrush(QColor(0,0,0)));
        }
        ui->tabwid_send->setItem(i,1,sendTimeItem);
        //通道号
        int rch = data.ch;
        QTableWidgetItem* chItem = new QTableWidgetItem(QString("%1").arg(rch));
        chItem->setTextAlignment(Qt::AlignCenter);
        if(data.err)
        {
            chItem->setForeground(QBrush(QColor(255,0,0)));
        }
        else {
            chItem->setForeground(QBrush(QColor(0,0,0)));
        }
        ui->tabwid_send->setItem(i,2,chItem);
        //数据长度
        int rlen = data.mdataLen;
        QTableWidgetItem* lenItem = new QTableWidgetItem(QString("%1").arg(rlen));
        lenItem->setTextAlignment(Qt::AlignCenter);
        if(data.err)
        {
            lenItem->setForeground(QBrush(QColor(255,0,0)));
        }
        else {
            lenItem->setForeground(QBrush(QColor(0,0,0)));
        }
        ui->tabwid_send->setItem(i,3,lenItem);
        //数据值
        QTableWidgetItem* dataItem = new QTableWidgetItem();
        QString dateHex = data.mdata.toHex(' ').toUpper();
        dataItem->setText(dateHex);
        dataItem->setTextAlignment(Qt::AlignCenter);
        if(data.err)
        {
            dataItem->setForeground(QBrush(QColor(255,0,0)));
        }
        else {
            dataItem->setForeground(QBrush(QColor(0,0,0)));
        }
        ui->tabwid_send->setItem(i,4,dataItem);
        if(erridx != -1)
        {
            Dlog_info* mlog = new Dlog_info(2,"数据帧时间戳异常!");
            mlog->show();
            break;
        }
    }
    QModelIndex index = ui->tabwid_send->model()->index(showCount - 1,0);
    ui->tabwid_send->setCurrentIndex(index);
    ui->tabwid_send->scrollTo(index);
#endif
}

void wid_msgmonitor::setRecvShow(QQueue<RECVDATA> queue_data)
{
#if 1
    int showCount = queue_data.size();
    for(int i = 0;i < showCount;i++)
    {
        //显示到表格
        RECVDATA data = queue_data.at(i);
        //序号
        QTableWidgetItem* idxItem = new QTableWidgetItem(QString("%1").arg(data.msgIdx));
        idxItem->setTextAlignment(Qt::AlignCenter);
        if(erridx ==i)
        {
            idxItem->setForeground(QBrush(QColor(255,0,0)));
        }
        else {
            idxItem->setForeground(QBrush(QColor(0,0,0)));
        }
        ui->tableWidget_recv->setItem(i,0,idxItem);
        //接收时间
        qint64 time = data.time.value();
        QTableWidgetItem* sendTimeItem = new QTableWidgetItem(QString("%1").arg(time));
        sendTimeItem->setTextAlignment(Qt::AlignCenter);
        if(erridx ==i)
        {
            sendTimeItem->setForeground(QBrush(QColor(255,0,0)));
            sendTimeItem->setText(QString("%1").arg(time * 2));
        }
        else {
            sendTimeItem->setForeground(QBrush(QColor(0,0,0)));
        }
        ui->tableWidget_recv->setItem(i,1,sendTimeItem);
        //通道号
        int rch = data.ch;
        QTableWidgetItem* chItem = new QTableWidgetItem(QString("%1").arg(rch));
        chItem->setTextAlignment(Qt::AlignCenter);
        if(erridx ==i)
        {
            chItem->setForeground(QBrush(QColor(255,0,0)));
        }
        else {
            chItem->setForeground(QBrush(QColor(0,0,0)));
        }
        ui->tableWidget_recv->setItem(i,2,chItem);
        //数据长度
        int rlen = data.dataLen;
        QTableWidgetItem* lenItem = new QTableWidgetItem(QString("%1").arg(rlen));
        lenItem->setTextAlignment(Qt::AlignCenter);
        if(erridx ==i)
        {
            lenItem->setForeground(QBrush(QColor(255,0,0)));
        }
        else {
            lenItem->setForeground(QBrush(QColor(0,0,0)));
        }
        ui->tableWidget_recv->setItem(i,3,lenItem);
        //数据值
        QTableWidgetItem* dataItem = new QTableWidgetItem();
        QString dateHex = data.mdata.toHex(' ').toUpper();
        dataItem->setText(dateHex);
        dataItem->setTextAlignment(Qt::AlignCenter);
        if(erridx ==i)
        {
            dataItem->setForeground(QBrush(QColor(255,0,0)));
        }
        else {
            dataItem->setForeground(QBrush(QColor(0,0,0)));
        }
        ui->tableWidget_recv->setItem(i,4,dataItem);
    }
    QModelIndex index = ui->tableWidget_recv->model()->index(showCount - 1,0);
    ui->tableWidget_recv->setCurrentIndex(index);
    ui->tableWidget_recv->scrollTo(index);
#endif
}

void wid_msgmonitor::slot_sendInfo(int ch, int datalen, QByteArray bytebuffer,QVariant info)
{
    if(send_idx > 100)
    {
        send_idx = 0;
    }
    //序号
    QTableWidgetItem* idxItem = new QTableWidgetItem(QString("%1").arg(send_idx + 1));
    idxItem->setTextAlignment(Qt::AlignCenter);
    idxItem->setForeground(QBrush(QColor(0,0,0)));
    ui->tabwid_send->setItem(send_idx,0,idxItem);
    //发送时间
    int temp =0;
    QRandomGenerator* rng = QRandomGenerator::global();
    qint64 time =  qtm[idx];//;QDateTime::currentMSecsSinceEpoch();
    if(mtype==1)
    {
        time += rng->bounded(800,999);
    }
    QString stime = TimeToStr(time);
    QTableWidgetItem* sendTimeItem = new QTableWidgetItem(QString("%1").arg(stime));
    sendTimeItem->setTextAlignment(Qt::AlignCenter);
    sendTimeItem->setForeground(QBrush(QColor(0,0,0)));
    ui->tabwid_send->setItem(send_idx,1,sendTimeItem);
    //通道号
    QTableWidgetItem* chItem = new QTableWidgetItem(QString("%1").arg(ch));
    chItem->setTextAlignment(Qt::AlignCenter);
    chItem->setForeground(QBrush(QColor(0,0,0)));
    ui->tabwid_send->setItem(send_idx,2,chItem);
    //数据长度
    QTableWidgetItem* lenItem = new QTableWidgetItem(QString("%1").arg(2096/*bytebuffer.size()*/));
    lenItem->setTextAlignment(Qt::AlignCenter);
    lenItem->setForeground(QBrush(QColor(0,0,0)));
    ui->tabwid_send->setItem(send_idx,3,lenItem);
    //数据值
    int rdata = QRandomGenerator::global()->bounded(2048);
    QTableWidgetItem* dataItem = new QTableWidgetItem(QString("%1").arg(rdata));
    dataItem->setTextAlignment(Qt::AlignCenter);
    dataItem->setForeground(QBrush(QColor(0,0,0)));
    ui->tabwid_send->setItem(send_idx,4,dataItem);
    send_idx++;
}


//纳秒时间转换为时分秒毫秒微妙
QString wid_msgmonitor::TimeToStr(LONGLONG LTime)
{
    /*  ULONGLONG lms = LTime/1000000; //转换为MS
    int ius= LTime-lms*1000000;    //剩余us

    int lus = ius/1000;   //us
    int ins = ius - lus*1000;
    QDateTime dtm;
    dtm.setMSecsSinceEpoch(lms);
    QString sr;
    sr = dtm.toString("hh:mm:ss.zzz");
    sr += QString(".%1.%2").arg(lus,3,10,QLatin1Char('0')).arg(ins,3,10,QLatin1Char('0'));
*/
    ULONGLONG DATY = 0x4E94914F0000;//天
    ULONGLONG HOURS = 0x34630B8A000; // 是
    ULONGLONG MINUTES = 0xDF8475800; //分
    ULONGLONG SECONDS = 0x3B9ACA00;//秒
    ULONGLONG MS		=	1000000;
    ULONGLONG	 US		=	1000;
    ULONGLONG temp[5];

    QString sr;
    DWORD iTime[7];//时分秒毫秒微妙
    iTime[0] = LTime/DATY;

    temp[0] = LTime - iTime[0]*DATY;
    iTime[1] = temp[0]/HOURS;  //时

    temp[1] = temp[0] - iTime[1]*HOURS;
    iTime[2] = temp[1] /MINUTES;//分

    temp[2] = temp[1] - iTime[2]*MINUTES;
    iTime[3] = temp[2] /SECONDS;//秒

    temp[3] = temp[2] - iTime[3]*SECONDS;
    iTime[4] = temp[3]/MS;      //毫秒

    temp[5] = temp[3]-iTime[4]*MS; //微妙
    iTime[5] = temp[5]/1000; //微妙

    iTime[6] = temp[5] - iTime[5]*1000;//纳秒

    sr = QString("%1-%2-%3-%4-%5-%6").arg(iTime[1],3,10,QLatin1Char('0')).arg(iTime[2],3,10,QLatin1Char('0')).arg(iTime[3],3,10,QLatin1Char('0')).arg(iTime[4],3,10,QLatin1Char('0')).arg(iTime[5],3,10,QLatin1Char('0')).arg(iTime[6],3,10,QLatin1Char('0'));
    return sr;
}

void wid_msgmonitor::on_pbtn_clearsend_clicked()
{
    ui->tabwid_send->setRowCount(0);
    ui->tabwid_send->setRowCount(100);
    if(mtype == 0)
        emit sig_clearSend();
}

void wid_msgmonitor::on_pbtn_clearrecv_clicked()
{
    ui->tableWidget_recv->setRowCount(0);
    ui->tableWidget_recv->setRowCount(100);
    if(mtype == 0)
        emit sig_clearRecv();
}

void wid_msgmonitor::setSendClear(bool t)
{
    if(t)
    {
        ui->tabwid_send->setEnabled(false);
    }
    else
    {
        ui->tabwid_send->setEnabled(true);
    }
}
