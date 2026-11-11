#include "wid_stack.h"
#include "ui_wid_stack.h"
#include "ELSForm.h"
#include "fcnetform.h"
wid_stack::wid_stack(qint64 ipriID,int id,QWidget *parent) :
    QWidget(parent),
    ui(new Ui::wid_stack)
{
    ui->setupUi(this);
    mipriID = ipriID;
    m_devConfigMap.clear();
    InitUI(id);
    InitInfo();
    timerStatus = new QTimer(this);
    connect(timerStatus,&QTimer::timeout,this,&wid_stack::checkLoad);
    timerLogin = new QTimer(this);
    connect(timerLogin,&QTimer::timeout,this,&wid_stack::FClogin);
    timerFCclose = new QTimer(this);
    connect(timerFCclose,&QTimer::timeout,this,&wid_stack::FCclose);
    timerAStatus = new QTimer(this);
    connect(timerAStatus,&QTimer::timeout,this,&wid_stack::updateAuroraStatus);

    timerMem = new QTimer(this);
    connect(timerMem,&QTimer::timeout,this,&wid_stack::updateMem);

    m_id = id;
    if(id == 0)
    {
        //temper
        timerTem = new QTimer(this);
        connect(timerTem,&QTimer::timeout,this,&wid_stack::updateTem);
        m_temperatureReader = new CpuTemperatureReader();
        timerMem->start(1000);
        //主页
        timerTem->start(1500);
    }
}

wid_stack::~wid_stack()
{
    delete ui;
}

void wid_stack::InitUI(int id)
{
    ui->btn_fclogin->hide();
    InitTree();
    ui->btn_open_3->hide();
    ui->pushButton_tb->hide();
    ui->lineEdit_name->setEnabled(false);
    ui->lineEdit_type->setEnabled(false);
    ui->lineEdit_status->setEnabled(false);
    ui->lineEdit_tem->setEnabled(false);
    ui->lineEdit_mem->setEnabled(false);
    ui->lineEdit_interface->setEnabled(false);
    ui->lineEdit_aurora->setEnabled(false);
    ui->lineEdit_cpu->setEnabled(false);
    ui->lineEdit_fc->setEnabled(false);
    ui->lineEdit_interface->setEnabled(false);
    ui->lineEdit_memcheck->setEnabled(false);
    ui->lineEdit_memselfcheck->setEnabled(false);
    ui->lineEdit_Netinterface->setEnabled(false);
    //默认Aurora为从模式
    ui->stackedWidget->setCurrentIndex(id);
    ui->treeWidget->expandAll();
    ui->treeWidget->setContextMenuPolicy(Qt::CustomContextMenu);
    ui->gridLayout_3->setColumnStretch(0,0);
    ui->gridLayout_3->setColumnStretch(1,1);
    ui->widget->hide();
    if(id == 0)
    {
        ui->widget_31->hide();
        ui->widget_22->hide();
        ui->widget_21->hide();
        ui->gridLayout_3->setColumnStretch(0,2);
        ui->gridLayout_3->setColumnStretch(1,8);
        ui->widget->show();
        connect(ui->treeWidget,&QTreeWidget::customContextMenuRequested,this,&wid_stack::onTreeContextMenu);
    }
    connect(ui->treeWidget,&QTreeWidget::itemClicked,this,&wid_stack::onItemClicked);
    if(id == 1)
    {
        //协议规则
        wid_rulwindow* mwid_rul = new wid_rulwindow(mipriID);
        connect(mwid_rul,&wid_rulwindow::sig_updateICDtable,this,&wid_stack::slot_updateICDtable);
        ui->glay_rul->addWidget(mwid_rul);
    }
    if(id == 4)
    {
        //数据源管理
#if 0
        mwid_datacontrol = new wid_datacontrol(mipriID);
        connect(mwid_datacontrol,&wid_datacontrol::sig_scpData,this,&wid_stack::slot_scpShow);
        ui->glay_datacontrol->addWidget(mwid_datacontrol);
#endif
        wid_datasource* m_dataSource = new wid_datasource();
        ui->glay_datacontrol->addWidget(m_dataSource);
    }
    if(id == 5)
    {
        //回放配置
        WidReview* mwid_review = new WidReview();
        ui->glay_reviewset->addWidget(mwid_review);
    }
    if(id == 6)
    {
        //场景并行
        mwid_scp = new wid_scp(mipriID);
        connect(this,&wid_stack::sig_updateFC,mwid_scp,&wid_scp::slot_FCOnline);
        connect(this,&wid_stack::sig_updateAurora,mwid_scp,&wid_scp::slot_AuroraOnline);
        connect(this,&wid_stack::sig_scpData,mwid_scp,&wid_scp::readDataSource);
        connect(this,&wid_stack::sig_ICDTShow,mwid_scp,&wid_scp::readICDTable);
        connect(mwid_scp,&wid_scp::sig_botspeed,this,&wid_stack::slot_botspeed);
        ui->glay_scp->addWidget(mwid_scp);
    }
    if(id == 7)
    {
        //数据监控
        wid_datamonitor* mwid_datamonitor = new wid_datamonitor(mipriID);
        ui->glay_datamonitor->addWidget(mwid_datamonitor);
    }
}

void wid_stack::slot_botspeed(int ch, int speed)
{
    //下发
    AURORACONTROL ac;
    ac.devId = AuroraID;
    ac.scmd = speed;
    QStringList chList = PM()->GetCHLs(mipriID,"Aurora");
    WTContrl *pCtrl = (WTContrl *)(PM()->GetWTCtrl(chList.at(ch)));//创建设备句柄及设置设备属性默认使用通道1的ctrl
    QVariant mCmd = QVariant::fromValue(ac);
    int result = pCtrl->Contrl("setspeed",mCmd);
    QString text = QString("Aurora通道%1").arg(ch+1);
    if(result != 0)
    {
        text  += "波特率设置失败！";
    }
    else
    {
        text += "波特率设置成功！";
    }
    Dlog_info* mlog = new Dlog_info(2,text);
    mlog->exec();
}


void wid_stack::onTreeContextMenu(const QPoint &pos)
{
    delIndex = -1;
    copyIndex = -1;
    QTreeWidgetItem* item = ui->treeWidget->itemAt(pos);
    if(!item)
    {
        return;
    }
    QMenu menu(this);
    QAction* actAdd = menu.addAction(QIcon(":/Images/Icons1/ICON_Plus.png"),"添加设备");
    QAction* actDel = menu.addAction(QIcon(":/Images/Icons1/ICON_Delete.png"),"删除设备");
    QAction* actCopy = menu.addAction(QIcon(":/Images/Icons1/Icon_Arrow1.png"),"复制设备");
    //    QAction* actOpen = menu.addAction(QIcon(":/Images/Icons1/ICON_Delete.png"),"打开板卡");
    //    QAction* actClose = menu.addAction(QIcon(":/Images/Icons1/Icon_Arrow1.png"),"关闭板卡");
    QAction* actEdit = menu.addAction(QIcon(":/Images/Icons1/Icon_Menu.png"),"编辑设备");
    //    QAction* actsend = menu.addAction(QIcon(":/Images/Icons2/Icon_Navigation.png"),"发送");
    //    QAction* actstop = menu.addAction(QIcon(":/Images/Icons4/ICON_play.png"),"停止发送");

    menu.setStyleSheet(R"(
                       QMenu {
                       color:rgb(0,0,0);
                       }
                       QMenu::hover{
                       color:rgb(166,171,158);
                       }
                       )");
    if(!item->parent())
    {
        //根节点--所有设备
        //        actAdd->setVisible(true);
        //        actDel->setVisible(false);
        //        actCopy->setVisible(false);
        //        if(menu.exec(ui->treeWidget->viewport()->mapToGlobal(pos)) == actAdd)
        //        {
        //            //添加设备节点---弹出选项窗口
        //            dlog_devadd* dlog_add = new dlog_devadd();
        //            connect(dlog_add,&dlog_devadd::sig_addDev,this,&wid_stack::slot_addDev);
        //            dlog_add->exec();
        //        }
    }
    else
    {
        //设备节点
        actAdd->setVisible(false);
        actDel->setVisible(false);
        actCopy->setVisible(false);
        actEdit->setVisible(true);
        if(item->text(0) == "FC设备-01" || item->text(0) == "Aurora设备-01")
        {
            return;
        }
        if(menu.exec(ui->treeWidget->viewport()->mapToGlobal(pos)) == actEdit)
        {
            //编辑节点
            dlog_editnet* mlog_editnet = new dlog_editnet();
            mlog_editnet->exec();
            return;
        }
        if(menu.exec(ui->treeWidget->viewport()->mapToGlobal(pos)) == actDel)
        {
            //删除设备节点---弹出确认窗口

            delIndex = item->parent()->indexOfChild(item);
            Dlog_info* dlog_del = new Dlog_info(0,"确认删除设备吗？");
            connect(dlog_del,&Dlog_info::sig_delItem,this,&wid_stack::slot_delItem);
            dlog_del->exec();
        }
        if(menu.exec(ui->treeWidget->viewport()->mapToGlobal(pos)) == actCopy)
        {
            //复制设备节点
            copyIndex = item->parent()->indexOfChild(item);
            Dlog_info* dlog_copy = new Dlog_info(1,"确认复制设备吗？");
            connect(dlog_copy,&Dlog_info::sig_copyItem,this,&wid_stack::slot_copyItem);
            dlog_copy->exec();
        }
    }
}

void wid_stack::slot_addDev(DEVINFO info)
{
    //保存到配置文件
    QMap<QString,QString> map_devinfo;
    //读取文件配置的设备信息
    QString devPath = QApplication::applicationDirPath() + "/prj/WTBTS/DevicesConfig/DevConfig.json";
    QFile file(devPath);
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
    QStringList keys = json.keys();
    if(keys.contains(info.devname))
    {
        //已有该设备
        Dlog_info* mlog_info = new Dlog_info(2,"已存在同名设备！");
        mlog_info->exec();
        return;
    }
    //写入文件
    QJsonObject adddev;
    adddev.insert("type",QString("%1").arg(info.type));
    adddev.insert("ID",QString("%1").arg(info.id));
    adddev.insert("N_Port",info.N_Port);
    adddev.insert("set",info.set);
    adddev.insert("IP",info.IP);
    adddev.insert("Port",info.Port);
    adddev.insert("Mac",info.Mac);
    adddev.insert("status",info.status);
    adddev.insert("ch",info.ch);
    json.insert(info.devname,adddev);
    jsonDoc.setObject(json);
    //保存回文件
    QFile ref(devPath);
    if(!ref.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text))
    {
        return;
    }
    ref.write(jsonDoc.toJson(QJsonDocument::Indented));
    ref.close();

    //添加节点
    QString statusPath = ":/images/radio_normal.png";
    QTreeWidgetItem* topItem = ui->treeWidget->topLevelItem(0);
    QTreeWidgetItem* item = new QTreeWidgetItem();
    item->setText(0,info.devname);
    QFont f = item->font(0);
    f.setPointSize(11);
    f.setFamily("Microsoft YaHei");
    item->setFont(0,f);
    item->setBackground(0,QBrush(QColor(21,34,74)));
    item->setForeground(0,QBrush(QColor(166,200,243)));
    item->setIcon(0,QIcon(statusPath));
    topItem->addChild(item);

    map_devinfo.insert("type",QString("%1").arg(info.type));
    map_devinfo.insert("ID",QString("%1").arg(info.id));
    map_devinfo.insert("N_Port",info.N_Port);
    map_devinfo.insert("set",info.set);
    map_devinfo.insert("IP",info.IP);
    map_devinfo.insert("Port",info.Port);
    map_devinfo.insert("Mac",info.Mac);
    map_devinfo.insert("status",info.status);
    map_devinfo.insert("ch",info.ch);
    m_devConfigMap.insert(item,map_devinfo);
    ui->lineEdit_interface->setText(QString("%1").arg(m_devConfigMap.count()));
    //    emit sig_updateItem();
}

void wid_stack::slot_delItem()
{
    //删除节点
    QTreeWidgetItem* topItem = ui->treeWidget->topLevelItem(0);
    QTreeWidgetItem* item = topItem->takeChild(delIndex);
    QString delStr = item->text(0);

    //获取删除节点的type和id
    QString deltype;
    QString delid;
    int index = delStr.indexOf("-");
    delid = delStr.mid(index + 1);
    if(delStr.contains("FC"))
    {
        deltype = "0";
    }
    if(delStr.contains("Aurora"))
    {
        deltype = "1";
    }
    if(delStr.contains("以太网"))
    {
        deltype = "2";
    }

    m_devConfigMap.remove(item);
    delete item;
    QString devPath = QApplication::applicationDirPath() + "/prj/WTBTS/DevicesConfig/DevConfig.json";
    QFile file(devPath);
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
    QString delss;
    QStringList jsonk =  json.keys();
    for(int i = 0;i <jsonk.count();i++)
    {
        QJsonObject devobj = json[jsonk.at(i)].toObject();
        QString id = devobj["ID"].toString();
        QString type = devobj["type"].toString();
        if(id == delid && type == deltype)
        {
            delss = jsonk.at(i);
        }
    }
    json.remove(delss);
    jsonDoc.setObject(json);
    //删除该item
    QFile ref(devPath);
    if(!ref.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text))
    {
        return;
    }
    ref.write(jsonDoc.toJson(QJsonDocument::Indented));
    ref.close();

    //    emit sig_updateItem();
}

void wid_stack::slot_copyItem(QString name)
{

    int index = name.indexOf("-");
    if(index == -1)
    {
        //格式错误
        Dlog_info* mlog = new Dlog_info(2,"新设备名格式错误！");
        mlog->exec();
        return;
    }
    //复制节点
    QTreeWidgetItem* sourceItem = ui->treeWidget->topLevelItem(0)->child(copyIndex);
    QMap<QString,QString> sourceInfo = m_devConfigMap.value(sourceItem);
    QString statusPath = ":/images/radio_normal.png";
    QTreeWidgetItem* newItem = new QTreeWidgetItem();

    sourceInfo["status"] = "0";
    sourceInfo["ID"] = name.mid(index + 1);

    //写入文件
    QString devPath = QApplication::applicationDirPath() + "/prj/WTBTS/DevicesConfig/DevConfig.json";
    QFile file(devPath);
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
    QStringList keys = json.keys();
    if(keys.contains(name))
    {
        //已有该设备
        Dlog_info* mlog_info = new Dlog_info(2,"已存在同名设备！");
        mlog_info->exec();
        return;
    }
    QJsonObject adddev;
    adddev.insert("type",sourceInfo.value("type"));
    adddev.insert("ID",sourceInfo.value("ID"));
    adddev.insert("N_Port",sourceInfo.value("N_Port"));
    adddev.insert("set",sourceInfo.value("set"));
    adddev.insert("IP",sourceInfo.value("IP"));
    adddev.insert("Port",sourceInfo.value("Port"));
    adddev.insert("Mac",sourceInfo.value("Mac"));
    adddev.insert("status",sourceInfo.value("status"));
    adddev.insert("ch",sourceInfo.value("ch"));
    json.insert(name,adddev);
    jsonDoc.setObject(json);
    //保存回文件
    QFile ref(devPath);
    if(!ref.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text))
    {
        return;
    }
    ref.write(jsonDoc.toJson(QJsonDocument::Indented));
    ref.close();
    //新增item
    newItem->setText(0,name);
    QFont f = newItem->font(0);
    f.setPointSize(11);
    f.setFamily("Microsoft YaHei");
    newItem->setFont(0,f);
    newItem->setBackground(0,QBrush(QColor(21,34,74)));
    newItem->setForeground(0,QBrush(QColor(166,200,243)));
    newItem->setIcon(0,QIcon(statusPath));
    ui->treeWidget->topLevelItem(0)->addChild(newItem);
    m_devConfigMap.insert(newItem,sourceInfo);

    //    emit sig_updateItem();
}

void wid_stack::onItemClicked(QTreeWidgetItem *item, int column)
{
    if(!item->parent())
    {
        //根节点
        emit sig_topItemClick(true,0);
    }
    else
    {
        //子节点
        int row = item->parent()->indexOfChild(item);
        emit sig_topItemClick(false,row);
        //显示设备属性到界面
        QMap<QString,QString> map_iteminfo = m_devConfigMap.value(item);
        QString devname;
        QString s_type;
        QString s_status;
        int id = map_iteminfo.value("ID").toInt();
        QString type = map_iteminfo.value("type");
        QString status = map_iteminfo.value("status");
        if(type == "0")
        {
            devname = "FC设备";
            s_type = "FC";
            ui->btn_fclogin->show();
            if(fcStart)
            {
                ui->btn_open->setText("关闭设备");
                ui->btn_fclogin->setEnabled(true);
            }
            else
            {
                ui->btn_open->setText("打开设备");
                ui->btn_fclogin->setEnabled(false);
            }

        }
        if(type == "1")
        {
            devname = "Aurora设备";
            s_type = "Aurora";
            if(auroaStart)
            {
                ui->btn_open->setText("关闭设备");
            }
            else
            {
                ui->btn_open->setText("打开设备");
            }
            ui->btn_fclogin->hide();
        }
        if(type == "2")
        {
            devname = "千兆网设备";
            s_type = "千兆网";
            if(netStart)
            {
                ui->btn_open->setText("关闭设备");
            }
            else
            {
                ui->btn_open->setText("打开设备");
            }
            ui->btn_fclogin->hide();
        }
        if(status == "0")
        {
            s_status = "断开";
        }
        if(status == "1")
        {
            s_status = "认证中";
        }
        if(status == "2")
        {
            s_status = "已认证";
        }
        devname += QString("-%1").arg(id,2,10,QChar('0'));
        ui->lineEdit_name->setText(devname);
        ui->lineEdit_type->setText(s_type);
        ui->lineEdit_status->setText(s_status);
    }
}

void wid_stack::slot_TOPClick(bool top,int row)
{
    QTreeWidgetItem* p = ui->treeWidget->topLevelItem(0);
    if(top)
    {
        p->setSelected(true);
        ui->treeWidget->setCurrentItem(p);
    }
    else
    {
        QTreeWidgetItem* c = p->child(row);
        c->setSelected(true);
        ui->treeWidget->setCurrentItem(c);
    }
}

void wid_stack::slot_updateitem(int fcstatus)
{
    updateTree(fcstatus);
}

void wid_stack::slot_updateAitem(int astatus)
{
    updateATree(astatus);
}

void wid_stack::updateATree(int astatus)
{
    //更改刷新列表的方式，仅修改对应设备的在线图标状态显示，不重新读取影响item点击的状态
    //Aurora
    QTreeWidgetItem* item = map_devItem.value("Aurora设备-01");
    QString  statusPath;
    if(astatus == 0)
    {
        statusPath = ":/images/radio_normal.png";
    }
    else if(astatus == 1)
    {
        statusPath = ":/images/radio_selected.png";
    }
    item->setIcon(0,QIcon(statusPath));
}

void wid_stack::updateTree(int fcstatus)
{
    //更改刷新列表的方式，仅修改对应设备的在线图标状态显示，不重新读取影响item点击的状态
    //FC
    QTreeWidgetItem* item = map_devItem.value("FC设备-01");
    QString  statusPath;
    if(fcstatus == 0)
    {
        statusPath = ":/images/radio_normal.png";
    }
    else if(fcstatus == 1)
    {
        statusPath = ":/images/radio_selected.png";
    }
    item->setIcon(0,QIcon(statusPath));
#if 0
    m_devConfigMap.clear();
    map_devItem.clear();
    //读取文件配置的设备信息
    QString devPath = QApplication::applicationDirPath() + "/prj/WTBTS/DevicesConfig/DevConfig.json";
    QFile file(devPath);
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
    QStringList keys = json.keys();
    //删除默认的
    QTreeWidgetItem* topItem = ui->treeWidget->topLevelItem(0);
    QList<QTreeWidgetItem*> children = topItem->takeChildren();
    for(QTreeWidgetItem* child:children)
    {
        delete child;
    }
    //读取配置文件中的设备信息
    for(int i = 0;i <keys.count();i++)
    {
        QString devName = keys.at(i);
        QTreeWidgetItem* item = new QTreeWidgetItem();
        QJsonObject devInfo = json[devName].toObject();
        QString str;
        QString strnum;
        QString statusPath;
        int speed;
        int dtype;
        QMap<QString,QString> map_devinfo;
        foreach(QString config,devInfo.keys())
        {
            map_devinfo.insert(config,devInfo[config].toString());
            if(config == "type")
            {
                if(devInfo[config].toString() == "0")
                {
                    str = "FC设备";
                    dtype = 0;
                    item->setToolTip(1,"FC");

                    QStringList chList = PM()->GetCHLs(mipriID,"FC");
                    WTContrl *pCtrl = (WTContrl *)(PM()->GetWTCtrl(chList.at(0)));//创建设备句柄及设置设备属性默认使用通道1的ctrl
                    if(pCtrl->m_bNetWork == false)
                    {
                        statusPath = ":/images/radio_normal.png";
                    }
                    else
                    {
                        statusPath = ":/images/radio_selected.png";
                    }
                }
                if(devInfo[config].toString() == "1")
                {
                    str = "Aurora设备";
                    dtype = 1;
                }
                if(devInfo[config].toString() == "2")
                {
                    str = "千兆网设备";
                    dtype = 2;
                }
            }
            if(config == "speed")
            {
                speed = devInfo[config].toInt();
            }
            if(config == "ID")
            {
                QString id = devInfo[config].toString();
                int num = id.toInt();
                strnum = QString("-%1").arg(num,2,10,QChar('0'));
            }
        }
        //        map_Devspeed.insert(dtype,speed);
        item->setText(0,str + strnum);
        QFont f = item->font(0);
        f.setPointSize(11);
        f.setFamily("Microsoft YaHei");
        item->setFont(0,f);
        item->setBackground(0,QBrush(QColor(255,255,255)));
        item->setForeground(0,QBrush(QColor(0,0,0)));
        item->setIcon(0,QIcon(statusPath));
        topItem->addChild(item);
        m_devConfigMap.insert(item,map_devinfo);
        map_devItem.insert(str + strnum,item);
    }
#endif
}

void wid_stack::InitTree()
{
    //读取文件配置的设备信息
    QString devPath = QApplication::applicationDirPath() + "/prj/WTBTS/DevicesConfig/DevConfig.json";
    QFile file(devPath);
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
    QStringList keys = json.keys();
    //删除默认的
    QTreeWidgetItem* topItem = ui->treeWidget->topLevelItem(0);
    QList<QTreeWidgetItem*> children = topItem->takeChildren();
    for(QTreeWidgetItem* child:children)
    {
        delete child;
    }
    //读取配置文件中的设备信息
    for(int i = 0;i <keys.count();i++)
    {
        QString devName = keys.at(i);
        QTreeWidgetItem* item = new QTreeWidgetItem();
        QJsonObject devInfo = json[devName].toObject();
        QString str;
        QString strnum;
        QString statusPath;
        int speed;
        int dtype;
        QMap<QString,QString> map_devinfo;
        foreach(QString config,devInfo.keys())
        {
            map_devinfo.insert(config,devInfo[config].toString());
            if(config == "type")
            {
                if(devInfo[config].toString() == "0")
                {
                    str = "FC设备";
                    dtype = 0;
                    item->setToolTip(1,"FC");
                }
                if(devInfo[config].toString() == "1")
                {
                    str = "Aurora设备";
                    item->setToolTip(1,"Aurora");
                    dtype = 1;
                }
                if(devInfo[config].toString() == "2")
                {
                    str = "千兆网设备";
                    dtype = 2;
                }
            }
            if(config == "speed")
            {
                speed = devInfo[config].toInt();
            }
            if(config == "ID")
            {
                QString id = devInfo[config].toString();
                int num = id.toInt();
                strnum = QString("-%1").arg(num,2,10,QChar('0'));
            }
            if(config == "status")
            {
                if(devInfo[config].toString() == "1")
                {
                    //认证中
                    statusPath = ":/images/circle.png";
                }
                if(devInfo[config].toString() == "0")
                {
                    //断开
                    statusPath = ":/images/radio_normal.png";
                }
                if(devInfo[config].toString() == "2")
                {
                    //已认证
                    statusPath = ":/images/radio_selected.png";
                }
            }
        }
        //        map_Devspeed.insert(dtype,speed);
        item->setText(0,str + strnum);
        QFont f = item->font(0);
        f.setPointSize(11);
        f.setFamily("Microsoft YaHei");
        item->setFont(0,f);
        item->setBackground(0,QBrush(QColor(255,255,255)));
        item->setForeground(0,QBrush(QColor(0,0,0)));
        item->setIcon(0,QIcon(statusPath));
        topItem->addChild(item);
        m_devConfigMap.insert(item,map_devinfo);
        map_devItem.insert(str + strnum,item);
    }
}

void wid_stack::updateMem()
{
    //    ui->lineEdit_mem->setText(QString("%1GB").arg(getFreeGB()));
    ui->lineEdit_mem->setText("1372.04GB");
    timerMem->stop();
}

void wid_stack::updateTem()
{
    //    double temp = m_temper->readAverageTemperature();//readCpuTempSharedMem();//QRandomGenerator::global()->bounded(70,80);
    //    ui->lineEdit_tem->setText(QString("%1").arg(temp) + "°C");
    updateUi(m_temperatureReader->readAverageTemperature());
}

bool wid_stack::launchCoreTempSilent()
{
    if(m_coreTempStarted)
    {
        return true;
    }
    QString path = QCoreApplication::applicationDirPath() + "/Core Temp.exe";
    QProcess* process = new QProcess(this);
    process->setCreateProcessArgumentsModifier([](QProcess::CreateProcessArguments* args)
    {
        args->flags |= CREATE_NO_WINDOW;
    });
    QStringList args;
    args << "-nogui";
    process->start(path,args);
    if(!process->waitForStarted(2000))
    {
        process->deleteLater();
        return false;
    }
    m_coreTempStarted = true;
    return true;
}

double wid_stack::readCpuTempSharedMem()
{
    HANDLE hMap = OpenFileMappingA(FILE_MAP_READ,FALSE,"CoreTempSharedMemory");
    if(hMap == nullptr)
    {
        DWORD errcode = GetLastError();
        qDebug()<<"mem err:"<<errcode;
        return -1;
    }
    CoreTempShareData* buf = (CoreTempShareData*)MapViewOfFile(hMap,FILE_MAP_READ,0,0,0);
    if(buf == nullptr)
    {
        DWORD errcode = GetLastError();
        qDebug()<<"mem err:"<<errcode;
        CloseHandle(hMap);
        return -1;
    }
    double maxTemp = -99;
    for(int i = 0;i < buf->CoreCnt;i++)
    {
        int t = buf->Temp[i];
        if(t > maxTemp)
        {
            maxTemp = t;
        }
    }
    UnmapViewOfFile(buf);
    CloseHandle(hMap);
    return maxTemp;
}

double wid_stack::getTemp()
{
    QProcess process;
    QStringList args;
    args << "-json" << "-nogui";
    QString path = QCoreApplication::applicationDirPath() + "/Core Temp.exe";
    process.start(path,args);

    if(!process.waitForFinished(1000))
    {
        process.kill();
        return -1;
    }
    QString output = process.readAllStandardOutput();
    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(output.toUtf8(),&err);
    if(err.error != QJsonParseError::NoError)
    {
        return -2;
    }
    QJsonObject root = doc.object();
    QJsonArray cores = root["Cores"].toArray();
    double maxTemp = 0;
    for(auto item : cores)
    {
        QJsonObject coreObjet = item.toObject();
        double temp = coreObjet["Temp"].toDouble();
        if(temp > maxTemp) maxTemp = temp;
    }
    return maxTemp;
}

void wid_stack::showInfo(QTreeWidgetItem *item)
{
    //    if(ui->lineEdit_name->text() != "")
    //    {
    ui->lineEdit_mem->setText(QString("%1GB").arg(getFreeGB()));
    ui->lineEdit_interface->setText("3");
    //    }
}

void wid_stack::InitInfo()
{
    ui->lineEdit_mem->setText(QString("%1GB").arg(getFreeGB()));
    ui->lineEdit_interface->setText(QString("%1").arg(m_devConfigMap.count()));
}


double wid_stack::getFreeGB()
{
    //    QStorageInfo info(QCoreApplication::applicationDirPath());
    //    info.refresh();
    //    if(!info.isValid() || !info.isReady())
    //    {
    //        return -1;
    //    }
    double gb = 1024.0 * 1024 * 1024;
    qint64 totalAvailableBytes = 0;
    //获取系统全部挂载卷
    QList<QStorageInfo> volList = QStorageInfo::mountedVolumes();
    for(QStorageInfo info : volList)
    {
        info.refresh();
        if(!info.isValid())
        {
            continue;
        }
        if(info.rootPath().startsWith("\\\\"))
        {
            continue;
        }
        qint64 avail = info.bytesAvailable();
        if(avail > 0)
        {
            totalAvailableBytes += avail;
        }
    }
    if(totalAvailableBytes <= 0)
    {
        return -1;
    }

    double freeGb = totalAvailableBytes / gb;
    freeGb = qRound(freeGb * 10.0) / 10.0;
    return freeGb;


    //    double total = info.bytesTotal() / gb;
    //    double avail = info.bytesAvailable() / gb;
    //    double free = info.bytesFree() / gb;
    //    double res = qRound(total * 10)/10.0;
    //    return res;
}

void wid_stack::closeEvent(QCloseEvent *event)
{
    QProcess::execute("taskkill /F /IM CoreTemp.exe");
    event->accept();
}


void wid_stack::on_pushButton_fix_clicked()
{
#if 1
    //自检
    int type = 2;
    //FC版本号
    WTAPI_TE_DSIM_DEV_VERSION* fcversion = (WTAPI_TE_DSIM_DEV_VERSION*)malloc(sizeof(WTAPI_TE_DSIM_DEV_VERSION));
    memset(fcversion,0x0,sizeof(WTAPI_TE_DSIM_DEV_VERSION));
    QStringList fchList = PM()->GetCHLs(mipriID,"FC");
    WTContrl* fcpCtrl = (WTContrl *)(PM()->GetWTCtrl(fchList.at(0)));
    UINT fcilen = 0;
    fcpCtrl->ReadData(type,(PUCHAR)fcversion,fcilen);
    //Aurora版本号
    WT_VERSION_INFO* aversion = (WT_VERSION_INFO*)malloc(sizeof(WT_VERSION_INFO));
    memset(aversion,0x0,sizeof(WT_VERSION_INFO));
    QStringList AurorahList = PM()->GetCHLs(mipriID,"Aurora");
    WTContrl* AurorapCtrl = (WTContrl *)(PM()->GetWTCtrl(AurorahList.at(0)));
    UINT ailen = AuroraID;
    int are = -1;
    are = AurorapCtrl->ReadData(type,(PUCHAR)aversion,ailen);

    QString fcver_str;
    QString aver_str;
    int count = 0;
    if(fcilen == 1)
    {
        fcver_str = QString("API版本号：%1       FPGA版本号：%2").arg(fcversion->uiAPIVer).arg(fcversion->uiFPGAVer);
        ui->lineEdit_fc->setText(fcver_str);
        count++;
    }
    else
    {
        ui->lineEdit_fc->setText("自检失败!");
    }
    if(are != -1)
    {
        QString apiver_str = QString::fromLatin1((const char*)aversion->api_version);
        QString fpgaver_str = QString::fromLatin1((const char*)aversion->fpga_version);
        aver_str = QString("API版本号：%1       FPGA版本号：%2").arg(apiver_str).arg(fpgaver_str);
        ui->lineEdit_aurora->setText(aver_str);
        count++;
    }
    else
    {
        ui->lineEdit_aurora->setText("自检失败!");
    }
    if(count == 2)
    {
        ui->lineEdit_result->setText("正常");
    }
    else
    {
        ui->lineEdit_result->setText("自检异常！");
    }

    free(fcversion);
#elif 0
    //临时用
    ui->lineEdit_fc->setText("正常");
    ui->lineEdit_aurora->setText("正常");
    ui->lineEdit_result->setText("正常");
#endif
}


void wid_stack::slot_FCOnline(int ch, int type)
{
    emit sig_updateFC(ch,type);
}

void wid_stack::slot_AuroraOnline(int ch, int type)
{
    emit sig_updateAurora(ch,type);
}

int wid_stack::InitAuroraDev()
{
    int astatus = 0;
    QList<QTreeWidgetItem*> items = m_devConfigMap.keys();
    for(int i = 0;i < items.count();i++)
    {
        if(items.at(i)->toolTip(1) == "Aurora")
        {
            //Aurora
            QStringList chList = PM()->GetCHLs(mipriID,"Aurora");
            QTreeWidgetItem* item = map_devItem.value("Aurora设备-01");
            WTContrl *pCtrl = (WTContrl *)(PM()->GetWTCtrl(chList.at(0)));//创建设备号及设置设备属性默认使用通道1的ctrl
            QVariant mCmd;
            AuroraID = pCtrl->Contrl("FindDev",mCmd);
            if(AuroraID != -1)
            {
                //连接成功
                for(int i = 0;i < chList.count();i++)
                {
                    emit sig_AuroraOnline(i,1);
                }
                isAConnect = true;
                //Aurora接口在线
                ui->widget_aurorainter->setStyleSheet(
                            "background: qradialgradient(cx:0.5,cy:0.5,radius:0.5,fx:0.5,fy:0.5,stop:0 rgba(0,255,200,255),stop:1 rgba(0,200,150,255));"
                            "box-shadow:0 0 8px rgba(0,255,200,180);"
                            "border-radius:8px;"
                            );
                ui->label_aurorastatus->setText("在线");
                astatus = 1;
                QString statusPath = ":/images/radio_selected.png";
                item->setIcon(0,QIcon(statusPath));
                map_status.insert("Aurora",1);
                QMap<QString,QString> mm = m_devConfigMap.value(item);
                mm["status"] = "2";
                m_devConfigMap.insert(item,mm);
                QString type = ui->lineEdit_type->text();
                if(type.contains("Aurora"))
                {
                    ui->lineEdit_status->setText("在线");
                }
            }
            else
            {
                //连接失败
                ui->widget_aurorainter->setStyleSheet(
                            "background-color:rgba(100,100,100,150);"
                            "box-shadow:0 0 5px rgba(100,100,100,100);"
                            "border-radius:8px;"
                            );
                ui->label_aurorastatus->setText("离线");
                QString type = ui->lineEdit_type->text();
                QTreeWidgetItem* item = map_devItem.value("Aurora设备-01");
                QString  statusPath = ":/images/radio_normal.png";
                item->setIcon(0,QIcon(statusPath));
                QMap<QString,QString> mm = m_devConfigMap.value(item);
                mm["status"] = "0";
                m_devConfigMap.insert(item,mm);
                //写回ch0的pCtrl
                if(type.contains("Aurora"))
                {
                    ui->lineEdit_status->setText("断开");
                }
                for(int i = 0;i < chList.count();i++)
                {
                    emit sig_AuroraOnline(i,0);
                }
                emit sig_updateAItem(astatus);
            }
        }
    }
    return astatus;
}

void wid_stack::checkLoad()
{
    //校验FC设备是否find
    //1.初次启动
    fcmutex.lock();
    int fcstatus = 0;
    if(isConnect != true)
    {
        QList<QTreeWidgetItem*> items = m_devConfigMap.keys();
        for(int i = 0;i < items.count();i++)
        {
            if(items.at(i)->toolTip(1) == "FC")
            {
                //FC
                QStringList chList = PM()->GetCHLs(mipriID,"FC");
                WTContrl *pCtrl = (WTContrl *)(PM()->GetWTCtrl(chList.at(0)));//创建设备句柄及设置设备属性默认使用通道1的ctrl
                MYHANDLE* myHandle = new MYHANDLE();
                QVariant mCmd = QVariant::fromValue(myHandle);
                int result = pCtrl->Contrl("FindDev",mCmd);
                map_FCHandle.clear();
                map_FCHandle.insert("FC",myHandle);
                if(result == 0)
                {
                    isConnect = true;
                    //复制到scp中
                    //                    mwid_scp->setHandle(myHandle->Handle);
                }
                else
                {
                    QString type = ui->lineEdit_type->text();
                    QTreeWidgetItem* item = map_devItem.value("FC设备-01");
                    QString  statusPath = ":/images/radio_normal.png";
                    item->setIcon(0,QIcon(statusPath));
                    QMap<QString,QString> mm = m_devConfigMap.value(item);
                    mm["status"] = "0";
                    m_devConfigMap.insert(item,mm);
                    //写回ch0的pCtrl
                    pCtrl->m_bNetWork = false;
                    if(type.contains("FC"))
                    {
                        ui->lineEdit_status->setText("断开");
                        fcstatus = 0;
                    }
                    emit sig_FCOnline(0,0);
                    emit sig_FCOnline(1,0);
                    emit sig_updateItem(fcstatus);
                }
            }
        }
    }
    else
    {
        //2.校验重连(获取连接状态)FC
        if(map_FCHandle.count() > 0)
        {
            QTreeWidgetItem* item = map_devItem.value("FC设备-01");
            QStringList chList = PM()->GetCHLs(mipriID,"FC");
            int chCount = chList.count();
            int links = 0;
            for(int chidx = 0;chidx < chList.count();chidx++)
            {
                WTContrl *pCtrl = (WTContrl *)(PM()->GetWTCtrl(chList.at(chidx)));
                MYHANDLE* myHandle = map_FCHandle.value("FC");
                myHandle->scmd = links;
                QVariant mCmd = QVariant::fromValue(myHandle);
                int result = pCtrl->Contrl("tetsConnect",mCmd);
                if(result == 0)
                {
                    emit sig_FCOnline(chidx,1);
                    links++;
                }
                else
                {
                    emit sig_FCOnline(chidx,0);
                }
            }
            WTContrl *pCtrl = (WTContrl *)(PM()->GetWTCtrl(chList.at(0)));
            if(links == chCount)
            {
                //FC接口在线
                ui->widget_fcinter->setStyleSheet(
                            "background: qradialgradient(cx:0.5,cy:0.5,radius:0.5,fx:0.5,fy:0.5,stop:0 rgba(0,255,200,255),stop:1 rgba(0,200,150,255));"
                            "box-shadow:0 0 8px rgba(0,255,200,180);"
                            "border-radius:8px;"
                            );
                ui->label_fcstatus->setText("在线");
                fcstatus = 1;
                QString statusPath = ":/images/radio_selected.png";
                item->setIcon(0,QIcon(statusPath));
                map_status.insert("FC",1);
                QMap<QString,QString> mm = m_devConfigMap.value(item);
                mm["status"] = "2";
                m_devConfigMap.insert(item,mm);
                QString type = ui->lineEdit_type->text();
                pCtrl->m_bNetWork = true;
                if(type.contains("FC"))
                {
                    if(m_FClogin)
                    {
                        ui->lineEdit_status->setText("已认证");
                    }
                    else
                    {
                        ui->lineEdit_status->setText("在线");
                    }
                    ui->btn_fclogin->setEnabled(true);
                    ui->btn_open->setText("关闭设备");
                }
                else
                {
                    ui->lineEdit_status->setText(" ");
                }
            }
            else
            {
                ui->widget_fcinter->setStyleSheet(
                            "background-color:rgba(100,100,100,150);"
                            "box-shadow:0 0 5px rgba(100,100,100,100);"
                            "border-radius:8px;"
                            );
                map_status.insert("FC",0);
                ui->label_fcstatus->setText("离线");
                fcstatus = 0;
                QString  statusPath = ":/images/radio_normal.png";
                item->setIcon(0,QIcon(statusPath));
                QMap<QString,QString> mm = m_devConfigMap.value(item);
                mm["status"] = "0";
                m_devConfigMap.insert(item,mm);
                pCtrl->m_bNetWork = false;
                QString type = ui->lineEdit_type->text();
                if(type.contains("FC"))
                {
                    ui->lineEdit_status->setText("断开");
                    ui->btn_open->setText("打开设备");
                    ui->btn_fclogin->setEnabled(false);
                }
                else
                {
                    ui->lineEdit_status->setText(" ");
                }
            }
            emit sig_updateItem(fcstatus);
        }
    }
    fcmutex.unlock();
}


void wid_stack::on_comboBox_fcspeed_currentIndexChanged(int index)
{
    int speed;
    switch (index) {
    case 0:
        speed = 4;
        break;
    case 1:
        speed = 1;
        break;
    case 2:
        speed = 2;
        break;
    default:
        break;
    }
    QStringList chList = PM()->GetCHLs(mipriID,"FC");
    WTContrl *pCtrl = (WTContrl *)(PM()->GetWTCtrl(chList.at(0)));//创建设备句柄及设置设备属性默认使用通道1的ctrl
    MYHANDLE* myHandle = map_FCHandle.value("FC");
    myHandle->scmd = speed;
    QVariant mCmd = QVariant::fromValue(myHandle);
    int result = pCtrl->Contrl("setspeed",mCmd);
    QString text;
    if(result != 0)
    {
        text  = "速率设置失败！";
    }
    else
    {
        text = "速率设置成功！";
    }
    Dlog_info* mlog = new Dlog_info(2,text);
    mlog->exec();
}

void wid_stack::on_comboBox_auroraspeed_currentIndexChanged(int index)
{
    if(index == 2)
    {
        index = 1;
    }
    int speed = index;
    QStringList chList = PM()->GetCHLs(mipriID,"Aurora");
    WTContrl *pCtrl = (WTContrl *)(PM()->GetWTCtrl(chList.at(0)));//创建设备句柄及设置设备属性默认使用通道1的ctrl
    AURORACONTROL ac;
    ac.devId = AuroraID;
    ac.scmd = speed;
    QVariant mCmd = QVariant::fromValue(ac);
    int result = pCtrl->Contrl("setspeed",mCmd);
    QString text;
    if(result != 0)
    {
        text  = "速率设置失败！";
    }
    else
    {
        text = "速率设置成功！";
    }
    Dlog_info* mlog = new Dlog_info(2,text);
    mlog->exec();
}

void wid_stack::on_comboBox_FCType_currentIndexChanged(int index)
{
    if(m_id != 0)
    {
        return;
    }
    //index----0 主 1从
    int type;
    if(index == 1)
    {
        type = 0;
    }
    if(index == 0)
    {
        type = 1;
    }

    //type----1 主 0从
    QStringList chList = PM()->GetCHLs(mipriID,"FC");
    WTContrl *pCtrl = (WTContrl *)(PM()->GetWTCtrl(chList.at(0)));//创建设备句柄及设置设备属性默认使用通道1的ctrl
    MYHANDLE* myHandle = map_FCHandle.value("FC");
    myHandle->scmd = type;
    QVariant mCmd = QVariant::fromValue(myHandle);
    int result = pCtrl->Contrl("setType",mCmd);
    QString text;
    if(result != 0)
    {
        text  = "FC同步模式设置失败！";
    }
    else
    {
        text = "FC同步模式设置成功！";
    }
    Dlog_info* mlog = new Dlog_info(2,text);
    mlog->exec();
}

void wid_stack::on_comboBox_AuroraType_currentIndexChanged(int index)
{
    //index----1主 0从
    if(m_id != 0)
    {
        return;
    }
    //type----0 主 1从
    QStringList chList = PM()->GetCHLs(mipriID,"Aurora");
    WTContrl *pCtrl = (WTContrl *)(PM()->GetWTCtrl(chList.at(0)));//创建设备句柄及设置设备属性默认使用通道1的ctrl
    AURORACONTROL com;
    com.scmd = index;
    com.devId = AuroraID;
    QVariant mCmd = QVariant::fromValue(com);
    int result = pCtrl->Contrl("setType",mCmd);
    QString text;
    if(result != 0)
    {
        text  = "Aurora同步模式设置失败！";
    }
    else
    {
        text = "Aurora同步模式设置成功！";
    }
    Dlog_info* mlog = new Dlog_info(2,text);
    mlog->exec();
}

void wid_stack::slot_scpShow(QList<QString> list)
{
    emit sig_scpShow(list);
}

void wid_stack::updateDBTable()
{
    mwid_datacontrol->updateTable();
}

void wid_stack::slot_updateICDtable()
{
    //接收rul发送的同步ICD规则数据列表
    emit sig_ICDShow();
}

void wid_stack::slot_ICDShow()
{
    //下发到view
    emit sig_ICDTShow();
}

void wid_stack::slot_scpDs(QList<QString> list)
{
    //从datacontrol接收转发到scp
    emit sig_scpData(list);
}

void wid_stack::on_btn_open_clicked()
{
    if(ui->lineEdit_name->text().isEmpty())
    {
        return;
    }
    //管理是否打开设备
    if(ui->btn_open->text() == "打开设备")
    {
        //打开板卡驱动
        if(ui->lineEdit_name->text() == "FC设备-01")
        {
            timerStatus->start(1000);//FC
            fcStart = true;
            isConnect = false;
            ui->btn_open->setText("关闭设备");
            ui->btn_fclogin->setEnabled(true);
        }
        if(ui->lineEdit_name->text() == "Aurora设备-01")
        {
#if 1
            InitAuroraDev();
            //Aurora初始化成功
            ui->btn_open->setText("关闭设备");
            //启动定时器监测当前选中的item是否为Aurora
            timerAStatus->start(1000);
#endif
        }
        if(ui->lineEdit_name->text() == "千兆网设备-01")
        {
            ui->widget_netinter->setStyleSheet(
                        "background: qradialgradient(cx:0.5,cy:0.5,radius:0.5,fx:0.5,fy:0.5,stop:0 rgba(0,255,200,255),stop:1 rgba(0,200,150,255));"
                        "box-shadow:0 0 8px rgba(0,255,200,180);"
                        "border-radius:8px;"
                        );
            ui->label_netstatus->setText("在线");
            QTreeWidgetItem* item = map_devItem.value("千兆网设备-01");
            QString statusPath = ":/images/radio_selected.png";
            item->setIcon(0,QIcon(statusPath));
            netStart = true;
            ui->btn_open->setText("关闭设备");
        }
    }
    else if(ui->btn_open->text() == "关闭设备")
    {
        //关闭板卡
        if(ui->lineEdit_name->text() == "FC设备-01")
        {
            QStringList chList = PM()->GetCHLs(mipriID,"FC");
            WTContrl *pCtrl = (WTContrl *)(PM()->GetWTCtrl(chList.at(0)));//创建设备句柄及设置设备属性默认使用通道1的ctrl
            MYHANDLE* myHandle = map_FCHandle.value("FC");
            QVariant mCmd = QVariant::fromValue(myHandle);
            pCtrl->Contrl("CloseDev",mCmd);
            fcmutex.lock();
            map_FCHandle.clear();
            map_FCHandle.insert("FC",myHandle);
            fcStart = false;
            fcmutex.unlock();
            ui->btn_open->setText("打开设备");
            ui->btn_fclogin->setEnabled(false);
            m_FClogin = false;
        }
#if 1
        if(ui->lineEdit_name->text() == "Aurora设备-01")
        {
            //
            QStringList chList = PM()->GetCHLs(mipriID,"Aurora");
            WTContrl *pCtrl = (WTContrl *)(PM()->GetWTCtrl(chList.at(0)));//创建设备句柄及设置设备属性默认使用通道1的ctrl
            QVariant mCmd = AuroraID;
            pCtrl->Contrl("CloseDev",mCmd);
            auroaStart = false;
//            AuroraID = -1;
            ui->btn_open->setText("打开设备");
        }
        if(ui->lineEdit_name->text() == "千兆网设备-01")
        {
            ui->widget_netinter->setStyleSheet(
                        "background-color:rgba(100,100,100,150);"
                        "box-shadow:0 0 5px rgba(100,100,100,100);"
                        "border-radius:8px;"
                        );
            ui->label_netstatus->setText("离线");
            QTreeWidgetItem* item = map_devItem.value("千兆网设备-01");
            QString statusPath = ":/images/radio_normal.png";
            item->setIcon(0,QIcon(statusPath));
            netStart = false;
            ui->btn_open->setText("打开设备");
        }
#endif
    }
}

void wid_stack::updateAuroraStatus()
{
#if 1
    int astatus = 0;
    if(isAConnect != true)
    {//首次启动
        QList<QTreeWidgetItem*> items = m_devConfigMap.keys();
        for(int i = 0;i < items.count();i++)
        {
            if(items.at(i)->toolTip(1) == "Aurora")
            {
                //Aurora
                QStringList chList = PM()->GetCHLs(mipriID,"Aurora");
                WTContrl *pCtrl = (WTContrl *)(PM()->GetWTCtrl(chList.at(0)));//创建设备句柄及设置设备属性默认使用通道1的ctrl
                MYHANDLE* myHandle = new MYHANDLE();
                QVariant mCmd = QVariant::fromValue(myHandle);
                int result = pCtrl->Contrl("FindDev",mCmd);
                if(result != -1)
                {
                    isAConnect = true;
                    AuroraID = result;
                }
                else
                {
                    //连接失败
                    ui->widget_aurorainter->setStyleSheet(
                                "background-color:rgba(100,100,100,150);"
                                "box-shadow:0 0 5px rgba(100,100,100,100);"
                                "border-radius:8px;"
                                );
                    ui->label_aurorastatus->setText("离线");
                    QString type = ui->lineEdit_type->text();
                    QTreeWidgetItem* item = map_devItem.value("Aurora设备-01");
                    QString  statusPath = ":/images/radio_normal.png";
                    item->setIcon(0,QIcon(statusPath));
                    QMap<QString,QString> mm = m_devConfigMap.value(item);
                    mm["status"] = "0";
                    m_devConfigMap.insert(item,mm);
                    //写回ch0的pCtrl
                    if(type.contains("Aurora"))
                    {
                        ui->lineEdit_status->setText("断开");
                    }
                    for(int i = 0;i < chList.count();i++)
                    {
                        emit sig_AuroraOnline(i,0);
                    }
                    emit sig_updateAItem(astatus);
                }
            }
        }
    }
    else
    {
        //定时查询Aurora在线状态
        if(AuroraID != -1)
        {
            QTreeWidgetItem* item = map_devItem.value("Aurora设备-01");
            QStringList chList = PM()->GetCHLs(mipriID,"Aurora");
            int chCount = chList.count();
            int links = 0;
            for(int chidx = 0;chidx < chList.count();chidx++)
            {
                WTContrl *pCtrl = (WTContrl *)(PM()->GetWTCtrl(chList.at(chidx)));
                AURORACONTROL ac;
//                bool status = false;
                ac.devId = AuroraID;
//                ac.scmd =status;
                QVariant mCmd = QVariant::fromValue(ac);
                int result = pCtrl->Contrl("tetsConnect",mCmd);
                if(result == 0)
                {
                    emit sig_AuroraOnline(chidx,1);
                    links++;
                }
                else
                {
                    emit sig_AuroraOnline(chidx,0);
                }
            }
            if(links == chCount)
            {
                isAConnect = true;
                //Aurora接口在线
                ui->widget_aurorainter->setStyleSheet(
                            "background: qradialgradient(cx:0.5,cy:0.5,radius:0.5,fx:0.5,fy:0.5,stop:0 rgba(0,255,200,255),stop:1 rgba(0,200,150,255));"
                            "box-shadow:0 0 8px rgba(0,255,200,180);"
                            "border-radius:8px;"
                            );
                ui->label_aurorastatus->setText("在线");
                astatus = 1;
                QString statusPath = ":/images/radio_selected.png";
                item->setIcon(0,QIcon(statusPath));
                map_status.insert("Aurora",1);
                QMap<QString,QString> mm = m_devConfigMap.value(item);
                mm["status"] = "2";
                m_devConfigMap.insert(item,mm);
                QString type = ui->lineEdit_type->text();
                if(type.contains("Aurora"))
                {
                    ui->lineEdit_status->setText("在线");
                    ui->btn_open->setText("关闭设备");
                }
            }
            else
            {
                    //离线
                //连接失败
                ui->widget_aurorainter->setStyleSheet(
                            "background-color:rgba(100,100,100,150);"
                            "box-shadow:0 0 5px rgba(100,100,100,100);"
                            "border-radius:8px;"
                            );
                ui->label_aurorastatus->setText("离线");
                QString type = ui->lineEdit_type->text();
                QTreeWidgetItem* item = map_devItem.value("Aurora设备-01");
                QString  statusPath = ":/images/radio_normal.png";
                item->setIcon(0,QIcon(statusPath));
                QMap<QString,QString> mm = m_devConfigMap.value(item);
                mm["status"] = "0";
                m_devConfigMap.insert(item,mm);
                //写回ch0的pCtrl
                if(type.contains("Aurora"))
                {
                    ui->lineEdit_status->setText("断开");
                    ui->btn_open->setText("打开设备");
                }
                emit sig_updateAItem(astatus);
            }
        }
    }
#endif
#if 0
    QTreeWidgetItem* item = map_devItem.value("Aurora设备-01");
    QStringList chList = PM()->GetCHLs(mipriID,"Aurora");
    if(AuroraID != -1)
    {
        //在线
        ui->widget_aurorainter->setStyleSheet(
                    "background: qradialgradient(cx:0.5,cy:0.5,radius:0.5,fx:0.5,fy:0.5,stop:0 rgba(0,255,200,255),stop:1 rgba(0,200,150,255));"
                    "box-shadow:0 0 8px rgba(0,255,200,180);"
                    "border-radius:8px;"
                    );
        ui->label_aurorastatus->setText("在线");
        QString type = ui->lineEdit_type->text();
        if(type.contains("Aurora"))
        {
            ui->lineEdit_status->setText("在线");
        }
        QString statusPath = ":/images/radio_selected.png";
        item->setIcon(0,QIcon(statusPath));
        QMap<QString,QString> mm = m_devConfigMap.value(item);
        mm["status"] = "2";
        m_devConfigMap.insert(item,mm);
        for(int i = 0;i < chList.count();i++)
        {
            emit sig_AuroraOnline(i,1);
        }
        emit sig_updateAItem(1);
    }
    else
    {
        //离线
        ui->widget_aurorainter->setStyleSheet(
                    "background-color:rgba(100,100,100,150);"
                    "box-shadow:0 0 5px rgba(100,100,100,100);"
                    "border-radius:8px;"
                    );
        ui->label_aurorastatus->setText("离线");
        QString  statusPath = ":/images/radio_normal.png";
        item->setIcon(0,QIcon(statusPath));
        QMap<QString,QString> mm = m_devConfigMap.value(item);
        mm["status"] = "0";
        m_devConfigMap.insert(item,mm);
        QString type = ui->lineEdit_type->text();
        if(type.contains("Aurora"))
        {
            ui->lineEdit_status->setText("断开");
        }
        for(int i = 0;i < chList.count();i++)
        {
            emit sig_AuroraOnline(i,0);
        }
        emit sig_updateAItem(0);
    }
#endif
}

void wid_stack::FCclose()
{
    if(ui->lineEdit_name->text() == "FC设备-01")
    {
        QStringList chList = PM()->GetCHLs(mipriID,"FC");
        WTContrl *pCtrl = (WTContrl *)(PM()->GetWTCtrl(chList.at(0)));//创建设备句柄及设置设备属性默认使用通道1的ctrl
        QTreeWidgetItem* item = map_devItem.value("FC设备-01");
        ui->label_fcstatus->setText("离线");
        int  fcstatus = 0;
        QString  statusPath = ":/images/radio_normal.png";
        item->setIcon(0,QIcon(statusPath));
        QMap<QString,QString> mm = m_devConfigMap.value(item);
        mm["status"] = "0";
        m_devConfigMap.insert(item,mm);
        pCtrl->m_bNetWork = false;
        QString type = ui->lineEdit_type->text();
        if(type.contains("FC"))
        {
            ui->lineEdit_status->setText("断开");
        }
        else
        {
            ui->lineEdit_status->setText(" ");
        }
        emit sig_updateItem(fcstatus);
    }
}

void wid_stack::FClogin()
{
    //登录认证-fc
    QStringList chList = PM()->GetCHLs(mipriID,"FC");
    WTContrl *pCtrl = (WTContrl *)(PM()->GetWTCtrl(chList.at(0)));//创建设备句柄及设置设备属性默认使用通道1的ctrl
    QVariant mCmd;
    int result = pCtrl->Contrl("loginDev",mCmd);
    if(result == 0)
    {
        m_FClogin = true;
    }
}

/**
 * @brief wid_stack::on_btn_open_2_clicked
 * 入网认证
 */
void wid_stack::on_btn_open_2_clicked()
{
#if 0
    ELSForm cSendElsDlg;
    cSendElsDlg.exec();
#endif
    //启动登录定时器，定期获取状态
    timerLogin->start(1000);
}
/**
 * @brief wid_stack::on_btn_open_3_clicked
 * 网络管理
 */
void wid_stack::on_btn_open_3_clicked()
{
//    FCNETForm cFCNETForm;
//    cFCNETForm.exec();
}

void wid_stack::on_btn_fclogin_clicked()
{
    FClogin();
}

void wid_stack::on_comboBox_loopmode_currentIndexChanged(int index)
{
    //修改回环模式
    QStringList chList = PM()->GetCHLs(mipriID,"Aurora");
    WTContrl *pCtrl = (WTContrl *)(PM()->GetWTCtrl(chList.at(0)));//创建设备句柄及设置设备属性默认使用通道1的ctrl
    AURORACONTROL ac;
    ac.devId = AuroraID;
    ac.scmd = index;
    QVariant mCmd = QVariant::fromValue(ac);
    int result = pCtrl->Contrl("setLoop",mCmd);
    QString text;
    if(result != 0)
    {
        text  = "回环设置失败！";
    }
    else
    {
        text = "回环设置成功！";
    }
    Dlog_info* mlog = new Dlog_info(2,text);
    mlog->exec();
}

void wid_stack::updateUi(const TemperatureReading &reading)
{
    // 读取失败时保留占位值，并显示具体原因。
    if (!reading.valid) {
        ui->lineEdit_tem->setText(QStringLiteral("--.- \u00B0C"));
        return;
    }

    // 读取成功时显示所有 CPU 核心的平均摄氏温度。
    ui->lineEdit_tem->setText(
                QString("%1 \u00B0C").arg(reading.celsius, 0, 'f', 1));
    setTemperatureColor(reading.celsius);
}

void wid_stack::setTemperatureColor(double celsius)
{
    if (celsius >= 85.0) {
        ui->lineEdit_tem->setStyleSheet(R"(
                                        background-color: rgb(255,255,255);
                                        color: rgb(180,35,24);
                                        font: 75 14pt "微软雅黑";
                                        )");

    } else if (celsius >= 70.0) {
        ui->lineEdit_tem->setStyleSheet(R"(
                                        background-color: rgb(255,255,255);
                                        color: rgb(181,71,8);
                                        font: 75 14pt "微软雅黑";
                                        )");
    } else {
        ui->lineEdit_tem->setStyleSheet(R"(
                                        background-color: rgb(255,255,255);
                                        color: rgb(0,255,0);
                                        font: 75 14pt "微软雅黑";
                                        )");
    }
}
