#include "wid_action.h"
#include "ui_wid_action.h"

wid_action::wid_action(int row,QString text,QWidget *parent) :
    QWidget(parent),
    ui(new Ui::wid_action)
{
    ui->setupUi(this);
    mrow = row;
    mtext = text;
}

wid_action::~wid_action()
{
    delete ui;
}

void wid_action::on_pushButton_clicked()
{
    //编辑
    //弹出对应的ICD协议的列表
    emit sig_editICD(mrow);
}

void wid_action::on_pushButton_2_clicked()
{
    //删除
    //1.弹窗提示是否删除
    Dlog_info* mdlog = new Dlog_info(0,mtext);
    connect(mdlog,&Dlog_info::sig_delItem,this,&wid_action::slot_delICD);
    mdlog->exec();
}

void wid_action::slot_delICD()
{
    emit sig_delICD(mrow);
}
