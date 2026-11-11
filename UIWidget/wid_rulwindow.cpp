#include "wid_rulwindow.h"
#include "ui_wid_rulwindow.h"

wid_rulwindow::wid_rulwindow(qint64 ipriID,QWidget *parent) :
    QWidget(parent),
    ui(new Ui::wid_rulwindow)
{
    ui->setupUi(this);
    mipriID = ipriID;
    Init();
}

wid_rulwindow::~wid_rulwindow()
{
    mdb.close();
    delete ui;
}

void wid_rulwindow::Init()
{
    ui->tabwid_rul->setColumnCount(3);
    ui->tabwid_rul->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    ui->tabwid_rul->setEditTriggers(QAbstractItemView::NoEditTriggers);
    ui->tabwid_rul->verticalHeader()->setVisible(false);
    ui->tabwid_rul->verticalHeader()->setDefaultSectionSize(50);
    ui->tabwid_rul->horizontalHeader()->setStyleSheet(R"(
                                                      QHeaderView::section{
                                                      background-color:rgb(255,255,255);
                                                      color:rgb(0,0,0);
                                                      font: 75 10pt "微软雅黑";
                                                      border-top:1px solid rgb(70,90,120);
                                                      border-bottom:1px solid rgb(70,90,120);
                                                      border-radius:1px;
                                                      }
                                                      )");
    char* cstr = "00 FF 12 FF 80 81 FF 6E 00 02 00 FA 9F 60 8E FF 00 03 07 FB 4A 33 08 12 BF A0 3B 22 00 00 00 1A 0B 2C 04 7B FF 80 81 FF 6E 00 02 00 FA 9F 60 8E FF 00 03 07 FB 4A 33 08 12 BF A0 3B 22 00 00 00 1A"
                 "FF 80 81 FF 6E 00 02 00 FA 9F 60 8E FF 00 03 07 FB 4A 33 08 12 BF A0 3B 22 00 00 00 1A FF 6E 00 02 00 FA 8E FF 00 03 07 FB 00 02 00 FA 9F 00 00 00 1A 0B 2C 04 7B FF 80 81 22 00 00 00 1A FF 22 00"
                 " 01 00 1A FF FF 00 03 07 FB 60 8E FF 00 03 07 FB 4A 33 00 00 1A FF 6E 00 02 00 FA 12 BF A0 3B 22 00 00 00 1A FF 6E A0 3B 22 00 00 00 1A 0B 00 03 07 FB 00 02 00 FA 9F 00 00 02 00 FA 9F 60 8E FF 00"
                 "03 07 8E FF 00  03 07 6E 00 02 00 FA 9F 60 8E FF 00 03 07 FB 4A 33 08 12 BF 4A 33 08 12 BF 4A 33 08 12 BF";
    QString showtext = QString::fromUtf8(cstr);
    ui->plainTextEdit->setPlainText(showtext);

#if 1
    ui->tableWidget->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    ui->tableWidget->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->tableWidget->setEditTriggers(QAbstractItemView::NoEditTriggers);
    ui->tableWidget->verticalHeader()->setVisible(false);
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
#endif

    ui->tabWidget->tabBar()->hide();
    InitTable();
    InitSql();
    InitVec();

}

void wid_rulwindow::updateTable()
{
    m_ICDPath.clear();
    ui->tabwid_rul->setUpdatesEnabled(false);
    ui->tabwid_rul->clearContents();
    ui->tabwid_rul->setRowCount(0);
    int start = m_currentPage * m_PageSize;
    int count = (m_totalPage == 0)? 0 : qMin(m_PageSize,FileList.size() - start);
    ui->tabwid_rul->setRowCount(count);
    for(int i = 0;i < count;++i)
    {
        QString path = FileList.at(i);
        QJsonDocument doc = readJsonFile(path);
        QString ICD_map;
        if(!doc.isNull())
        {
            if(doc.isObject())
            {
                QJsonObject obj = doc.object();
                QStringList keys = obj.keys();
                //规则名称
                QTableWidgetItem* nameitem = new QTableWidgetItem();
                QFont namefont;
                namefont.setPointSize(11);
                namefont.setFamily("Microsoft YaHei");
                QString nametext = keys.at(0);
                ICD_map =nametext;
                nameitem->setText(nametext);
                nameitem->setFont(namefont);
                nameitem->setBackground(QBrush(QColor(255,255,255)));
                nameitem->setForeground(QBrush(QColor(0,0,0)));
                nameitem->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
                ui->tabwid_rul->setItem(i,0,nameitem);
                ui->comboBox_rulist->addItem(nametext);
                //协议
                QTableWidgetItem* typeitem = new QTableWidgetItem();
                QFont typefont;
                typefont.setPointSize(11);
                typefont.setFamily("Microsoft YaHei");
                QJsonObject devInfo = obj[keys.at(0)].toObject();
                QString typetext = devInfo["type"].toString();
                typeitem->setText(typetext);
                typeitem->setFont(typefont);
                typeitem->setBackground(QBrush(QColor(255,255,255)));
                typeitem->setForeground(QBrush(QColor(0,0,0)));
                typeitem->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
                ui->tabwid_rul->setItem(i,1,typeitem);
                //操作
                wid_action* mwid_act = new wid_action(i,"确认删除ICD吗？");
                connect(mwid_act,&wid_action::sig_delICD,this,&wid_rulwindow::slot_delICD);
                connect(mwid_act,&wid_action::sig_editICD,this,&wid_rulwindow::slot_editICD);
                ui->tabwid_rul->setCellWidget(i,2,mwid_act);
            }
            m_ICDPath.insert(path,ICD_map);
        }
    }
    ui->tabwid_rul->setUpdatesEnabled(true);

    if(m_totalPage == 0)
    {
        ui->label_currentpage->setValue(0);
        ui->label_totalpage->setText(QString("%1").arg(0));
    }
    else {
        ui->label_currentpage->setValue(m_currentPage + 1);
        ui->label_totalpage->setText(QString("%1").arg(m_totalPage));
    }
}

void wid_rulwindow::InitTable()
{
    //读取ICD配置文件
    FileList.clear();
    QString ICDPath = QApplication::applicationDirPath() + "/prj/WTBTS/ICD";
    FileList = findJsonFiles(ICDPath);
    //总页数
    m_totalPage = std::max(1,(FileList.size() + m_PageSize - 1)/ m_PageSize);
    m_currentPage = 0;
#if 1
    //更新规则列表
      updateTable();
#elif 0
    int currrow = ui->tabwid_rul->rowCount();
    for(int fid = 0;fid < FileList.count();fid++)
    {
        QString path = FileList.at(fid);
        QJsonDocument doc = readJsonFile(path);
        if(!doc.isNull())
        {
            if(doc.isObject())
            {
                QJsonObject obj = doc.object();
                QStringList keys = obj.keys();
                //存储到表格中
                QString ICD_map;
                for(int i = 0;i < keys.count();i++)
                {
                    ui->tabwid_rul->insertRow(currrow);
                    //复选框
                    //                    QTableWidgetItem* checkItem = new QTableWidgetItem();
                    //                    checkItem->setCheckState(Qt::Unchecked);
                    //                    checkItem->setFlags(checkItem->flags() & ~Qt::ItemIsEditable);
                    //                    ui->tabwid_rul->setItem(currrow,0,checkItem);
                    //规则名称
                    QTableWidgetItem* nameitem = new QTableWidgetItem();
                    QFont namefont;
                    namefont.setPointSize(11);
                    namefont.setFamily("Microsoft YaHei");
                    QString nametext = keys.at(i);
                    ICD_map =nametext;
                    nameitem->setText(nametext);
                    nameitem->setFont(namefont);
                    nameitem->setBackground(QBrush(QColor(255,255,255)));
                    nameitem->setForeground(QBrush(QColor(0,0,0)));
                    nameitem->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
                    ui->tabwid_rul->setItem(currrow,0,nameitem);
                    ui->comboBox_rulist->addItem(nametext);
                    //协议
                    QTableWidgetItem* typeitem = new QTableWidgetItem();
                    QFont typefont;
                    typefont.setPointSize(11);
                    typefont.setFamily("Microsoft YaHei");
                    QJsonObject devInfo = obj[keys.at(i)].toObject();
                    QString typetext = devInfo["type"].toString();
                    typeitem->setText(typetext);
                    typeitem->setFont(typefont);
                    typeitem->setBackground(QBrush(QColor(255,255,255)));
                    typeitem->setForeground(QBrush(QColor(0,0,0)));
                    typeitem->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
                    ui->tabwid_rul->setItem(currrow,1,typeitem);
                    //操作
                    wid_action* mwid_act = new wid_action(currrow,"确认删除ICD吗？");
                    connect(mwid_act,&wid_action::sig_delICD,this,&wid_rulwindow::slot_delICD);
                    connect(mwid_act,&wid_action::sig_editICD,this,&wid_rulwindow::slot_editICD);
                    ui->tabwid_rul->setCellWidget(currrow,2,mwid_act);
                    currrow++;
                    ui->tabwid_rul->setRowCount(currrow);
                }
                m_ICDPath.insert(path,ICD_map);
            }
        }
    }
#endif
    saveICD();
}

void wid_rulwindow::InitVec()
{
    QStringList names = m_ICDPath.values();
    for(int i =  0;i < names.count();i++)
    {
        QString ICDName = names.at(i);
        QString tableName = "TABLE_ICD_" + ICDName;
        QSqlQuery query;
        //判断是否存在
        QString sqlstr = "SELECT name FROM sqlite_master WHERE type = 'table' AND name = '" + tableName + "';";
        query.exec(sqlstr);
        if(!query.next())
        {
            Dlog_info* mlog = new Dlog_info(2,"数据表不存在！");
            mlog->exec();
            return;
        }
        sqlstr = "SELECT name, type, len FROM "+tableName+";";
        if(!query.exec(sqlstr))
        {
            return;
        }
        QVector<ICDDATA> vec_ICD;
        while(query.next())
        {
            ICDDATA info;
            info.name = query.value("name").toString();
            info.type = query.value("type").toString();
            info.len = query.value("len").toInt();

            vec_ICD.append(info);
        }
        map_ICDData.insert(ICDName,vec_ICD);
    }
}

void wid_rulwindow::InitSql()
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
}

void wid_rulwindow::slot_editICD(int row)
{
    //获取对应行所在数据库中的内容
    QString ICDName = ui->tabwid_rul->item(row,0)->text();
#if 0
    QString tableName = "TABLE_ICD_" + ICDName;
    QSqlQuery query;
    //判断是否存在
    QString sqlstr = "SELECT name FROM sqlite_master WHERE type = 'table' AND name = '" + tableName + "';";
    query.exec(sqlstr);
    if(!query.next())
    {
        Dlog_info* mlog = new Dlog_info(2,"数据表不存在！");
        mlog->exec();
        return;
    }
    sqlstr = "SELECT name, type, len FROM "+tableName+";";
    if(!query.exec(sqlstr))
    {
        return;
    }
    QVector<ICDDATA> vec_ICD;
    while(query.next())
    {
        ICDDATA info;
        info.name = query.value("name").toString();
        info.type = query.value("type").toString();
        info.len = query.value("len").toInt();

        vec_ICD.append(info);
    }
    map_ICDData.insert(ICDName,vec_ICD);
#endif
    QVector<ICDDATA> vec_ICD = map_ICDData.value(ICDName);
    dlog_editRul* mlog_ed = new dlog_editRul(ICDName,vec_ICD);
    mlog_ed->exec();
}

void wid_rulwindow::delICDTable(QString tableName)
{
    QString strsql;
    QString tName = "TABLE_ICD_" + tableName;
    strsql = QString("DROP TABLE IF EXISTS %1;").arg(tName);
    QSqlQuery query;
    if(!query.exec(strsql))
    {
        Dlog_info* mlog = new Dlog_info(2,"数据源删除失败！");
        mlog->exec();
        return;
    }
}

void wid_rulwindow::slot_delICD(int row)
{
    //删除指定行
    QTableWidgetItem *item = ui->tabwid_rul->item(row,0);
    QString ICDName = item->text();
    QStringList pathKeys = m_ICDPath.keys();
    for(int idx = 0;idx < pathKeys.count();idx++)
    {
        QString ICD_map = m_ICDPath.value(pathKeys.at(idx));
        int index = ICD_map.indexOf(ICDName);
        if(index != -1)
        {
            //存在该ICD
            //在文件中删除该节点
            QString filePath = pathKeys.at(idx);
            if(QFile::exists(filePath))
            {
                bool ok = QFile::remove(filePath);
            }
#if 0
            QFile file(filePath);
            if(!file.open(QIODevice::ReadOnly | QIODevice::Text))
            {
                return;
            }
            QByteArray jsonData = file.readAll();
            file.close();
            QJsonParseError error;
            QJsonDocument jsonDoc = QJsonDocument::fromJson(jsonData,&error);
            if(error.error != QJsonParseError::NoError)
            {
                return;
            }
            if(!jsonDoc.isObject())
            {
                return;
            }
            QJsonObject json = jsonDoc.object();
            if(json.contains(ICDName))
            {
                json.remove(ICDName);
            }
            jsonDoc.setObject(json);
            QFile ref(filePath);
            if(!ref.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text))
            {
                return;
            }
            ref.write(jsonDoc.toJson(QJsonDocument::Indented));
            ref.close();
#endif
        }
    }
    ui->tabwid_rul->setRowCount(0);
    ui->comboBox_rulist->clear();
    m_ICDPath.clear();
    delICDTable(ICDName);
    InitTable();
    InitVec();
    emit sig_updateICDtable();
}

QStringList wid_rulwindow::findJsonFiles(const QString &floderPath)
{
    QStringList jsonFiles;
    QDir dir(floderPath);
    if(!dir.exists())
    {
        jsonFiles.clear();
        return jsonFiles;
    }
    //过滤.json文件
    QStringList filters;
    filters << "*.json";

    QFileInfoList fileList = dir.entryInfoList(filters,QDir::Files | QDir::NoDotAndDotDot,QDir::Name);
    for(const QFileInfo &fileInfo : fileList)
    {
        jsonFiles.append(fileInfo.absoluteFilePath());
    }

    return jsonFiles;
}

QJsonDocument wid_rulwindow::readJsonFile(const  QString &filePath)
{
    QFile file(filePath);
    if(!file.open(QIODevice::ReadOnly))
    {
        return QJsonDocument();
    }
    QByteArray data = file.readAll();
    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(data,&error);
    if(error.error != QJsonParseError::NoError)
    {
        file.close();
        return QJsonDocument();
    }
    file.close();
    return doc;
}

void wid_rulwindow::on_pbtn_inport_clicked()
{
    //导入ICD文件
    //获取文件路径   所有文件(*)
    QString innerPath = QFileDialog::getOpenFileName(this,"选择ICD规则文件",QDir::homePath(),"XML 文件(*.xml)");
    if(innerPath.isEmpty())
    {
        return;
    }

    //打开并解析xml格式
    QFile file(innerPath);
    if(!file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        return;
    }
    QXmlStreamReader reader(&file);
    QVector<ICDDATA> vec_Icd;
    QString messageName;
    QString ICDTYPE;
    while(!reader.atEnd() && !reader.hasError())
    {
        QXmlStreamReader::TokenType token = reader.readNext();
        if(token == QXmlStreamReader::StartElement)
        {
            QString tag = reader.name().toString();

            if(tag.compare("root") == 0)
            {
                messageName = reader.attributes().value("messageName").toString();
                ICDTYPE = reader.attributes().value("TYPE").toString();
            }
            else
            {
                QXmlStreamAttributes attrs = reader.attributes();
                ICDDATA data;
                data.name = attrs.value("Name").toString();
                data.type = attrs.value("type").toString();
                data.len = attrs.value("len").toInt();
                data.ICDType = ICDTYPE;
                vec_Icd.append(data);
            }
        }
    }
    file.close();

    //更新到列表和数据库中
    //存储到ICD路径下
    QString FilePath = QApplication::applicationDirPath() + "/prj/WTBTS/ICD";
    FilePath += QString("/") + messageName + ".json";
    saveICD(FilePath,messageName,ICDTYPE,vec_Icd);

    ui->tabwid_rul->setRowCount(0);
    ui->comboBox_rulist->clear();
    m_ICDPath.clear();
    InitTable();
    InitVec();
    emit sig_updateICDtable();
}

void wid_rulwindow::saveICD(QString path,QString name,QString type,QVector<ICDDATA> data)
{
    //保存信息到ICD文件夹目录下
    QJsonObject innerObj;
    QString TableName = QString("TABLE_ICD_%1").arg(name);
    innerObj["data"] = TableName;
    innerObj["type"] = type;

    QJsonObject rootObj;
    rootObj[name] = innerObj;

    QJsonDocument doc(rootObj);

    QFile f(path);
    if(!f.open(QIODevice::WriteOnly | QIODevice::Text))
    {
        return;
    }
    f.write(doc.toJson(QJsonDocument::Indented));
    f.close();

    //保存ICD数据内容到数据库中
    QSqlQuery query;
    //判断是否存在
    QString sqlstr = "SELECT name FROM sqlite_master WHERE type = 'table' AND name = '" + TableName + "';";
    query.exec(sqlstr);
    if(!query.next())
    {
        //不存在则创建表
        sqlstr = "CREATE TABLE " + TableName + " (name TEXT PRIMARY KEY,type TEXT NOT NULL,len INT NOT NULL)";
        bool ok = query.exec(sqlstr);
        if(!ok)
        {
            //建表失败
            Dlog_info* mlog = new Dlog_info(2,"数据表创建失败！");
            mlog->exec();
            return;
        }
    }
    //获取数据
    //    QSqlDatabase::database().transaction();
    int row = data.count();
    for(int i = 0; i < row;i++)
    {
        QString name = data.at(i).name;
        QString type = data.at(i).type;
        int len = data.at(i).len;
        query.prepare("INSERT INTO " + TableName + " (name,type,len) VALUES (:name,:type,:len)");
        query.bindValue(":name",name);
        query.bindValue(":type",type);
        query.bindValue(":len",len);

        if(!query.exec())
        {
            //数据保存失败
            Dlog_info* mflog = new Dlog_info(2,"数据保存失败！");
            mflog->exec();
            return;
        }
    }
}


void wid_rulwindow::on_pbtn_export_clicked()
{
    //导出ICD文件
    QStringList list_rul;
    for(int i = 0;i <ui->comboBox_rulist->count();i++)
    {
        list_rul <<ui->comboBox_rulist->itemText(i);
    }
    dlog_export* mlog_ex = new dlog_export(list_rul);
    connect(mlog_ex,&dlog_export::sig_selectRul,this,&wid_rulwindow::slot_exrul);
    mlog_ex->exec();
}


void wid_rulwindow::slot_exrul(QString savepath,int index)
{
    QString v = ui->comboBox_rulist->itemText(index);
    //获取协议类型

    QString ICDType = ui->tabwid_rul->item(index,1)->text();
    //    QList<QStringList> valuelist = m_ICDPath.values();

#if 1
    //获取数据库中对应结构并存储为xml格式导出
    QString tableName = "TABLE_ICD_" + v;
    QSqlQuery query;
    //判断是否存在
    QString sqlstr = "SELECT name FROM sqlite_master WHERE type = 'table' AND name = '" + tableName + "';";
    query.exec(sqlstr);
    if(!query.next())
    {
        Dlog_info* mlog = new Dlog_info(2,"数据表不存在！");
        mlog->exec();
        return;
    }

    sqlstr = "SELECT name, type, len FROM "+tableName+";";
    if(!query.exec(sqlstr))
    {
        return;
    }
    QVector<ICDDATA> vec_ICD;
    while(query.next())
    {
        ICDDATA info;
        info.name = query.value("name").toString();
        info.type = query.value("type").toString();
        info.len = query.value("len").toInt();
        info.ICDType = ICDType;

        vec_ICD.append(info);
    }
    bool ok = exportToXML(vec_ICD,savepath + "/WT_ICD_" + v + ".xml",v);
    if(ok)
    {
        Dlog_info* mslog = new Dlog_info(2,"导出成功！");
        mslog->exec();
    }
    else {
        return;
    }

#endif

#if 0

    for(int i = 0;i < valuelist.count();i++)
    {
        QStringList nameList = valuelist.at(i);
        for(int j = 0;j < nameList.count();j++)
        {
            if(nameList.at(j) == v)
            {
                //save file
                QString sourcePath = m_ICDPath.key(nameList);
                if(copyFile(sourcePath,savepath))
                {
                    Dlog_info* mslog = new Dlog_info(2,"导出成功！");
                    mslog->exec();
                }
                else
                {
                    return;
                }
            }
        }
    }
#endif
}

bool wid_rulwindow::exportToXML(QVector<ICDDATA> vec, QString savePath, QString name)
{
    QFile file(savePath);
    file.open(QIODevice::WriteOnly | QIODevice::Text);
    QXmlStreamWriter writer(&file);

    writer.setAutoFormatting(true);
    writer.setAutoFormattingIndent(4);
    writer.setCodec("UTF-8");

    writer.writeStartDocument("1.0");
    writer.writeStartElement("root");
    writer.writeAttribute("messageName",name);
    writer.writeAttribute("TYPE",vec.at(0).ICDType);

    for(int i = 0;i < vec.count();i++)
    {
        writer.writeStartElement("item");
        writer.writeAttribute("Name",vec.at(i).name);
        writer.writeAttribute("type",vec.at(i).type);
        writer.writeAttribute("len",QString("%1").arg(vec.at(i).len));
        writer.writeEndElement();
    }
    writer.writeEndElement();
    writer.writeEndDocument();
    file.close();

    return true;
}

bool wid_rulwindow::copyFile(QString source, QString dst)
{
    if(!QFile::exists(source))
    {
        Dlog_info* mlog = new Dlog_info(2,"源ICD文件丢失！");
        mlog->exec();
        return false;
    }
    QFileInfo sourceFileInfo(source);
    QString sourceName = sourceFileInfo.fileName();
    dst += "/" + sourceName;
    //将src内容写入dst
    //1.读取文件内容
    QFile file(source);
    if(!file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        file.close();
        return false;
    }
    QByteArray jsonData = file.readAll();
    file.close();
    QJsonParseError error;
    QJsonDocument jsonDoc = QJsonDocument::fromJson(jsonData,&error);
    if(error.error != QJsonParseError::NoError)
    {
        return false;
    }
    if(!jsonDoc.isObject())
    {
        return false;
    }
    //2.保存到新文件
    QFile ref(dst);
    if(!ref.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text))
    {
        ref.close();
        return false;
    }
    ref.write(jsonDoc.toJson(QJsonDocument::Indented));
    ref.close();
    return true;
}

void wid_rulwindow::saveICD()
{
    QStringList chList = PM()->GetCHLs(mipriID,"FC");
    for(int chindex = 0;chindex < chList.count();chindex++)
    {
        WTContrl *pCtrl = (WTContrl *)(PM()->GetWTCtrl(chList.at(chindex)));
        CMessageBlock * cmblok = pCtrl->cMessageBlock;
        QVector<MsgBlock>m_vMessage1 = cmblok->m_vMessage1;
        pCtrl->cMessageBlock->m_vMessage1.clear();//清空ctrl中的ICD
        QStringList keysList = m_ICDPath.keys();
        for(int idx = 0;idx < keysList.count();idx++)
        {
            QString FilePath = keysList.at(idx);
            QString icdlist = m_ICDPath.value(keysList.at(idx));
            MsgBlock msgblock;
            for(int icd = 0;icd < 1;icd++)
            {
                QString icdname = icdlist;//icdlist.at(icd);
                if(icdname.contains("FC"))
                {
                    msgblock.sMsgID = "FC";
                }
                if(icdname.contains("Aurora"))
                {
                    msgblock.sMsgID = "Aurora";
                }
                if(icdname.contains("以太网"))
                {
                    msgblock.sMsgID = "以太网";
                }
                msgblock.qmapini.insert(icdlist.at(icd),FilePath);
            }
            m_vMessage1.append(msgblock);
        }
        pCtrl->cMessageBlock->m_vMessage1 = m_vMessage1;
    }
}

void wid_rulwindow::on_pbtn_newrul_clicked()
{
    //新建ICD规则
    dlog_addrul* mlog_add = new dlog_addrul();
    connect(mlog_add,&dlog_addrul::sig_addrul,this,&wid_rulwindow::slot_addrul);
    mlog_add->exec();
    //新增了ICD规则后
    emit sig_updateICDtable();
}

void wid_rulwindow::slot_addrul()
{
    ui->tabwid_rul->setRowCount(0);
    ui->comboBox_rulist->clear();
    m_ICDPath.clear();
    InitTable();
    InitVec();
    emit sig_updateICDtable();
}

void wid_rulwindow::on_pushButton_clicked()
{
    //解析测试
    QString text = ui->plainTextEdit->toPlainText();
    QByteArray ba = text.toUtf8();
    char* c_str = qstrdup(ba.constData());
    // 获取当前的协议并进行转换
#if 1
    //获取当前解析所用规则
    QString name = ui->comboBox_rulist->currentText();
    QVector<ICDDATA> icds = map_ICDData.value(name);
    ui->tableWidget->setRowCount(icds.count());
    for(int idx = 0;idx < icds.count();idx++)
    {
        //字段名
        QTableWidgetItem* icditem = new QTableWidgetItem();
        QFont icdfont;
        icdfont.setPointSize(11);
        icdfont.setFamily("Microsoft YaHei");
        icditem->setText(icds.at(idx).name);
        icditem->setFont(icdfont);
        icditem->setBackground(QBrush(QColor(255,255,255)));
        icditem->setForeground(QBrush(QColor(0,0,0)));
        icditem->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
        ui->tableWidget->setItem(idx,0,icditem);

        //值
        QTableWidgetItem* valitem = new QTableWidgetItem();
        QFont valfont;
        valfont.setPointSize(11);
        valfont.setFamily("Microsoft YaHei");
        QRandomGenerator* rng = QRandomGenerator::global();
        qint64 v = rng->bounded(0,255);
        valitem->setText(QString("%1").arg(v));
        valitem->setFont(valfont);
        valitem->setBackground(QBrush(QColor(255,255,255)));
        valitem->setForeground(QBrush(QColor(0,0,0)));
        valitem->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
        ui->tableWidget->setItem(idx,1,valitem);
    }
#endif

#if 0
    char* cstr = "00 FF 12 FF 80 81 FF 6E 00 02 00 FA 9F 60 8E FF 00 03 07 FB 4A 33 08 12 BF A0 3B 22 00 00 00 1A 0B 2C 04 7B FF 80 81 FF 6E 00 02 00 FA 9F 60 8E FF 00 03 07 FB 4A 33 08 12 BF A0 3B 22 00 00 00 1A"
                 "FF 80 81 FF 6E 00 02 00 FA 9F 60 8E FF 00 03 07 FB 4A 33 08 12 BF A0 3B 22 00 00 00 1A FF 6E 00 02 00 FA 8E FF 00 03 07 FB 00 02 00 FA 9F 00 00 00 1A 0B 2C 04 7B FF 80 81 22 00 00 00 1A FF 22 00"
                 " 01 00 1A FF FF 00 03 07 FB 60 8E FF 00 03 07 FB 4A 33 00 00 1A FF 6E 00 02 00 FA 12 BF A0 3B 22 00 00 00 1A FF 6E A0 3B 22 00 00 00 1A 0B 00 03 07 FB 00 02 00 FA 9F 00 00 02 00 FA 9F 60 8E FF 00"
                 "03 07 8E FF 00  03 07 6E 00 02 00 FA 9F 60 8E FF 00 03 07 FB 4A 33 08 12 BF 4A 33 08 12 BF 4A 33 08 12 BF";
    QString showtext = QString::fromUtf8(cstr);
    ui->plainTextEdit_2->setPlainText(showtext);
#endif
    delete[] c_str;
}

void wid_rulwindow::on_pbtn_rulFirst_clicked()
{
    //首页
    if(m_currentPage != 0)
    {
        m_currentPage = 0;
        InitTable();
        InitVec();
    }
}

void wid_rulwindow::on_pbtn_rulUp_clicked()
{
    //上一页
    if(m_currentPage > 0)
    {
        --m_currentPage;
        InitTable();
        InitVec();
    }
}

void wid_rulwindow::on_pbtn_rulDown_clicked()
{
    //下一页
    if(m_currentPage < m_totalPage -1)
    {
        ++m_currentPage;
        InitTable();
        InitVec();
    }
}

void wid_rulwindow::on_pbtn_rulend_clicked()
{
    //尾页
    if(m_currentPage != m_totalPage - 1)
    {
        m_currentPage = m_totalPage -1;
        InitTable();
        InitVec();
    }
}

void wid_rulwindow::on_pbtn_turn_clicked()
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
    InitTable();
    InitVec();
}
