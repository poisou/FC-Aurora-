#include "txctrldialog.h"
#include "ui_txctrldialog.h"
#include <wtgl.h>
#include <IPluginV2.h>

TXCtrlDialog::TXCtrlDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::TXCtrlDialog)
{
    ui->setupUi(this);
    m_widget = nullptr;
 /*   QObject *obj1 = PM()->queryPlugin("WTTitleBarPlugin");
    if(obj1!=nullptr)
    {
        m_widget = ((IPluginV2 *)obj1)->Create(this,"wttitlebar2");
        installEventFilter(m_widget);
        //    connect(pTitleBar, SIGNAL(sigUdp(bool)), this, SLOT(SoltNetwork(bool)));
        m_widget->setFixedHeight(40);
        connect(this, SIGNAL(setTitle(QString)), m_widget, SLOT(setTitle(QString)));
        qobject_cast<QVBoxLayout *>(this->layout())->insertWidget(0,m_widget);
        emit setTitle("消息控制");
    }*/

   // setWindowFlags(Qt::FramelessWindowHint);
    ui->label_2->hide();
    ui->radioButton_2->hide();
    ui->lineEdit_2->hide();
}

TXCtrlDialog::~TXCtrlDialog()
{
    delete ui;
}

void TXCtrlDialog::on_radioButton_clicked()
{
    ui->lineEdit->setEnabled(true);
    ui->lineEdit_2->setEnabled(false);
}

void TXCtrlDialog::on_radioButton_2_clicked()
{
    ui->lineEdit->setEnabled(false);
    ui->lineEdit_2->setEnabled(true);
}

void TXCtrlDialog::on_pbnOK_clicked()
{
    m_iTXCnt = 0;
    if(ui->radioButton->isChecked())
    {
        m_iTXCnt = ui->lineEdit->text().toInt();
        if(m_iTXCnt>1)
        {
           isPer = false;
           m_iPerTm = 0;
        }
    }
    else
    {
        float fTXCnt = ui->lineEdit_2->text().toFloat(); //!p/s：每秒包数据
        if(fTXCnt>1)
        {
           isPer = true;
           float fper = ((float)fTXCnt)/1000.0;   //!毫秒发送包数
           if(fper<1)
           {
               m_iTXCnt = 1;
               m_iPerTm = fper*1000;
               if(0.0009<=(fper-0.009))
               {
                   m_iPerTm += 1;
               }
           }
           else
           {
               m_iPerTm = 1;
               m_iTXCnt = fper;
           }
        }
        else
        {
            m_iTXCnt = 1;
        }
    }
    this->accept();
}
