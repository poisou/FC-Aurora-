#include "SendElsDlg.h"
#include "ElsDlg.h"
#include "ui_SendElsDlg.h"
#include "txctrldialog.h"
SendElsDlg::SendElsDlg(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::SendElsDlg)
{
    ui->setupUi(this);

    ui->tableWidget->setEditTriggers(QAbstractItemView::NoEditTriggers);
    ui->tableWidget->horizontalHeader()->setStretchLastSection(true);

//    m_SendElsDataThread =new QMyThread();
//    m_SendElsDataThread->pThread=this;
//    m_SendElsDataThread->m_etype=WTPERSEND;

    QAction*act=new QAction();
    act->setText("ELS消息配置");
    m_CreateMsgMenu.addAction(act);
    connect(act,SIGNAL(triggered()),this,SLOT(slot_menu()));

//    act =new QAction();
//    act->setText("清空接收列表");
//    m_ClearMenu.addAction(act);
//    connect(act,SIGNAL(triggered()),this,SLOT(slot_menu()));

    ui->tableWidget->setEditTriggers(QAbstractItemView::NoEditTriggers);
    ui->tableWidget->horizontalHeader()->setStretchLastSection(true);

//    act=new QAction();
//        act->setText("删除");
//        m_CreateMsgMenu.addAction(act);
//        connect(act,SIGNAL(triggered()),this,SLOT(slot_menu()));

    connect(this,SIGNAL(SendFinish(int)),this,SLOT(slotSendFinish(int)),Qt::QueuedConnection);

}

SendElsDlg::~SendElsDlg()
{
//    m_SendElsDataThread->bRun=false;
//    QThread::msleep(50);
//    delete m_SendElsDataThread;
    delete ui;
}

void SendElsDlg::init(QString sch)
{
    m_sch=sch;
    QString str= m_sch.split("CH").last();
    this->setWindowTitle("ELS(port"+str+")");
    m_Pctrl=(WTContrl*)PM()->GetWTCtrl(m_sch);

    ui->tableWidget->horizontalHeader()->setVisible(true);
}

void SendElsDlg::slot_menu()
{
    QAction *act = qobject_cast<QAction*>(sender());
    QString name = act->text();
    if(name=="ELS消息配置")
    {
        ElsDlg dlg;
        dlg.initUI(m_sch,m_blockList,0,0);
        if(QDialog::Accepted==dlg.exec())
        {
            AddMsg(dlg.m_BlockList);
            m_blockList = dlg.m_BlockList;
        }
    }
    else if(name == "清空接收列表")
    {
        //clearData();
    }
}

void SendElsDlg::msgBtnClicked()
{
    QPushButton *btn = qobject_cast<QPushButton*>(sender());
    if(btn->text()=="发送")
    {
        TXCtrlDialog dlg;
        if(QDialog::Accepted == dlg.exec())
        {
            int id = btn->windowTitle().toInt();

            m_blockList[id].bTR=1;
            m_blockList[id].iTotal=0;
            m_blockList[id].ITxCnt= dlg.m_iTXCnt;
//            if(m_SendElsDataThread->bRun==false)
//            {
//                m_SendElsDataThread->start();
//            }
            btn->setText("停止");
        }
    }
    else if(btn->text()=="停止")
    {
        int id = btn->windowTitle().toInt();
        //sendMutex.lock();
        m_blockList[id].bTR=0;
        m_blockList[id].iTotal=0;
        btn->setText("发送");
    }
}

void SendElsDlg::slotSendFinish(int index)
{
    qDebug()<<"index is "<<index;
    QPushButton *btn = (QPushButton *)ui->tableWidget->cellWidget(index,3);
    ui->tableWidget->item(index,4)->setText(QString::number(m_blockList[index].iTotalRX));
    btn->setText("发送");
}

void SendElsDlg::sendElsDataThread()
{
    bool quit=false;
//    while(m_SendElsDataThread->bRun)
//    {
//        //foreach(int id,m_SendBlockMap.keys())
//        quit=true;
//        for(int i=0;i<m_blockList.count();i++)
//        {
//            MsgBlock block= m_blockList[i];
//            if(block.bTR==1)
//            {
//                if(block.ITxCnt>0 &&block.iTotal!=block.ITxCnt)
//                {
//                    qDebug()<<"send start"<<QDateTime::currentDateTime();
//                    int ret = m_Pctrl->upDateData(&block);
//                    if(ret==0)
//                    {
//                        qDebug()<<"send stop"<<QDateTime::currentDateTime();
//                        m_blockList[i].iTotal++;
//                        m_blockList[i].iTotalRX++;  //用于发送次数计数

//                        if(m_blockList[i].iTotal%100==0)
//                        {
//                            QThread::msleep(1);
//                        }
//                    }
//                    quit=false;
//                }
//                else
//                {
//                    //qDebug()<<"send fail"<<QDateTime::currentDateTime();
//                    m_blockList[i].iTotal=0;
//                    m_blockList[i].bTR=0;
//                    emit SendFinish(i);
//                }
//            }

//        }
//        if(quit)
//        {
//            m_SendElsDataThread->bRun=false;
//        }
//        //QThread::msleep(1);
//    }
//    for(int i=0;i<m_blockList.count();i++)
//    {
//        emit SendFinish(i);
//    }

//    qDebug()<<"sendElsDataThread quit";
}

void SendElsDlg::closeEvent(QCloseEvent *e)
{
    e->ignore();
    this->hide();
}

void SendElsDlg::AddMsg(QList<MsgBlock> list)
{
    int cout=ui->tableWidget->rowCount();
    for(int i=0;i<cout;i++)
        ui->tableWidget->removeRow(0);

    //ui->tableWidget->clear();
    int count = list.count();
    ui->tableWidget->setRowCount(count);
    for(int i=0;i<count;i++)
    {
        QTableWidgetItem *item=new QTableWidgetItem();
        item->setText("ELS");
        ui->tableWidget->setItem(i,0,item);

        int size = list[i].sData.size()-OFFSET;
        item=new QTableWidgetItem();
        item->setText(QString("%1").arg(list[i].sData.size()-OFFSET));
        ui->tableWidget->setItem(i,2,item);

        item=new QTableWidgetItem();
        QString strData;
        for(int x=0;x<size;x++)
        {
            strData+=QString("%1 ").arg(list[i].sData.data()[x+OFFSET],2,16,QLatin1Char('0'));
            if(x>4)
            {
                strData+="..";
                break;
            }

        }
        item->setText(strData);
        ui->tableWidget->setItem(i,1,item);

        //        item=new QTableWidgetItem();
        //        item->setText(QString("%1").arg(list[i].ITxCnt));
        //        ui->tableWidget->setItem(i,3,item);

        QPushButton *btn=new QPushButton();
        btn=new QPushButton("发送");
        btn->setWindowTitle(QString::number(i));
        ui->tableWidget->setCellWidget(i,3,btn);
        connect(btn,SIGNAL(clicked()),this,SLOT(msgBtnClicked()));

        item=new QTableWidgetItem();
        item->setText(QString::number(list[i].iTotalRX));
        ui->tableWidget->setItem(i,4,item);
    }

    m_blockList = list;
}

void SendElsDlg::MessageInfo(const MsgBlock *par_msg, int ilen, int pos)
{
    ui->textEdit->clear();
    int irow = pos/16;
    int icom = 0;
    QString sdata;
    QString sr;
    ilen = ilen-OFFSET;
    ui->label_2->setText("发送数据:"+QString::number(ilen));
    //  ilen = ilen>8192?8192:ilen;
    int inum = 0;
    for(int i=pos;i<ilen;i++)
    {
        if(icom%16==0)
        {
            if(i>0)
            {
                sdata += "\r\n";
                irow++;
            }
            sr = QString("%1: ").arg(irow*16,4,10,QLatin1Char('0'));

            sdata += sr;
            sr = QString("%1 ").arg(par_msg->sData.at(i+OFFSET)&0xff,2,16,QLatin1Char('0'));
            sdata += sr.toUpper();
            icom = 0;
        }
        else
        {
            sr = QString("%1 ").arg(par_msg->sData.at(i+OFFSET)&0xff,2,16,QLatin1Char('0'));
            sdata += sr.toUpper();
        }
        icom++;
        inum++;
        if(inum==8192)
        {
            break;
        }
    }
    ui->textEdit->insertPlainText(sdata);
}

void SendElsDlg::on_tableWidget_customContextMenuRequested(const QPoint &pos)
{
    m_CurModeIndex = ui->tableWidget->indexAt(pos);
    m_CreateMsgMenu.exec(QCursor::pos());
}

void SendElsDlg::on_tableWidget_itemClicked(QTableWidgetItem *item)
{
    if(item!=nullptr)
    {
        int row = ui->tableWidget->currentRow();
        //        mpClickItem= item;
        //        int id= mpClickItem->data(0).toInt();
        MsgBlock cMsgBlock = m_blockList[row];
        MessageInfo(&cMsgBlock,cMsgBlock.sData.count());
        //m_cMsgBlock = cMsgBlock;
    }
}

void SendElsDlg::on_tableWidget_itemDoubleClicked(QTableWidgetItem *item)
{
    if(item!=nullptr)
    {
        int index=item->row();
        ElsDlg dlg;

        dlg.initUI(m_sch,m_blockList,1,index);
        if(QDialog::Accepted==dlg.exec())
        {
            AddMsg(dlg.m_BlockList);

        }
    }
}
