#include "wid_reviewset.h"
#include "ui_wid_reviewset.h"

wid_reviewset::wid_reviewset(qint64 ipriID,QWidget *parent) :
    QWidget(parent),
    ui(new Ui::wid_reviewset)
{
    ui->setupUi(this);
    mipriID = ipriID;
    Init();
}

wid_reviewset::~wid_reviewset()
{
    delete ui;
}

void wid_reviewset::Init()
{
    ui->radioButton_2->setChecked(true);
    ui->stackedWidget->setCurrentIndex(0);
    ui->tableWidget_data->verticalHeader()->setVisible(false);
    ui->tableWidget_data->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    ui->tableWidget_data->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->tableWidget_info->setColumnCount(16);
    ui->tableWidget_info->setEditTriggers(QAbstractItemView::NoEditTriggers);
    ui->tableWidget_info->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    connect(ui->tableWidget_info,&QTableWidget::itemChanged,this,&wid_reviewset::onItemchange);
    connect(ui->tableWidget_data,&QTableWidget::cellClicked,this,&wid_reviewset::slot_itemClick);
    readIni();

    ui->tableWidget_data->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    ui->tableWidget_data->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->tableWidget_data->setEditTriggers(QAbstractItemView::NoEditTriggers);
    ui->tableWidget_data->verticalHeader()->setVisible(false);
    ui->tableWidget_data->horizontalHeader()->setStyleSheet(R"(
                                                            QHeaderView::section{
                                                            background-color:rgb(255,255,255);
                                                            color:rgb(0,0,0);
                                                            font: 75 10pt "微软雅黑";
                                                            border-top:1px solid rgb(70,90,120);
                                                            border-bottom:1px solid rgb(70,90,120);
                                                            border-radius:1px;
                                                            }
                                                            )");
    //000
    readData();
    ui->tableWidget_info->setColumnCount(16);
}

void wid_reviewset::readIni()
{
    QString iniPath = QApplication::applicationDirPath() + "/prj/WTBTS/DataFile/DataSaveInfo.ini";
    QSettings settings(iniPath,QSettings::IniFormat);

    //读取save文件夹存储路径
    QString savePath = settings.value("saveConfig/savePath").toString();
    mtype = settings.value("saveConfig/reviewType").toInt();

    //显示到界面并填充data表
    ui->lineEdit->setText(savePath);
    //获取savepath路径下的所有文件并显示

}

void wid_reviewset::on_radioButton_2_clicked()
{
    ui->radioButton_2->setChecked(true);
    ui->radioButton_3->setChecked(false);
    ui->radioButton_4->setChecked(false);
    mtype = 0;
}

void wid_reviewset::on_radioButton_3_clicked()
{
    ui->radioButton_2->setChecked(false);
    ui->radioButton_3->setChecked(true);
    ui->radioButton_4->setChecked(false);
    mtype = 1;
}

void wid_reviewset::on_radioButton_4_clicked()
{
    ui->radioButton_2->setChecked(false);
    ui->radioButton_3->setChecked(false);
    ui->radioButton_4->setChecked(true);
    mtype = 2;
}

void wid_reviewset::slot_itemClick(int row, int colum)
{
    //显示回放数据值


    //暂时测试阶段用固定值显示
    //记录当前行
    mclickRow = row;
    MYDBSTRUCT mb = mDataList.at(0);
    m_data = mb.data;
#if 1
    //总页数
    m_totalPage = std::max(1,(m_data.size() + KItemsPerPage - 1)/ KItemsPerPage);
    m_currentPage = 0;
    updateDatainfo();
#elif
        setDataInfo(data);
#endif

}

void wid_reviewset::onItemchange(QTableWidgetItem *item)
{
    if(!item)
    {
        return;
    }
    QString text = item->text();
    if(text == "")
    {
        return;
    }

    bool ok = false;
    int value = 0;
    int mode = ui->comboBox_type->currentIndex();
    //判断当前模式
    if(mode == 0)
    {
        //当前为16进制
        value = text.toInt(&ok,16);
    }
    else
    {
        //当前为10进制
        value = text.toInt(&ok,10);
    }
    if(ok && value >=0 && value <= 255)
    {
        item->setData(Qt::UserRole,value);
    }
    else
    {
        int oldValue = item->data(Qt::UserRole).toInt();
        if(mode == 0)
        {
            item->setText(QString("%1").arg(oldValue,2,16,QChar('0')).toUpper());
        }
        else
        {
            item->setText(QString("%1").arg(oldValue));
        }
    }
}

void wid_reviewset::updateDatainfo()
{
    ui->tableWidget_info->blockSignals(true);
    ui->tableWidget_info->clearContents();
    ui->tableWidget_info->setRowCount(0);

    const int base = m_currentPage * KItemsPerPage;
    const int total = m_data.size();
    int realCount = std::min(KItemsPerPage,total - base);
    int rows = ceil(realCount / kCols);
    ui->tableWidget_info->setRowCount(rows);
    for(int r = 0;r < kRowsPerPage;++r)
    {
        for(int c = 0;c < kCols;++c)
        {
            const int idx = base + r * kCols + c;
            //获取item
            QTableWidgetItem* item = new QTableWidgetItem();
            item->setTextAlignment(Qt::AlignCenter);
            ui->tableWidget_info->setItem(r,c,item);
            if(idx < total)
            {
                const quint8 byteVal = static_cast<quint8>(m_data.at(idx));
                item->setData(Qt::UserRole,byteVal);
                int val = item->data(Qt::UserRole).toInt();
                //显示
                QString displayText;
                if(ui->comboBox_type->currentIndex() == 0)
                {
                    displayText = QString("%1").arg(val,2,16,QChar('0')).toUpper();
                }
                else
                {
                    displayText = QString::number(val);
                }
                item->setText(displayText);
            }
        }
    }
    ui->tableWidget_info->blockSignals(false);
    ui->sbox_currentPage->setValue(m_currentPage + 1);
    ui->label_pageall->setText(QString("%1").arg(m_totalPage));

    ui->pbtn_up->setEnabled(m_currentPage > 0);
    ui->pbtn_down->setEnabled(m_currentPage < m_totalPage - 1);

}

void wid_reviewset::setDataInfo(const QByteArray &data)
{
    ui->tableWidget_info->blockSignals(true);
    ui->tableWidget_info->clearContents();
    ui->tableWidget_info->setRowCount(0);
    int cols = ui->tableWidget_info->columnCount();
    int sizes = data.size();
    int rows = data.size() / cols;
    ui->tableWidget_info->setRowCount(rows);
    //遍历Qbytearay的每一个字节
    for(int i = 0;i < sizes;++i)
    {
        int row = i / cols;
        int col = i % cols;
        quint8 byteVal = static_cast<quint8>(data.at(i));
        //获取item
        QTableWidgetItem* item = new QTableWidgetItem();
        item->setTextAlignment(Qt::AlignCenter);
        ui->tableWidget_info->setItem(row,col,item);
        item->setData(Qt::UserRole,byteVal);
        int val = item->data(Qt::UserRole).toInt();
        //显示
        QString displayText;
        if(ui->comboBox_type->currentIndex() == 0)
        {
            displayText = QString("%1").arg(val,2,16,QChar('0')).toUpper();
        }
        else
        {
            displayText = QString::number(val);
        }
        item->setText(displayText);
    }
    ui->tableWidget_info->blockSignals(false);
}

void wid_reviewset::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    static bool inited = false;
    if(!inited)
    {
        inited = true;
        ui->tableWidget_data->scrollToBottom();
        ui->tableWidget_data->selectRow(999);
        ui->tableWidget_info->scrollToBottom();
        ui->tableWidget_info->selectRow(999);
    }
}

void wid_reviewset::on_pushButton_clicked()
{
    //保存配置
    Dlog_info* mlog = new Dlog_info(2,"配置保存成功！");
    mlog->exec();
    QStringList chList = PM()->GetCHLs(mipriID,"FC");
    for(int chindex = 0;chindex < chList.count();chindex++)
    {
        WTContrl *pCtrl = (WTContrl *)(PM()->GetWTCtrl(chList.at(chindex)));
        pCtrl->m_iCfgID = mtype;
    }
}

void wid_reviewset::on_pushButton_3_clicked()
{
    ui->stackedWidget->setCurrentIndex(1);
}

void wid_reviewset::on_pushButton_2_clicked()
{
    ui->stackedWidget->setCurrentIndex(0);
}

void wid_reviewset::on_toolButton_choosePath_clicked()
{
    //确定
    QString dirpath = QFileDialog::getOpenFileName(
                this,
                "选择文件",
                QDir::homePath(),
                "bin 文件(*.bin)");
    if(dirpath != "")
    {
        //导入数据源数据
        ui->lineEdit->setText(dirpath);
        QString pname = QFileInfo(dirpath).fileName();
        QString name = pname;
        if(name.endsWith(".bin",Qt::CaseSensitive))
        {
            name.chop(4);
        }
        //        int row = ui->tableWidget_data->rowCount();
        QFile file(dirpath);
        if(!file.open(QIODevice::ReadOnly))
        {
            return;
        }
        QByteArray allData = file.readAll();
        file.close();

        if(file.size() < 8)
        {
            return;
        }

        quint32 payloadLength;
        quint32 payloadType;

        //大端
        payloadLength = (static_cast<quint8>(allData[0]) << 24 |
                                                            static_cast<quint8>(allData[1]) << 16 |
                                                                                               static_cast<quint8>(allData[2]) << 8 |
                                                                                                                                  static_cast<quint8>(allData[3]));

        payloadType = (static_cast<quint8>(allData[4]) << 24 |
                                                          static_cast<quint8>(allData[5]) << 16 |
                                                                                             static_cast<quint8>(allData[6]) << 8 |
                                                                                                                                static_cast<quint8>(allData[7]));

        QByteArray payload = allData.mid(8);
        if(payload.size() != static_cast<int>(payloadLength))
        {
            return;
        }
//        ui->tableWidget_data->blockSignals(true);
        int group = static_cast<int>(payloadLength)/2096;
        ui->tableWidget_data->setRowCount(group);



//        int cols = ui->tableWidget_info->columnCount();
//        int sizes = payload.size();
//        int rows = payload.size() / cols;
//        ui->tableWidget_info->setRowCount(rows);

        for(int i = 0;i < group;i++)
        {
            QTableWidgetItem* item = new QTableWidgetItem();
            item->setTextAlignment(Qt::AlignCenter);
            item->setText(QString("%1").arg(i+1));
            ui->tableWidget_data->setItem(i,0,item);
            QTableWidgetItem* item1 = new QTableWidgetItem();
            item1->setTextAlignment(Qt::AlignCenter);
            QString dateHex = payload.toHex(' ').toUpper();
            item1->setText(dateHex);
            ui->tableWidget_data->setItem(i,1,item1);
            MYDBSTRUCT mb;
            mb.data = payload;
            mDataList.clear();
            mDataList.append(mb);
#if 0
            int row = i / cols;
            int col = i % cols;
            quint8 byteVal = static_cast<quint8>(payload.at(i));
            //获取item
            QTableWidgetItem* item = new QTableWidgetItem();
            item->setTextAlignment(Qt::AlignCenter);
            ui->tableWidget_info->setItem(row,col,item);
            item->setData(Qt::UserRole,byteVal);
            int val = item->data(Qt::UserRole).toInt();
            //显示
            QString displayText;
            if(ui->comboBox_type->currentIndex() == 0)
            {
                displayText = QString("%1").arg(val,2,16,QChar('0')).toUpper();
            }
            else
            {
                displayText = QString::number(val);
            }
            item->setText(displayText);
        }
        ui->tableWidget_info->blockSignals(false);
#endif
        }
    }
}

//切换进制
void wid_reviewset::on_comboBox_type_currentIndexChanged(int index)
{
    ui->tableWidget_info->setUpdatesEnabled(false);
    int row = ui->tableWidget_info->rowCount();
    int colunm = ui->tableWidget_info->columnCount();
    for(int i = 0;i < row;i++)
    {
        for(int j = 0;j < colunm;j++)
        {
            QTableWidgetItem* item = ui->tableWidget_info->item(i,j);
            if(!item)
            {
                continue;
            }
            int value = item->data(Qt::UserRole).toInt();
            if(index == 1)
            {
                //切换为10进制
                item->setText(QString::number(value));
            }
            else
            {
                //16进制
                item->setText(QString("%1").arg(value,2,16,QChar('0')).toUpper());
            }
        }
    }

    ui->tableWidget_info->setUpdatesEnabled(true);
}

//临时
void wid_reviewset::readData()
{
    //获取数据库中数据并存放在缓存中
    mDataList.clear();
    mDataList = getALLData();
    int allnum = mDataList.count();
    //    ui->lab_all->setText(QString("%1").arg(allnum));
    //发送数据存放到ctrl中
    QStringList chList = PM()->GetCHLs(mipriID,"FC");
    for(int chindex = 0;chindex < chList.count();chindex++)
    {
        WTContrl *pCtrl = (WTContrl *)(PM()->GetWTCtrl(chList.at(chindex)));
        CMessageBlock * cmblok = pCtrl->cMessageBlock;
//        pCtrl->cMessageBlock->mapsTTabel.clear();//清空ctrl中的发送数据源
        for(int i = 0; i < mDataList.count();i++)
        {
            MsgBlock mblock;
            QString dataName = mDataList.at(i).name;
            mblock.sMsgID = mDataList.at(i).name;
            mblock.iTxLen = mDataList.at(i).datalen;
            mblock.sData = mDataList.at(i).data;
            mblock.varparam = QVariant::fromValue(mDataList.at(i).WTHead);
            mblock.varparamini = QVariant::fromValue(mDataList.at(i).FCHead);
            mblock.iTotal = mDataList.at(i).start;
            mblock.ITxCnt = mDataList.at(i).groups;
            mblock.period = mDataList.at(i).step;
            mblock.mCheck = mDataList.at(i).date;
            mblock.busCHName = mDataList.at(i).type;
            pCtrl->cMessageBlock->mapsTTabel.insert(dataName,mblock);
        }
    }
}

QList<MYDBSTRUCT> wid_reviewset::getALLData()
{

    QString dbPath = QApplication::applicationDirPath() + "/prj/WTBTS/DataFile/WT_FCData.db";
    mdb = QSqlDatabase::addDatabase("QSQLITE");
    mdb.setDatabaseName(dbPath);
    if(!mdb.open())
    {
        Dlog_info* mlog = new Dlog_info(2,"数据源数据库打开失败！");
        mlog->exec();
        //        return;
    }
    //一个表中存储多个数据
    QSqlQuery query;
    //判断是否存在
    QString sqlstr = "SELECT name FROM sqlite_master WHERE type = 'table' AND name = '" + tname + "';";
    query.exec(sqlstr);
    if(!query.next())
    {
        //不存在则创建表
        sqlstr = "CREATE TABLE " + tname + " (name TEXT PRIMARY KEY,date TEXT NOT NULL,startTime TEXT NOT NULL,step TEXT NOT NULL,Datalen TEXT NOT NULL,groups TEXT NOT NULL,WTHead BLOB NOT NULL,FCHead BLOB NOT NULL,data BLOB NOT NULL,type TEXT NOT NULL)";
        bool ok = query.exec(sqlstr);
        if(!ok)
        {
            //建表失败
            Dlog_info* mlog = new Dlog_info(2,"数据表创建失败！");
            mlog->exec();
            //            return;
        }
    }
    QList<MYDBSTRUCT> re;
    sqlstr = "SELECT date,name,data,Datalen,WTHead,FCHead,startTime,step,groups,type FROM "+tname+" ORDER BY date DESC";
    if(!query.exec(sqlstr))
    {
        //查询数据表失败
        Dlog_info* mflog = new Dlog_info(2,"数据表为空!");
        mflog->exec();
        return re;
    }
    while (query.next())
    {
        MYDBSTRUCT d;
        d.date = query.value(0).toString();
        d.name = query.value(1).toString();
        QByteArray ba = query.value(2).toByteArray();
        d.data = ba;
        d.datalen = query.value(3).toInt();
        QByteArray WTba = query.value(4).toByteArray();
        d.WTHead = WTba;
        QByteArray FCba = query.value(5).toByteArray();
        d.FCHead = FCba;
        d.start = query.value(6).toInt();
        d.step = query.value(7).toInt();
        d.groups = query.value(8).toInt();
        d.type = query.value(9).toString();
        re.append(d);
    }
    return re;
}

void wid_reviewset::on_pbtn_turnTo_clicked()
{
    //跳转
    const int page = ui->sbox_currentPage->value() - 1;
    if(page < 0 || page >= m_totalPage)
    {
        ui->sbox_currentPage->setValue(m_currentPage + 1);
        return;
    }
    if(page == m_currentPage)
    {
        return;
    }
    m_currentPage = page;
    updateDatainfo();
}

void wid_reviewset::on_pbtn_FirstPage_clicked()
{
    //首页
    if(m_currentPage != 0)
    {
        m_currentPage = 0;
        updateDatainfo();
    }
}

void wid_reviewset::on_pbtn_up_clicked()
{
    //上一页
    if(m_currentPage > 0)
    {
        --m_currentPage;
        updateDatainfo();
    }
}

void wid_reviewset::on_pbtn_down_clicked()
{
    //下一页
    if(m_currentPage < m_totalPage -1)
    {
        ++m_currentPage;
        updateDatainfo();
    }
}

void wid_reviewset::on_pbtn_end_clicked()
{
    //尾页
    if(m_currentPage != m_totalPage - 1)
    {
        m_currentPage = m_totalPage -1;
        updateDatainfo();
    }
}
