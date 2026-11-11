#include "wid_datacontrol.h"
#include "ui_wid_datacontrol.h"

wid_datacontrol:: wid_datacontrol(qint64 ipriID,QWidget *parent) :
    QWidget(parent),
    ui(new Ui::wid_datacontrol)
{
    //数据源管理
    ui->setupUi(this);
    mipriID = ipriID;
    Init();

#if 0
    once();
#endif
}

wid_datacontrol::~wid_datacontrol()
{
    if(mdb.isOpen())
    {
        mdb.close();
    }
    delete ui;
}

void wid_datacontrol::once()
{
    int length = 2096;
    QByteArray rawData;
    rawData.resize(length + 8);

    QByteArray rawData1;
    rawData1.resize(length);

    for(int i = 0;i < length;i++)
    {
        rawData1[i] = static_cast<char>(QRandomGenerator::global()->generate() & 0xFF);
    }
    QByteArray header;
    header.resize(8);

    //大端
    quint32 lengthv = static_cast<quint32>(2096);
    header[0] = static_cast<char>((lengthv >> 24) & 0xFF); //高位
    header[1] = static_cast<char>((lengthv >> 16) & 0xFF);
    header[2] = static_cast<char>((lengthv >> 8) & 0xFF); //低位
    header[3] = static_cast<char>((lengthv) & 0xFF);
    //小端
    quint32 typev = static_cast<quint32>(1);//FC
    header[4] = static_cast<char>((typev >> 24) & 0xFF); //高位
    header[5] = static_cast<char>((typev >> 16) & 0xFF);
    header[6] = static_cast<char>((typev >> 8) & 0xFF); //低位
    header[7] = static_cast<char>((typev) & 0xFF);

    //拼接
    rawData.clear();
    rawData.append(header);
    rawData.append(rawData1);
    //save

    if(rawData.isEmpty())
    {
        return;
    }
    QString savepath = QApplication::applicationDirPath() + "/prj/WTBTS/WT_data.bin";
    QFile f(savepath);
    if(!f.open(QIODevice::WriteOnly | QIODevice::Truncate))
    {
        return;
    }
    qint64 writen = f.write(rawData);
    f.close();

}
void wid_datacontrol::Init()
{
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
    ui->tableWidget_info->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    ui->tableWidget_info->setEditTriggers(QAbstractItemView::NoEditTriggers);
    connect(ui->tableWidget_data,&QTableWidget::cellClicked,this,&wid_datacontrol::slot_itemClick);
    connect(ui->tableWidget_info,&QTableWidget::itemChanged,this,&wid_datacontrol::onItemchange);
    mlog_cd = new dlog_datacreat();
    ui->gridLayout_13->addWidget(mlog_cd);
    connect(mlog_cd,&dlog_datacreat::sig_saveFCData,this,&wid_datacontrol::slot_saveFCData);
    connect(mlog_cd,&dlog_datacreat::sig_back,this,&wid_datacontrol::slot_back);
    ui->tableWidget_info->setColumnCount(16);
    InitSql();
}

void wid_datacontrol::on_pushButton_2_clicked()
{
    //切换到自定义窗口
    ui->stackedWidget->setCurrentIndex(1);
}

void wid_datacontrol::InitSql()
{
    QString dbPath = QApplication::applicationDirPath() + "/prj/WTBTS/DataFile/WT_FCData.db";
    mdb = QSqlDatabase::addDatabase("QSQLITE");
    mdb.setDatabaseName(dbPath);
    if(!mdb.open())
    {
        Dlog_info* mlog = new Dlog_info(2,"数据源数据库打开失败！");
        mlog->exec();
        return;
    }
    //一个表中存储多个数据
    QSqlQuery query;
    //判断是否存在
    QString sqlstr = "SELECT name FROM sqlite_master WHERE type = 'table' AND name = '" + tname + "';";
    query.exec(sqlstr);
    if(!query.next())
    {
        //不存在则创建表
        sqlstr = "CREATE TABLE " + tname + " (name TEXT PRIMARY KEY,date TEXT NOT NULL,startTime TEXT NOT NULL,step TEXT NOT NULL,Datalen TEXT NOT NULL,bbc TEXT NOT NULL,groups TEXT NOT NULL,WTHead BLOB NOT NULL,FCHead BLOB NOT NULL,data BLOB NOT NULL,type TEXT NOT NULL)";
        bool ok = query.exec(sqlstr);
        if(!ok)
        {
            //建表失败
            Dlog_info* mlog = new Dlog_info(2,"数据表创建失败！");
            mlog->exec();
            return;
        }
    }
}

void wid_datacontrol::slot_itemClick(int row,int column)
{
    //记录当前行
    mclickRow = row;
    //获取该行的数据
    MYDBSTRUCT mb = mDataList.at(row);
    sname = mb.name;
    m_data.resize(mb.data.size());
    m_data = mb.data;
    //总页数
    m_totalPage = std::max(1,(m_data.size() + KItemsPerPage - 1)/ KItemsPerPage);
    m_currentPage = 0;
    setDataInfo();
}

void wid_datacontrol::setDataInfo()
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
    ui->label_currentpage->setValue(m_currentPage + 1);
    ui->label_totalpage->setText(QString("%1").arg(m_totalPage));

    ui->pbtn_infoUp->setEnabled(m_currentPage > 0);
    ui->pbtn_infoDown->setEnabled(m_currentPage < m_totalPage - 1);
#if 0
    int cols = ui->tableWidget_info->columnCount();
    int sizes = m_data.size();
    int rows = m_data.size() / cols;
    ui->tableWidget_info->setRowCount(rows);
    //遍历Qbytearay的每一个字节
    for(int i = 0;i < sizes;++i)
    {
        int row = i / cols;
        int col = i % cols;
        quint8 byteVal = static_cast<quint8>(m_data.at(i));
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

//修改数据
void wid_datacontrol::onItemchange(QTableWidgetItem *item)
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

QByteArray wid_datacontrol::getData()
{
    QByteArray result;
    int rows = ui->tableWidget_info->rowCount();
    int cols = ui->tableWidget_info->columnCount();
    for(int row = 0;row <rows;row++)
    {
        for(int col = 0;col < cols;col++)
        {
            QTableWidgetItem* item = ui->tableWidget_info->item(row,col);
            if(!item)
            {
                result.append('\0');
                continue;
            }
            quint8 byteVal = static_cast<quint8>(item->data(Qt::UserRole).toUInt() & 0XFF);
            result.append(static_cast<char>(byteVal));
        }
    }

    return result;
}

//切换进制
void wid_datacontrol::on_comboBox_type_currentIndexChanged(int index)
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

void wid_datacontrol::readData()
{
    //获取数据库中数据并存放在缓存中
    mDataList.clear();
    mDataList = getALLData();
    int allnum = mDataList.count();
    ui->lab_all->setText(QString("%1").arg(allnum));
    //发送数据存放到ctrl中
    QStringList chList = PM()->GetCHLs(mipriID,"FC");
    for(int chindex = 0;chindex < chList.count();chindex++)
    {
        WTContrl *pCtrl = (WTContrl *)(PM()->GetWTCtrl(chList.at(chindex)));
        CMessageBlock * cmblok = pCtrl->cMessageBlock;
        pCtrl->cMessageBlock->mapsTTabel.clear();//清空ctrl中的发送数据源
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
            mblock.uprioirity = mDataList.at(i).bbc;
            pCtrl->cMessageBlock->mapsTTabel.insert(dataName,mblock);
        }
    }
    //数据放在Aurora对象中
    QStringList AchList = PM()->GetCHLs(mipriID,"Aurora");
    for(int Achindex = 0;Achindex < AchList.count();Achindex++)
    {
        WTContrl *pCtrl = (WTContrl *)(PM()->GetWTCtrl(AchList.at(Achindex)));
        CMessageBlock * cmblok = pCtrl->cMessageBlock;
        pCtrl->cMessageBlock->mapsTTabel.clear();//清空ctrl中的发送数据源
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
            mblock.uprioirity = mDataList.at(i).bbc;
            pCtrl->cMessageBlock->mapsTTabel.insert(dataName,mblock);
        }
    }
    //数据放在以太网对象中
    QStringList nchList = PM()->GetCHLs(mipriID,"NET");
    for(int nchindex = 0;nchindex < nchList.count();nchindex++)
    {
        WTContrl *pCtrl = (WTContrl *)(PM()->GetWTCtrl(nchList.at(nchindex)));
        pCtrl->cMessageBlock->mapsTTabel.clear();//清空ctrl中的发送数据源
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
            mblock.uprioirity = mDataList.at(i).bbc;
            pCtrl->cMessageBlock->mapsTTabel.insert(dataName,mblock);
        }
    }
}

void wid_datacontrol::slot_saveFCData(QString name)
{
    //获取缓存中的FC数据
    WTAPI_TE_DSIM_SEND_HEAD* WTHead = mlog_cd->getWTDSIMHeader();
    WTAPI_TE_DSIM_FCHEAD* FCHead = mlog_cd->getWTFCHeader();
    QByteArray data = mlog_cd->getData();
    int start = mlog_cd->getFCStart();
    int step = mlog_cd->getFCStep();
    int groups = mlog_cd->getFCGroups();
    int bbc = mlog_cd->getbbc();
    //启动数据库
    QSqlQuery query;
    //插入数据到数据表
    QString date = QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss");
    QString type = "FC";
    int len = data.size();
    QString strlen = QString("%1").arg(len);
    QString strstart = QString("%1").arg(start);
    QString strstep = QString("%1").arg(step);
    QString strgroups = QString("%1").arg(groups);
    QByteArray WTba;
    QByteArray FCba;
    WTba.append(reinterpret_cast<const char*>(WTHead),sizeof(WTAPI_TE_DSIM_SEND_HEAD));
    FCba.append(reinterpret_cast<const char*>(FCHead),sizeof(WTAPI_TE_DSIM_FCHEAD));
    query.prepare("INSERT INTO " + tname + " (date,name,Datalen,data,startTime,step,groups,WTHead,FCHead,type,bbc) VALUES (:date,:name,:Datalen,:data,:startTime,:step,:groups,:WTHead,:FCHead,:type,:bbc)");
    query.bindValue(":date",date);
    query.bindValue(":name",name);
    query.bindValue(":Datalen",strlen);
    query.bindValue(":startTime",strstart);
    query.bindValue(":step",strstep);
    query.bindValue(":groups",strgroups);
    query.bindValue(":data",data);
    query.bindValue(":WTHead",WTba);
    query.bindValue(":FCHead",FCba);
    query.bindValue(":type",type);
    query.bindValue(":bbc",bbc);

    //保存到文件中 待完成
    #if 1



    #endif

    if(!query.exec())
    {
        //数据保存失败
        Dlog_info* mflog = new Dlog_info(2,"数据保存失败！");
        mflog->exec();
        return;
    }
    //成功
    Dlog_info* mflog = new Dlog_info(2,"数据保存成功！");
    mflog->exec();
    int num = ui->lab_all->text().toInt();
    num++;
    ui->lab_all->setText(QString("%1").arg(num));
    updateTable();
}

QList<MYDBSTRUCT> wid_datacontrol::getALLData()
{
    QSqlQuery query;
    QString sqlstr;
    QList<MYDBSTRUCT> re;
    sqlstr = "SELECT date,name,data,Datalen,WTHead,FCHead,startTime,step,groups,type,bbc FROM "+tname+" ORDER BY date DESC";
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
        d.bbc = query.value(10).toInt();
        re.append(d);
    }
    return re;
}

void wid_datacontrol::updateTable()
{
    readData();
    //修改数据源列表
    int dataCount = mDataList.count();
    //清空表格
    ui->tableWidget_data->blockSignals(true);
    ui->tableWidget_data->clearContents();
    ui->tableWidget_data->blockSignals(false);
    QList<QDateTime> list_Time;
    QList<QString> list_DataNameStr;
#if 1
    int start = dm_currentPage * dm_PageSize;
    int count = (m_totalPage == 0)? 0 : qMin(dm_PageSize,dataCount - start);
    ui->tableWidget_data->setRowCount(count);
    for(int i = 0;i < count;++i)
    {
        MYDBSTRUCT mb = mDataList.at(i);
        //数据源名
        QTableWidgetItem* nameItem = new QTableWidgetItem();
        nameItem->setText(mb.name);
        nameItem->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
        ui->tableWidget_data->setItem(i,0,nameItem);
        list_DataNameStr.append(mb.name);
        //总线类型
        QTableWidgetItem* typeItem = new QTableWidgetItem();
        typeItem->setText(mb.type);
        typeItem->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
        ui->tableWidget_data->setItem(i,1,typeItem);
        //时间
        QDateTime time = QDateTime::fromString(mb.date,"yyyy-MM-dd HH:mm:ss");
        QTableWidgetItem* timeItem = new QTableWidgetItem();
        timeItem->setText(mb.date);
        timeItem->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
        ui->tableWidget_data->setItem(i,2,timeItem);
        list_Time.append(time);
        //数据长度
        QTableWidgetItem* lenItem = new QTableWidgetItem();
        lenItem->setText(QString("%1").arg(mb.datalen));
        lenItem->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
        ui->tableWidget_data->setItem(i,3,lenItem);
        //值
        QTableWidgetItem* dataItem = new QTableWidgetItem();
        QString dateHex = mb.data.toHex(' ').toUpper();
        dataItem->setText(dateHex);
        dataItem->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
        ui->tableWidget_data->setItem(i,4,dataItem);
    }
#else
    ui->tableWidget_data->setRowCount(dataCount);
    for(int i = 0;i < mDataList.count();i++)
    {
        MYDBSTRUCT mb = mDataList.at(i);
        //数据源名
        QTableWidgetItem* nameItem = new QTableWidgetItem();
        nameItem->setText(mb.name);
        nameItem->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
        ui->tableWidget_data->setItem(i,0,nameItem);
        list_DataNameStr.append(mb.name);
        //总线类型
        QTableWidgetItem* typeItem = new QTableWidgetItem();
        typeItem->setText(mb.type);
        typeItem->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
        ui->tableWidget_data->setItem(i,1,typeItem);
        //时间
        QDateTime time = QDateTime::fromString(mb.date,"yyyy-MM-dd HH:mm:ss");
        QTableWidgetItem* timeItem = new QTableWidgetItem();
        timeItem->setText(mb.date);
        timeItem->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
        ui->tableWidget_data->setItem(i,2,timeItem);
        list_Time.append(time);
        //数据长度
        QTableWidgetItem* lenItem = new QTableWidgetItem();
        lenItem->setText(QString("%1").arg(mb.datalen));
        lenItem->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
        ui->tableWidget_data->setItem(i,3,lenItem);
        //值
        QTableWidgetItem* dataItem = new QTableWidgetItem();
        QString dateHex = mb.data.toHex(' ').toUpper();
        dataItem->setText(dateHex);
        dataItem->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
        ui->tableWidget_data->setItem(i,4,dataItem);
    }
#endif
    //修改时间范围
    QPair<QDateTime,QDateTime> times = getTimeRange(list_Time);
    QString min = times.first.toString("yyyy-MM-dd HH:mm:ss");
    QString max = times.second.toString("yyyy-MM-dd HH:mm:ss");
    if(list_Time.count() == 1)
    {
        ui->lab_time->setText(min);
    }
    else
    {
        ui->lab_time->setText(min + "-" + max);
    }

    emit sig_scpData(list_DataNameStr);
}

QPair<QDateTime,QDateTime> wid_datacontrol::getTimeRange(QList<QDateTime> dateTimes)
{
    if(dateTimes.isEmpty())
    {
        return qMakePair(QDateTime(),QDateTime());
    }
    auto minIt = std::min_element(dateTimes.begin(),dateTimes.end());
    auto maxIt = std::max_element(dateTimes.begin(),dateTimes.end());

    return qMakePair(*minIt,*maxIt);
}

void wid_datacontrol::on_pushButton_del_clicked()
{
    //删除点击行的数据源
    if(mclickRow == -1)
    {
        return;
    }
    else
    {
        QTableWidgetItem* nameItem = ui->tableWidget_data->item(mclickRow,0);
        QString text = nameItem->text();
        Dlog_info* mflog = new Dlog_info(0,"确认删除:" + text + "?");
        connect(mflog,&Dlog_info::sig_delItem,this,&wid_datacontrol::slot_delItem);
        mflog->exec();
    }
}

void wid_datacontrol::slot_delItem()
{
    //1.获取选中行的数据源名
    QTableWidgetItem* nameItem = ui->tableWidget_data->item(mclickRow,0);
    QString text = nameItem->text();
    QSqlQuery query;
    query.prepare("SELECT COUNT(*) FROM "+ tname +" WHERE name = :name");
    query.bindValue(":name",text);

    if(!query.exec() || !query.next())
    {
        Dlog_info* mflog = new Dlog_info(2,"数据查询失败！");
        mflog->exec();
        return;
    }

    int count = query.value(0).toInt();
    if(count == 0)
    {
        Dlog_info* mflog = new Dlog_info(2,"数据源不存在！");
        mflog->exec();
        return;
    }
    //执行删除
    query.prepare("DELETE FROM " + tname + " WHERE name = :name");
    query.bindValue(":name",text);
    if(!query.exec())
    {
        Dlog_info* mflog = new Dlog_info(2,"删除失败！");
        mflog->exec();
        return;
    }
    Dlog_info* mflog = new Dlog_info(2,"删除成功！");
    mflog->exec();
    ui->tableWidget_info->clearContents();
    ui->tableWidget_info->setRowCount(0);
    //1.刷新data表
    updateTable();
    //2.清空info表
    ui->tableWidget_info->blockSignals(true);
    ui->tableWidget_info->clearContents();
    ui->tableWidget_info->setRowCount(0);
    ui->tableWidget_info->blockSignals(false);
}

void wid_datacontrol::on_pushButton_upload_clicked()
{
    //上传数据源文件---考虑文件夹
    //确定导出的路径
    QString dirpath = QFileDialog::getOpenFileName(
                this,
                "选择文件",
                QDir::homePath(),
                "bin 文件(*.bin)");
    if(dirpath != "")
    {
        //导入数据源数据
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
        int group = static_cast<int>(payloadLength)/2096;
        QByteArray payload = allData.mid(8);
        if(payload.size() != static_cast<int>(payloadLength))
        {
            return;
        }


        //插入到数据库中
        QSqlQuery query;
        //插入数据到数据表
        QString date = QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss");
        int tt = static_cast<int>(payloadType);
        QString type;
        if(tt == 1)
        {
            type = "FC";
        }
        if(tt == 2)
        {
            type = "Aurora";
        }
        if(tt == 3)
        {
            type = "以太网";
        }
        QByteArray WTba;
        QByteArray FCba;
        WTba.append("0");
        FCba.append("0");
        QString strlen = QString("%1").arg(static_cast<int>(payloadLength));
        query.prepare("INSERT INTO " + tname + " (date,name,Datalen,data,startTime,step,groups,WTHead,FCHead,type) VALUES (:date,:name,:Datalen,:data,:startTime,:step,:groups,:WTHead,:FCHead,:type)");
        query.bindValue(":date",date);
        query.bindValue(":name",name);
        query.bindValue(":Datalen",strlen);
        query.bindValue(":startTime","0");
        query.bindValue(":step","0");
        query.bindValue(":groups",QString("%1").arg(group));
        query.bindValue(":data",payload);
        query.bindValue(":WTHead",WTba);
        query.bindValue(":FCHead",FCba);
        query.bindValue(":type",type);
        if(!query.exec())
        {
            //数据保存失败
            Dlog_info* mflog = new Dlog_info(2,"数据加载失败！");
            mflog->exec();
            return;
        }
        //成功
        Dlog_info* mflog = new Dlog_info(2,"数据加载成功！");
        mflog->exec();
        int num = ui->lab_all->text().toInt();
        num++;
        ui->lab_all->setText(QString("%1").arg(num));
        updateTable();

    }
}

void wid_datacontrol::slot_back()
{
    ui->stackedWidget->setCurrentIndex(0);
}

void wid_datacontrol::on_pushButton_modify_clicked()
{
    //同步更新到数据库并刷新data表格
    QSqlQuery query;
    QByteArray newData = getData();
    query.prepare("UPDATE "+tname+" Set data = :newData WHERE name = :sname");
    query.bindValue(":newData",newData,QSql::Binary);
    query.bindValue(":sname",sname);

    if(!query.exec())
    {
        Dlog_info* mlog = new Dlog_info(2,"数据源值修改失败！");
        mlog->exec();
        return;
    }
    //更新data表
    updateTable();
}

void wid_datacontrol::on_pbtn_infoUp_clicked()
{
    //详细数据上一页
    if(m_currentPage > 0)
    {
        --m_currentPage;
        setDataInfo();
    }
}

void wid_datacontrol::on_pbtn_infoDown_clicked()
{
    //详细数据下一页
    if(m_currentPage < m_totalPage -1)
    {
        ++m_currentPage;
        setDataInfo();
    }
}

void wid_datacontrol::on_pbtn_infoFirst_clicked()
{
    //首页
    if(m_currentPage != 0)
    {
        m_currentPage = 0;
        setDataInfo();
    }
}

void wid_datacontrol::on_pbtn_infoend_clicked()
{
    //尾页
    if(m_currentPage != m_totalPage - 1)
    {
        m_currentPage = m_totalPage -1;
        setDataInfo();
    }
}

void wid_datacontrol::on_pbtn_turn_clicked()
{
    //跳转
    const int page = ui->label_currentpage->value() - 1;
    if(page < 0 || page >= m_totalPage)
    {
        ui->label_currentpage->setValue(m_currentPage + 1);
        return;
    }
    if(page == m_currentPage)
    {
        return;
    }
    m_currentPage = page;
    setDataInfo();
}

void wid_datacontrol::on_pbtn_dataFirst_clicked()
{
    //数据首页
    if(dm_currentPage != 0)
    {
        dm_currentPage = 0;
        updateTable();
    }
}

void wid_datacontrol::on_pbtn_dataUp_clicked()
{
    //数据上一页
    if(dm_currentPage > 0)
    {
        --dm_currentPage;
        updateTable();
    }
}

void wid_datacontrol::on_pbtn_dataDown_clicked()
{
    //数据下一页
    if(dm_currentPage < dm_totalPage -1)
    {
        ++dm_currentPage;
        updateTable();
    }
}

void wid_datacontrol::on_pbtn_dataEnd_clicked()
{
        //数据尾页
    if(m_currentPage != m_totalPage - 1)
    {
        m_currentPage = m_totalPage -1;
        updateTable();
    }
}

void wid_datacontrol::on_pbtn_dataTurn_clicked()
{
        //数据跳转
    const int page = ui->sp_dataCurrentpage->value() - 1;
    if(page < 0 || page >= dm_totalPage)
    {
        ui->sp_dataCurrentpage->setValue(dm_currentPage + 1);
        return;
    }
    if(page == dm_currentPage)
    {
        return;
    }
    dm_currentPage = page;
    updateTable();
}
