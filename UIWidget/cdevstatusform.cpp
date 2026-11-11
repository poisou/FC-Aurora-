#include "cdevstatusform.h"
#include "ui_cdevstatusform.h"
#include <QDebug>

CDevStatusForm::CDevStatusForm(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::CDevStatusForm)
{
    ui->setupUi(this);

    ui->frame->setStyleSheet(".QFrame{background-color:white;}"
                                         ".QFrame:hover{background-color:#00a2e8;}");
    bPress = false;
}

CDevStatusForm::~CDevStatusForm()
{
    delete ui;
}

void CDevStatusForm::SetStatus(bool bstatus)
{
    if(!bstatus)
    {
        ui->label_3->setStyleSheet("QLabel{border-image: url(:/images/radio_normal.png);background:transparent}");
    //    qDebug()<<ui->label->text();
    }
    else
    {
        ui->label_3->setStyleSheet("QLabel{border-image: url(:/images/radio_selected.png);background:transparent}");
     //   this->setFocus();
    //    ui->frame->setStyleSheet(".QFrame{background-color:#00a2e8;}");
    //    bPress = true;
    }
}

void CDevStatusForm::SetPressStatus(bool bflg)
{
    if(!bflg)
    {
        ui->frame->setStyleSheet(".QFrame{background-color:white;}"
                                 ".QFrame:hover{background-color:#00a2e8;}");
        bPress = false;
    }
    else
    {
        ui->frame->setStyleSheet(".QFrame{background-color:#00a2e8;}");
    }
}

void CDevStatusForm::SetText(QString stext)
{
    QString sr = QString("%1").arg(stext.toUInt(),8,16,QLatin1Char('0'));
    ui->label->setText("ID:"+sr);
}

void CDevStatusForm::mousePressEvent(QMouseEvent *e)
{
    QString sid = ui->label->text();
    this->setFocus();
    emit MousePress(sid.right(sid.length()-3));
    ui->frame->setStyleSheet(".QFrame{background-color:#00a2e8;}");
    bPress = true;
}


void CDevStatusForm::focusInEvent(QFocusEvent *event)
{
    ui->frame->setStyleSheet(".QFrame{background-color:#00a2e8;}");
                                       //  ".QFrame:hover{border-image: url(:/new/1.png);background-color:transparent;}");
}

void CDevStatusForm::focusOutEvent(QFocusEvent *event)
{
    if(!bPress)
    {
        ui->frame->setStyleSheet(".QFrame{background-color:white;}"
                                             ".QFrame:hover{background-color:#00a2e8;}");
    }
}
