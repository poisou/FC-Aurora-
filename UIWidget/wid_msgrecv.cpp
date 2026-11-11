#include "wid_msgrecv.h"
#include "ui_wid_msgrecv.h"

wid_msgrecv::wid_msgrecv(qint64 ipriID,QWidget *parent) :
    QWidget(parent),
    ui(new Ui::wid_msgrecv)
{
    ui->setupUi(this);
    mipriID = ipriID;
    ui->plainTextEdit->setEnabled(false);
    ui->plainTextEdit_2->setEnabled(false);
}

wid_msgrecv::~wid_msgrecv()
{
    delete ui;
}

void wid_msgrecv::on_pushButton_clicked()
{
    ui->plainTextEdit->clear();
    ui->plainTextEdit_2->clear();
}
