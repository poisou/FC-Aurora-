#include "wid_fcdata.h"
#include "ui_wid_fcdata.h"

wid_FCData::wid_FCData(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::wid_FCData)
{
    ui->setupUi(this);
    Init();
}

wid_FCData::~wid_FCData()
{
    delete ui;
}

void wid_FCData::Init()
{
    ui->tableWidget->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    ui->tableWidget->verticalHeader()->setVisible(false);
    ui->tableWidget->verticalHeader()->setDefaultSectionSize(50);
    ui->tableWidget->horizontalHeader()->setStyleSheet(R"(
                                                       QHeaderView::section{
                                                       background-color:rgb(255,255,255);
                                                       color:rgb(0,0,0);
                                                       font: 75 10pt "微软雅黑";
                                                       border-top:1px solid rgb(70,90,120);
                                                       border-bottom:1px solid rgb(70,90,120);
                                                       border-radius:1px;
                                                       }
                                                       )");
    ui->spinBox_times->setEnabled(false);
}

void wid_FCData::updateTAB()
{
    ui->tableWidget->setUpdatesEnabled(false);
    ui->tableWidget->clearContents();
    ui->tableWidget->setRowCount(0);
    int start = m_currentPage * m_PageSize;
    int count = (m_totalPage == 0)? 0 : qMin(m_PageSize,vec_data.size() - start);
    ui->tableWidget->setRowCount(count);
    for(int i = 0;i < count;++i)
    {
        //帧序号
        QTableWidgetItem* idxitem = new QTableWidgetItem();
        QFont idxfont;
        idxfont.setPointSize(11);
        idxfont.setFamily("Microsoft YaHei");
        QString idxtext = QString("%1").arg(m_currentPage*m_PageSize+1+i);
        idxitem->setText(idxtext);
        idxitem->setFont(idxfont);
        idxitem->setBackground(QBrush(QColor(255,255,255)));
        idxitem->setForeground(QBrush(QColor(0,0,0)));
        idxitem->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
        ui->tableWidget->setItem(i,0,idxitem);
        //帧长
        QTableWidgetItem* lenitem = new QTableWidgetItem();
        QFont lenfont;
        lenfont.setPointSize(11);
        lenfont.setFamily("Microsoft YaHei");
        int len = vec_data.at(m_currentPage*m_PageSize).datalen;
        QString lentext;
        if(len == 0)
        {
            lentext = "";
        }
        else
        {
            lentext = QString("%1").arg(vec_data.at(m_currentPage*m_PageSize).datalen);
        }
        lenitem->setText(lentext);
        lenitem->setFont(lenfont);
        lenitem->setBackground(QBrush(QColor(255,255,255)));
        lenitem->setForeground(QBrush(QColor(0,0,0)));
        lenitem->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
        ui->tableWidget->setItem(i,1,lenitem);
        //值
        QTableWidgetItem* dataitem = new QTableWidgetItem();
        QFont datafont;
        datafont.setPointSize(11);
        datafont.setFamily("Microsoft YaHei");
        QString dateHex = vec_data.at(m_currentPage*m_PageSize).data.mdata.toHex(' ').toUpper();
        dataitem->setText(dateHex);
        dataitem->setFont(datafont);
        dataitem->setBackground(QBrush(QColor(255,255,255)));
        dataitem->setForeground(QBrush(QColor(0,0,0)));
        dataitem->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
        ui->tableWidget->setItem(i,2,dataitem);
        //操作
        wid_action* mwid_act = new wid_action(i,"确认删除帧吗？");
        connect(mwid_act,&wid_action::sig_delICD,this,&wid_FCData::slot_delICD);
        connect(mwid_act,&wid_action::sig_editICD,this,&wid_FCData::slot_editICD);
        ui->tableWidget->setCellWidget(i,3,mwid_act);
    }
    ui->tableWidget->setUpdatesEnabled(true);

    if(m_totalPage == 0)
    {
        ui->sBox_currentPage->setValue(0);
        ui->label_pageall->setText(QString("%1").arg(0));
    }
    else {
        ui->sBox_currentPage->setValue(m_currentPage + 1);
        ui->label_pageall->setText(QString("%1").arg(m_totalPage));
    }
}

void wid_FCData::updateTable()
{
    int dataCount = vec_data.count();
    int currRow = dataCount -  m_currentPage*m_PageSize;
    ui->tableWidget->setRowCount(0);//将当前表格清空
    int rowidx;
    rowidx = currRow< m_PageSize ? currRow : m_PageSize;
    ui->tableWidget->setRowCount(rowidx);
    ui->tableWidget->setUpdatesEnabled(false);
    for(int i = 0;i < rowidx;i++)
    {
        //帧序号
        QTableWidgetItem* idxitem = new QTableWidgetItem();
        QFont idxfont;
        idxfont.setPointSize(11);
        idxfont.setFamily("Microsoft YaHei");
        QString idxtext = QString("%1").arg(m_currentPage*m_PageSize);
        idxitem->setText(idxtext);
        idxitem->setFont(idxfont);
        idxitem->setBackground(QBrush(QColor(255,255,255)));
        idxitem->setForeground(QBrush(QColor(0,0,0)));
        idxitem->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
        ui->tableWidget->setItem(i,0,idxitem);
        //帧长
        QTableWidgetItem* lenitem = new QTableWidgetItem();
        QFont lenfont;
        lenfont.setPointSize(11);
        lenfont.setFamily("Microsoft YaHei");
        int len = vec_data.at(m_currentPage*m_PageSize).datalen;
        QString lentext;
        if(len == 0)
        {
            lentext = "";
        }
        else
        {
            lentext = QString("%1").arg(vec_data.at(m_currentPage*m_PageSize).datalen);
        }
        lenitem->setText(lentext);
        lenitem->setFont(lenfont);
        lenitem->setBackground(QBrush(QColor(255,255,255)));
        lenitem->setForeground(QBrush(QColor(0,0,0)));
        lenitem->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
        ui->tableWidget->setItem(i,1,lenitem);
        //值
        QTableWidgetItem* dataitem = new QTableWidgetItem();
        QFont datafont;
        datafont.setPointSize(11);
        datafont.setFamily("Microsoft YaHei");
        QString dateHex = vec_data.at(m_currentPage*m_PageSize).data.mdata.toHex(' ').toUpper();
        dataitem->setText(dateHex);
        dataitem->setFont(datafont);
        dataitem->setBackground(QBrush(QColor(255,255,255)));
        dataitem->setForeground(QBrush(QColor(0,0,0)));
        dataitem->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
        ui->tableWidget->setItem(i,2,dataitem);
        //操作
        wid_action* mwid_act = new wid_action(i,"确认删除帧吗？");
        connect(mwid_act,&wid_action::sig_delICD,this,&wid_FCData::slot_delICD);
        connect(mwid_act,&wid_action::sig_editICD,this,&wid_FCData::slot_editICD);
        ui->tableWidget->setCellWidget(i,3,mwid_act);
    }
    ui->tableWidget->setUpdatesEnabled(true);


}

void wid_FCData::slot_delICD(int row)
{
    //删除指定行
    int currallrow = ui->spinBox_msgCount->value();
    currallrow--;
    ui->spinBox_msgCount->setValue(currallrow);
    //删除的当前帧序号
    int delRow = row + m_currentPage*m_PageSize;
    vec_data.removeAt(delRow);
    int nowall = vec_data.count();
    //设置当前总页数
    int allpage = (nowall  + m_PageSize - 1)/ m_PageSize;
    ui->label_pageall->setText(QString("%1").arg(allpage));
    //设置当前页
    if(m_totalPage == 0)
    {
        ui->sBox_currentPage->setValue(0);
        ui->label_pageall->setText(QString("%1").arg(0));
    }
    else {
        ui->sBox_currentPage->setValue(m_currentPage + 1);
        ui->label_pageall->setText(QString("%1").arg(m_totalPage));
    }
    updateTAB();
}

void wid_FCData::slot_editICD(int row)
{
    //编辑指定行
    //打开编辑指定的页面
    dlog_FCdatainfo* mlog_fcinfo = new dlog_FCdatainfo();
    mlog_fcinfo->exec();
}

void wid_FCData::on_pushButton_creat_clicked()
{
    //创建数据
    int row = ui->spinBox_msgCount->value();
    vec_data.clear();
    for(int i = 0;i < row;i++)
    {
        CREATFCDATA data;
        vec_data.append(data);
    }
    //总页数
    m_totalPage = std::max(1,(vec_data.size() + m_PageSize - 1)/ m_PageSize);
    m_currentPage = 0;
    updateTAB();
}

void wid_FCData::on_pushButton_up_clicked()
{
    //上一页
    if(m_currentPage > 0)
    {
        --m_currentPage;//更新表格
        updateTAB();
    }
}

void wid_FCData::on_pushButton_down_clicked()
{
    //下一页
    if(m_currentPage < m_totalPage -1)
    {
        ++m_currentPage;
        updateTAB();
    }
}

void wid_FCData::on_checkBox_times_stateChanged(int arg1)
{
    if(arg1)
    {
        ui->spinBox_times->setEnabled(true);
    }
    else {
        ui->spinBox_times->setEnabled(false);
    }
}

void wid_FCData::on_pbtn_tunTo_clicked()
{
    //跳转
    const int page = ui->sBox_currentPage->value() - 1;
    if(page < 0 || page >= m_totalPage)
    {
        ui->sBox_currentPage->setValue(m_currentPage + 1);
        return;
    }
    if(page == m_currentPage)
    {
        return;
    }
    m_currentPage = page;
    updateTAB();
}
