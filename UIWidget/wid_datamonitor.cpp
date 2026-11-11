#include "wid_datamonitor.h"
#include "ui_wid_datamonitor.h"

wid_datamonitor::wid_datamonitor(qint64 ipriID,QWidget *parent) :
    QWidget(parent),
    ui(new Ui::wid_datamonitor)
{
    //数据监控
    ui->setupUi(this);
    mipriID = ipriID;
    Init();
}

wid_datamonitor::~wid_datamonitor()
{
    delete ui;
}

void wid_datamonitor::Init()
{
    ui->tabwid_datamonitor->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    ui->tabwid_datamonitor->setEditTriggers(QAbstractItemView::NoEditTriggers);
    ui->tabwid_datamonitor->verticalHeader()->setVisible(false);
    ui->tabwid_datamonitor->horizontalHeader()->setStyleSheet(R"(
                                                      QHeaderView::section{
                                                      background-color:rgb(255,255,255);
                                                      color:rgb(192,203,210);
                                                      font: 75 10pt "微软雅黑";
                                                      border-top:1px solid rgb(70,90,120);
                                                      border-bottom:1px solid rgb(70,90,120);
                                                      border-radius:1px;
                                                      }
                                                      )");
}
