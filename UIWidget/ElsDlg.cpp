#include <QMessageBox>
#include "ElsDlg.h"
#include "ui_ElsDlg.h"
#include "cdataedit.h"
ElsDlg::ElsDlg(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::ElsDlg)
{
    ui->setupUi(this);
    this->setWindowTitle("ELS配置");
    cDataEdit *pcDataEdit = new cDataEdit;
    connect(this, SIGNAL(sigInit(int,char*)), pcDataEdit, SLOT(Init(int,char*)));
    connect(pcDataEdit, SIGNAL(sigData(int,char*)), this, SLOT(EditData(int,char*)));
    connect(this, SIGNAL(sigRepOk()), pcDataEdit, SLOT(on_pbnOK_clicked()));

    ui->gridLayout_3->addWidget(pcDataEdit);

    LineEditList<<ui->lineEdit<<ui->lineEdit_2<<ui->lineEdit_3
               <<ui->lineEdit_4<<ui->lineEdit_5<<ui->lineEdit_6
              <<ui->lineEdit_7<<ui->lineEdit_8
             <<ui->lineEdit_9;

    m_dataBuffer=new unsigned char[2176];
    memset(m_dataBuffer,0,2176);

    emit sigInit(2096,(char*)m_dataBuffer+OFFSET);
    ui->pbnNew->setHidden(true);
    ui->pbnNew_2->setHidden(true);
    ui->pbnNew_5->setHidden(true);

    ui->treeWidget->setHidden(true);

    ui->label_12->setHidden(true);
    ui->spinBox_2->setHidden(true);
}

void ElsDlg::initUI(QString sch,MsgBlock cMsgBlock,int type)
{
    m_sch = sch;
    m_pCtrl = (WTContrl *)(PM()->GetWTCtrl(m_sch));
    ui->spinBox->setValue(2096);


    if(type==1)
    {
        ui->spinBox->setValue(cMsgBlock.sData.size()-OFFSET);
        if(cMsgBlock.sData.size()>OFFSET)
        emit sigInit(cMsgBlock.sData.size()-OFFSET,(char*)cMsgBlock.sData.data()+OFFSET);
    }

}

void ElsDlg::initUI(QString sch, QList<MsgBlock> list, int mode,int index)
{
    m_index=index;
    m_mode=mode;
    m_sch = sch;
    m_pCtrl = (WTContrl *)(PM()->GetWTCtrl(m_sch));
    //emit sigInit(2096,(char*)m_dataBuffer+OFFSET);
    m_BlockList=list;
    updateBlockTree();
    if(list.count()>0 &&m_mode)
    {
        QTreeWidgetItem *item = ui->treeWidget->topLevelItem(index);
        ui->treeWidget->setCurrentItem(item);
        m_cMsgBlock = list[index];

        unsigned char*data = (unsigned char*)m_cMsgBlock.sData.data();
        unsigned int *pMSGData=nullptr;
        pMSGData=(unsigned int *)data;
        for(int i=0;i<LineEditList.count();i++)
        {
            LineEditList[i]->setText(QString("%1").arg(pMSGData[8+i],8,16,QLatin1Char('0')));
        }
        ui->spinBox->setValue(m_cMsgBlock.sData.size()-OFFSET);
        ui->spinBox_2->setValue(m_cMsgBlock.ITxCnt);
        memcpy(m_dataBuffer,m_cMsgBlock.sData.data(),m_cMsgBlock.sData.size());
        sigInit(m_cMsgBlock.sData.size()-OFFSET,(char*)m_dataBuffer+OFFSET);
    }



}

ElsDlg::~ElsDlg()
{
    delete m_dataBuffer;
    delete ui;
}

void ElsDlg::addBlockTree()
{
    QTreeWidgetItem *item=new QTreeWidgetItem();
    item->setText(0,m_cMsgBlock.sMsgID);
}

void ElsDlg::updateBlockTree()
{
    ui->treeWidget->clear();
    for(int i=0;i<m_BlockList.count();i++)
    {
        QTreeWidgetItem *item=new QTreeWidgetItem();
        item->setText(0,"ELS:"+QString::number(i));
        ui->treeWidget->addTopLevelItem(item);
    }
}

void ElsDlg::on_pbnReq_clicked()
{
    if(ui->spinBox->value()%4!=0)
    {
        QMessageBox::information(nullptr,"提示","数据长度不是4的倍数，请修改数据长度！");
        return;
    }
    //m_BlockList.clear();
    unsigned int *pMSGData=nullptr;
    int uLength=ui->spinBox->value()+18*4;
    pMSGData=(unsigned int *)m_dataBuffer;
    pMSGData[0] = 0x57544344;
    pMSGData[1] = 0x00000000;
    pMSGData[2] = (uLength / 4 - 18 + 6 + 4 + 1) << 16;//MSG Len + FC header:6 + ELS header:4 + CRC:1  (32位字个数)
    pMSGData[3] = 5; //消息类型：1 - 紧急消息	2 - 周期消息  3 - 事件消息  4 - 数据块消息  5 - 非ASM消息 6 - 非ASM周期消息
    pMSGData[4] = 0; //消息周期ms
    pMSGData[5] = uLength / 4 - 18 /*+ 6 + 1*/; //非ASM有效数据字长度(MSG Len + FC header:6 + CRC:1)
    pMSGData[6] = 3; //0-序列第一帧、1-序列中间帧、3-序列最后一帧
    pMSGData[7] = m_sch.split("CH").last().toUInt(); //板卡内Port号 [0~3]
    //FC header
    for(int i=0;i<LineEditList.count();i++)
    {
        uint value = LineEditList[i]->text().toUInt(nullptr,16);
        pMSGData[8 + i] = value;
        //        pMSGData[8 + 0] = value;//R_CTL(8) + D_ID(24)
        //        pMSGData[8 + 1] = (0x00 << 24) + 0x10001;//CS_CTL(8) + S_ID(24)
        //        pMSGData[8 + 2] = (0x01 << 24) + 0x000000;//TYPE(8) + F_CTL(24)
        //        pMSGData[8 + 3] = 0x00000000;//SEQ_ID(8) + DF_CTL(8) + SEQ_CNT(16)
        //        pMSGData[8 + 4] = 0x00010001;//OX_ID(16) + RX_ID(16)
        //        pMSGData[8 + 5] = 0x00000000;//Parameter
    }
    m_cMsgBlock.sData.clear();
    m_cMsgBlock.sData.append((char*)m_dataBuffer,uLength);
    m_cMsgBlock.periodType=5;
    m_cMsgBlock.ITxCnt=ui->spinBox_2->value();
    m_cMsgBlock.bTR=0;
    m_cMsgBlock.iTotal=0;

    if(m_mode==1)
    {
        m_BlockList[m_index]=m_cMsgBlock;
    }
    else
    {
        m_BlockList.append(m_cMsgBlock);
    }


    this->accept();
}

void ElsDlg::on_spinBox_valueChanged(int arg1)
{
    emit sigInit(arg1,(char*)m_dataBuffer+OFFSET);
}

void ElsDlg::EditData(int len, char *data)
{
    memcpy(m_dataBuffer+OFFSET,data,len);
}

void ElsDlg::on_pbnNew_clicked()
{
    unsigned int *pMSGData=nullptr;
    int uLength=ui->spinBox->value()+OFFSET;
    pMSGData=(unsigned int *)m_dataBuffer;
    pMSGData[0] = 0x57544344;
    pMSGData[1] = 0x00000000;
    pMSGData[2] = (uLength / 4 - 18 + 6 + 4 + 1) << 16;//MSG Len + FC header:6 + ELS header:4 + CRC:1  (32位字个数)
    pMSGData[3] = 5; //消息类型：1 - 紧急消息	2 - 周期消息  3 - 事件消息  4 - 数据块消息  5 - 非ASM消息 6 - 非ASM周期消息
    pMSGData[4] = 0; //消息周期ms
    pMSGData[5] = uLength / 4 - 18 + 6 + 1; //非ASM有效数据字长度(MSG Len + FC header:6 + CRC:1)
    pMSGData[6] = 3; //0-序列第一帧、1-序列中间帧、3-序列最后一帧
    pMSGData[7] = m_sch.split("CH").last().toUInt(); //板卡内Port号 [0~3]
    //FC header
    for(int i=0;i<LineEditList.count();i++)
    {
        uint value = LineEditList[i]->text().toUInt(nullptr,16);
        pMSGData[8 + i] = value;
        //        pMSGData[8 + 0] = value;//R_CTL(8) + D_ID(24)
        //        pMSGData[8 + 1] = (0x00 << 24) + 0x10001;//CS_CTL(8) + S_ID(24)
        //        pMSGData[8 + 2] = (0x01 << 24) + 0x000000;//TYPE(8) + F_CTL(24)
        //        pMSGData[8 + 3] = 0x00000000;//SEQ_ID(8) + DF_CTL(8) + SEQ_CNT(16)
        //        pMSGData[8 + 4] = 0x00010001;//OX_ID(16) + RX_ID(16)
        //        pMSGData[8 + 5] = 0x00000000;//Parameter
    }
    m_cMsgBlock.sData.clear();
    m_cMsgBlock.sData.append((char*)m_dataBuffer,uLength);
    m_cMsgBlock.periodType=5;
    m_cMsgBlock.ITxCnt=ui->spinBox_2->value();
    m_cMsgBlock.bTR=0;
    m_cMsgBlock.iTotal=0;

   m_BlockList.append(m_cMsgBlock);
   updateBlockTree();
}

void ElsDlg::on_pbnNew_2_clicked()  //修改
{
    if(ui->treeWidget->currentItem()==nullptr)
    {
        return;
    }

    unsigned int *pMSGData=nullptr;
    int uLength=ui->spinBox->value()+OFFSET;
    pMSGData=(unsigned int *)m_dataBuffer;
    pMSGData[0] = 0x57544344;
    pMSGData[1] = 0x00000000;
    pMSGData[2] = (uLength / 4 - 18 + 6 + 4 + 1) << 16;//MSG Len + FC header:6 + ELS header:4 + CRC:1  (32位字个数)
    pMSGData[3] = 5; //消息类型：1 - 紧急消息	2 - 周期消息  3 - 事件消息  4 - 数据块消息  5 - 非ASM消息 6 - 非ASM周期消息
    pMSGData[4] = 0; //消息周期ms
    pMSGData[5] = uLength / 4 - 18 + 6 + 1; //非ASM有效数据字长度(MSG Len + FC header:6 + CRC:1)
    pMSGData[6] = 3; //0-序列第一帧、1-序列中间帧、3-序列最后一帧
    pMSGData[7] = m_sch.split("CH").last().toUInt(); //板卡内Port号 [0~3]
    //FC header
    for(int i=0;i<LineEditList.count();i++)
    {
        uint value = LineEditList[i]->text().toUInt(nullptr,16);
        pMSGData[8 + i] = value;
        //        pMSGData[8 + 0] = value;//R_CTL(8) + D_ID(24)
        //        pMSGData[8 + 1] = (0x00 << 24) + 0x10001;//CS_CTL(8) + S_ID(24)
        //        pMSGData[8 + 2] = (0x01 << 24) + 0x000000;//TYPE(8) + F_CTL(24)
        //        pMSGData[8 + 3] = 0x00000000;//SEQ_ID(8) + DF_CTL(8) + SEQ_CNT(16)
        //        pMSGData[8 + 4] = 0x00010001;//OX_ID(16) + RX_ID(16)
        //        pMSGData[8 + 5] = 0x00000000;//Parameter
    }
    m_cMsgBlock.sData.clear();
    m_cMsgBlock.sData.append((char*)m_dataBuffer,uLength);
    m_cMsgBlock.periodType=5;
    m_cMsgBlock.ITxCnt=ui->spinBox_2->value();
    m_cMsgBlock.bTR=0;
    m_cMsgBlock.iTotal=0;

    QTreeWidgetItem *item = ui->treeWidget->currentItem();
    int index =item->text(0).split(":").last().toInt();
    m_BlockList[index]=m_cMsgBlock;
    updateBlockTree();
    QTreeWidgetItem *sitem =ui->treeWidget->topLevelItem(index);
    ui->treeWidget->setCurrentItem(sitem);
}

void ElsDlg::on_pbnNew_5_clicked()  //删除
{
    if(ui->treeWidget->currentItem()==nullptr)
    {
        return;
    }
    QTreeWidgetItem *item = ui->treeWidget->currentItem();
    int index =item->text(0).split(":").last().toInt();
    m_BlockList.removeAt(index);
    updateBlockTree();
}



void ElsDlg::on_treeWidget_itemClicked(QTreeWidgetItem *item, int column)
{
    for(int i=0;i<ui->treeWidget->topLevelItemCount();i++)
    {
        if(item==ui->treeWidget->topLevelItem(i))
        {
            m_cMsgBlock = m_BlockList[i];
            qDebug()<<"m_cMsgBlock data size"<<m_cMsgBlock.sData.size();
            unsigned char*data = (unsigned char*)m_cMsgBlock.sData.data();
            unsigned int *pMSGData=nullptr;
            pMSGData=(unsigned int *)data;
            for(int i=0;i<LineEditList.count();i++)
            {
                LineEditList[i]->setText(QString("%1").arg(pMSGData[8+i],8,16,QLatin1Char('0')));
            }
            ui->spinBox->setValue(m_cMsgBlock.sData.size()-OFFSET);
            ui->spinBox_2->setValue(m_cMsgBlock.ITxCnt);
            memcpy(m_dataBuffer,m_cMsgBlock.sData.data(),m_cMsgBlock.sData.size());
            sigInit(m_cMsgBlock.sData.size()-OFFSET,(char*)m_dataBuffer+OFFSET);
        }
    }
}

void ElsDlg::on_pbnNew_3_clicked()
{
    this->reject();
}
