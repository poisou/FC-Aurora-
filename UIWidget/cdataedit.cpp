#include "cdataedit.h"
#include "ui_cdataedit.h"

cDataEdit::cDataEdit(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::cDataEdit)
{
    ui->setupUi(this);
    this->setAttribute(Qt::WA_DeleteOnClose);
}

cDataEdit::~cDataEdit()
{
    qvec.clear();
    delete ui;
}

void cDataEdit::Init(int len,char*data)
{
    if(data==nullptr)
    {
        ui->lineEdit->setText(QString::number(len));
        return ;
    }
    m_iBLen = 1;
    m_Ardata.clear();
    m_Ardata.resize(len);

  //  m_Ardata.append(data,len);
    memcpy(m_Ardata.data(),data,len);
    m_ilen = len;
    int btlen = (ui->comboBox->currentIndex()+1)*8;
    ui->lineEdit->setText(QString::number(m_ilen));
    ui->lineEdit_2->setText("0");
    m_iPage = m_ilen/512;
    if(m_iPage*512<m_ilen)
    {
        m_iPage += 1;
    }

    for(int i = 0;i<m_iPage;i++)
    {
        ui->comboBox_3->addItem("<"+QString::number(i+1)+">");
    }

    ui->label_6->setText("页数"+QString::number(m_iPage));

    if(m_iPage>0)
    {
        int ln = m_ilen - ui->comboBox_3->currentIndex()*512;
        len = ln>512?512:ln;
    }

    if(btlen<16)
    {      
        InitOneByte(len,m_Ardata.data());
    }
    else if(btlen>32)
    {
        InitfourByte(len,data);
    }
    else
    {
        InitTwoByte(len,data);
    }
}

void cDataEdit::InitOneByte(int len,char*data)
{
    int RowCont = 0;
    int icom = 0;
    while ( ui->tableWidget->rowCount()) {
        ui->tableWidget->removeRow(0);
    }
    if(ui->comboBox_3->currentIndex() == -1)
    {
        return;
    }
    char*pdata = data+ui->comboBox_3->currentIndex()*512;

    int bhex = ui->comboBox_2->currentIndex();
    qvec.clear();
    for(int i = 0;i<len;i++)
    {
        if(icom%8 == 0)
        {
            RowCont=ui->tableWidget->rowCount();
            ui->tableWidget->insertRow(RowCont);
            icom = 0;
            QLineEdit *pEdit = new QLineEdit();
            if(bhex>0)
            {
                pEdit->setText(QString("%1").arg(pdata[i]&0xff,2,16,QLatin1Char('0')));
            }
            else {
                pEdit->setText(QString::number(pdata[i]&0xff));
            }

            qvec.append(pEdit);
            ui->tableWidget->setCellWidget(RowCont,icom,pEdit);
            icom++;
        }
        else
        {
            QLineEdit *pEdit = new QLineEdit();
            if(bhex>0)
            {
                pEdit->setText(QString("%1").arg(pdata[i]&0xff,2,16,QLatin1Char('0')));
            }
            else
            {
                pEdit->setText(QString::number(pdata[i]&0xff));
            }

            qvec.append(pEdit);
            ui->tableWidget->setCellWidget(RowCont,icom,pEdit);
            icom++;
        }
    }
}

void cDataEdit::InitTwoByte(int len,char*data)
{
    int RowCont = 0;
    int icom = 0;
    while ( ui->tableWidget->rowCount()) {
        ui->tableWidget->removeRow(0);
    }
    qvec.clear();
    int bhex = ui->comboBox_2->currentIndex();

    int idex = ui->comboBox_3->currentIndex()*512*m_iBLen;
    if(idex<0)
    {
        return;
    }
  //  char*data0 = (char*)m_Ardata.data();
    data = data+idex;

    short*pdata = (short*)data;
    for(int i = 0;i<len;i++)
    {
        if(icom%8 == 0)
        {
            RowCont=ui->tableWidget->rowCount();
            ui->tableWidget->insertRow(RowCont);
            icom = 0;
            QLineEdit *pEdit = new QLineEdit();

            if(bhex>0)
            {
                pEdit->setText(QString("%1").arg(pdata[i]&0xffff,4,16,QLatin1Char('0')));
            }
            else
            {
                pEdit->setText(QString::number(pdata[i]&0xffff));
            }

            qvec.append(pEdit);
            ui->tableWidget->setCellWidget(RowCont,icom,pEdit);
            icom++;
        }
        else
        {
            QLineEdit *pEdit = new QLineEdit();
            if(bhex>0)
            {
                pEdit->setText(QString("%1").arg(pdata[i]&0xffff,4,16,QLatin1Char('0')));
            }
            else
            {
                pEdit->setText(QString::number(pdata[i]&0xffff));
            }
            qvec.append(pEdit);
            ui->tableWidget->setCellWidget(RowCont,icom,pEdit);
            icom++;
        }
    }
}

void cDataEdit::InitfourByte(int len,char*data)
{
    int RowCont = 0;
    int icom = 0;
    while ( ui->tableWidget->rowCount()) {
        ui->tableWidget->removeRow(0);
    }
    qvec.clear();
    int bhex = ui->comboBox_2->currentIndex();
    if(ui->comboBox_3->currentIndex()<0)
    {
        return;
    }
    data = data+(ui->comboBox_3->currentIndex()*512*m_iBLen);

    int*pdata = (int*)data;

    for(int i = 0;i<len;i++)
    {
        if(icom%8 == 0)
        {
            RowCont=ui->tableWidget->rowCount();
            ui->tableWidget->insertRow(RowCont);
            icom = 0;
            QLineEdit *pEdit = new QLineEdit();
            if(bhex>0)
            {
                pEdit->setText(QString("%1").arg(pdata[i]&0XFFFFFFFF,8,16,QLatin1Char('0')));
            }
            else
            {
                pEdit->setText(QString::number(pdata[i]&0XFFFFFFFF));
            }
            qvec.append(pEdit);
            ui->tableWidget->setCellWidget(RowCont,icom,pEdit);
            icom++;
        }
        else
        {
            QLineEdit *pEdit = new QLineEdit();
            if(bhex>0)
            {
                pEdit->setText(QString("%1").arg(pdata[i]&0XFFFFFFFF,8,16,QLatin1Char('0')));
            }
            else
            {
                pEdit->setText(QString::number(pdata[i]&0XFFFFFFFF));
            }
            qvec.append(pEdit);
            ui->tableWidget->setCellWidget(RowCont,icom,pEdit);
            icom++;
        }
    }
}

void cDataEdit::on_comboBox_currentIndexChanged(int index)
{
    m_iBLen = 1;
    if(index >0)
    {
        m_iBLen = index*2;
    }
    int len = m_ilen;

    m_iPage = m_ilen/(512*m_iBLen);
    if((m_iPage*512*m_iBLen)<m_ilen)
    {
        m_iPage += 1;
    }
    ui->comboBox_3->clear();
    for(int i = 0;i<m_iPage;i++)
    {
        ui->comboBox_3->addItem("<"+QString::number(i+1)+">");
    }
    ui->label_6->setText("页数>>"+QString::number(m_iPage));

    if(m_iPage>0)
    {
        int ln = m_ilen - ui->comboBox_3->currentIndex()*512*m_iBLen;
        ln = ln/m_iBLen;
        int itn = ln;
        if(ln*m_iBLen<itn)
        {
            ln++;
        }
        len = ln>512?512:ln;

    }
    switch(index)
    {
    case 0:
        InitOneByte(len,m_Ardata.data());
        break;
    case 1:
        InitTwoByte(len,m_Ardata.data());
        break;
    case 2:
        InitfourByte(len,m_Ardata.data());
        break;
    default:break;
    }
}

void cDataEdit::on_comboBox_2_currentIndexChanged(int index)
{
    int len = m_ilen;
    ui->comboBox_3->setCurrentText("1");
    if(m_iPage>0)
    {
        int ln = m_ilen - ui->comboBox_3->currentIndex()*512*m_iBLen;
        ln = ln/m_iBLen;
        int itn = ln;
        if(ln*m_iBLen<itn)
        {
            ln++;
        }
        len = ln>512?512:ln;

    }
    switch(ui->comboBox->currentIndex())
    {
    case 0:
        InitOneByte(len,m_Ardata.data());
        break;
    case 1:
        InitTwoByte(len,m_Ardata.data());
        break;
    case 2:
        InitfourByte(len,m_Ardata.data());
        break;
    default:break;
    }
}

//!生成
void cDataEdit::on_pushButton_clicked()
{
     int iaddValue = ui->lineEdit_4->text().toInt();//步进制
     int istartValue = ui->lineEdit_3->text().toInt();
     m_ilen = ui->lineEdit->text().toInt();
     short *spDatabuf;
     int *ipDatabuf;
     char *cDatabuf;

     int len = m_ilen;
     if(m_iPage>0)
     {
         int ln = m_ilen - ui->comboBox_3->currentIndex()*512*m_iBLen;
         ln = ln/m_iBLen;

         int itn = ln;
         ln = ln/m_iBLen;
         if(ln*m_iBLen<itn)
         {
             ln++;
         }
         len = ln>512?512:ln;

     }

     int index = ui->comboBox->currentIndex();//字长
     cDatabuf = (char*)m_Ardata.data();
     cDatabuf = cDatabuf+ui->comboBox_3->currentIndex()*512*m_iBLen;
     int cnt = 0;
     uchar uc = 0;
     switch(index)
     {
     case 0:      
         for(int i=0;i<m_ilen;i++)
         {
             if(i == 0)
             {
                 uc = istartValue;
             }
             else
                uc = istartValue+iaddValue;
             cDatabuf[i] = uc&0xff;
             istartValue = cDatabuf[i];
         }
         InitOneByte(len,m_Ardata.data());
         break;
     case 1:
         spDatabuf = (short*)cDatabuf;
         cnt = m_ilen/2;
         if(cnt*2<m_ilen)
         {
             cnt++;
         }
         for(int i=0;i<cnt;i++)
         {
             if(i == 0)
             {
                 spDatabuf[i] = istartValue;
             }
             else
                 spDatabuf[i] = istartValue+iaddValue;
          //   spDatabuf[i] = istartValue+iaddValue;
             spDatabuf[i] &= 0xffff;
             istartValue = spDatabuf[i];
         }
         InitTwoByte(len,m_Ardata.data());
         break;
     case 2:
         ipDatabuf = (int*)cDatabuf;
         cnt = m_ilen/4;
         if(cnt*4<m_ilen)
         {
             cnt++;
         }
         for(int i=0;i<cnt;i++)
         {
             if(i == 0)
             {
                 ipDatabuf[i] = istartValue;
             }
             else
                 ipDatabuf[i] = istartValue+iaddValue;
             ipDatabuf[i] &= 0xffffffff;
             istartValue = ipDatabuf[i];
         }
         InitfourByte(len,m_Ardata.data());
         break;
     default:break;
     }

    emit sigData(m_ilen,m_Ardata.data());
}


void cDataEdit::on_comboBox_3_currentIndexChanged(int index)
{
    int len = m_ilen;
    if(m_iPage>0)
    {
        int ln = m_ilen - ui->comboBox_3->currentIndex()*512*m_iBLen;
        int itn = ln;
        ln = ln/m_iBLen;
        if(ln*m_iBLen<itn)
        {
            ln++;
        }
        ui->lineEdit_2->setText(QString::number(ui->comboBox_3->currentIndex()*512*m_iBLen));
        len = ln>512?512:ln;

    }
    int index1 = ui->comboBox->currentIndex();//字长
    switch(index1)
    {
    case 0:
        InitOneByte(len,m_Ardata.data());
        break;
    case 1:
        InitTwoByte(len,m_Ardata.data());
        break;
    case 2:
        InitfourByte(len,m_Ardata.data());
        break;
    default:break;
    }
}

void cDataEdit::on_lineEdit_textEdited(const QString &arg1)
{
    m_ilen = arg1.toUInt();
    m_iPage = m_ilen/(512*m_iBLen);
    if((m_iPage*512*m_iBLen)<m_ilen)
    {
        m_iPage += 1;
    }
    ui->comboBox_3->clear();
    for(int i = 0;i<m_iPage;i++)
    {
        ui->comboBox_3->addItem("<"+QString::number(i+1)+">");
    }
    ui->label_6->setText("页数>>"+QString::number(m_iPage));
}

void cDataEdit::on_lineEdit_5_editingFinished()
{
    int idex= ui->lineEdit_5->text().toInt();
    if(idex<=(ui->lineEdit_2->text().toInt()+512))
    {
        idex = idex-ui->lineEdit_2->text().toInt();
        qvec[idex]->setStyleSheet("background: #0078d7; color: white;");
    }
}

void cDataEdit::on_pbnOK_clicked()
{
    m_ilen = ui->lineEdit->text().toInt();
    short *spDatabuf;
    int *ipDatabuf;
    char *cDatabuf;

    int len = m_ilen;
    if(m_iPage>0)
    {
        int ln = m_ilen - ui->comboBox_3->currentIndex()*512*m_iBLen;
        ln = ln/m_iBLen;

        int itn = ln;
        ln = ln/m_iBLen;
        if(ln*m_iBLen<itn)
        {
            ln++;
        }
        len = ln>512?512:ln;
    }

    int index = ui->comboBox->currentIndex();//字长
    cDatabuf = (char*)m_Ardata.data();
    cDatabuf = cDatabuf+ui->comboBox_3->currentIndex()*512*m_iBLen;
    int cnt = 0;
    QString sValue;
    switch(index)
    {
    case 0:
        for(int i=0;i<len;i++)
        {
            if(ui->comboBox_2->currentIndex()>0)
            {
                sValue = qvec.at(i)->text();
                cDatabuf[i] = sValue.toInt(nullptr,16)&0xff;
            }
            else
            {
                cDatabuf[i] = qvec.at(i)->text().toInt()&0xff;
            }
        }
        break;
    case 1:
        spDatabuf = (short*)cDatabuf;
        cnt = m_ilen/2;
        if(cnt*2<m_ilen)
        {
            cnt++;
        }
        for(int i=0;i<cnt;i++)
        {
            if(ui->comboBox_2->currentIndex()>0)
            {
                sValue = qvec.at(i)->text();
                spDatabuf[i] = sValue.toInt(nullptr,16)&0xffff;
            }
            else
            {
                spDatabuf[i] = qvec.at(i)->text().toInt()&0xffff;
            }
        }
        break;
    case 2:
        ipDatabuf = (int*)cDatabuf;
        cnt = m_ilen/4;
        if(cnt*4<m_ilen)
        {
            cnt++;
        }
        for(int i=0;i<cnt;i++)
        {
            if(ui->comboBox_2->currentIndex()>0)
            {
                sValue = qvec.at(i)->text();
                ipDatabuf[i] = sValue.toInt(nullptr,16)&0xffffffff;
            }
            else
            {
                ipDatabuf[i] = qvec.at(i)->text().toInt()&0xffffffff;
            }
        }
        break;
    default:break;
    }

   emit sigData(m_ilen,m_Ardata.data());
}
