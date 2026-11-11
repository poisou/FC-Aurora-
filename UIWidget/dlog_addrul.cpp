#include "dlog_addrul.h"
#include "ui_dlog_addrul.h"

dlog_addrul::dlog_addrul(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::dlog_addrul)
{
    ui->setupUi(this);
    InitUI();
    InitSql();
}

dlog_addrul::~dlog_addrul()
{
    delete ui;
}

void dlog_addrul::InitUI()
{
    setWindowFlags(Qt::Window | Qt::FramelessWindowHint);
    setMouseTracking(true);
    connect(ui->btnClose, &QPushButton::clicked, this, &dlog_addrul::close);
    ui->stackedWidget->setCurrentIndex(0);
    //设置表格信息
    ui->pbtn_fcre->hide();
    ui->pbtn_netre->hide();
    ui->pbtn_aurorare->hide();
    //FC
    ui->FC_tableWidget->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    //    ui->FC_tableWidget->setEditTriggers(QAbstractItemView::NoEditTriggers);
    ui->FC_tableWidget->verticalHeader()->setVisible(false);
    ui->FC_tableWidget->verticalHeader()->setDefaultSectionSize(50);
    ui->FC_tableWidget->horizontalHeader()->setStyleSheet(R"(
                                                          QHeaderView::section{
                                                          background-color:rgb(255,255,255);
                                                          color:rgb(0,0,0);
                                                          font: 75 10pt "微软雅黑";
                                                          border-top:1px solid rgb(70,90,120);
                                                          border-bottom:1px solid rgb(70,90,120);
                                                          border-radius:1px;
                                                          }
                                                          )");
    //NET
    ui->NET_tableWidget->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    //    ui->NET_tableWidget->setEditTriggers(QAbstractItemView::NoEditTriggers);
    ui->NET_tableWidget->verticalHeader()->setVisible(false);
    ui->NET_tableWidget->verticalHeader()->setDefaultSectionSize(50);
    ui->NET_tableWidget->horizontalHeader()->setStyleSheet(R"(
                                                           QHeaderView::section{
                                                           background-color:rgb(255,255,255);
                                                           color:rgb(0,0,0);
                                                           font: 75 10pt "微软雅黑";
                                                           border-top:1px solid rgb(70,90,120);
                                                           border-bottom:1px solid rgb(70,90,120);
                                                           border-radius:1px;
                                                           }
                                                           )");
    //Aurora
    ui->Aurora_tableWidget->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    //    ui->Aurora_tableWidget->setEditTriggers(QAbstractItemView::NoEditTriggers);
    ui->Aurora_tableWidget->verticalHeader()->setVisible(false);
    ui->Aurora_tableWidget->verticalHeader()->setDefaultSectionSize(50);
    ui->Aurora_tableWidget->horizontalHeader()->setStyleSheet(R"(
                                                              QHeaderView::section{
                                                              background-color:rgb(255,255,255);
                                                              color:rgb(0,0,0);
                                                              font: 75 10pt "微软雅黑";
                                                              border-top:1px solid rgb(70,90,120);
                                                              border-bottom:1px solid rgb(70,90,120);
                                                              border-radius:1px;
                                                              }
                                                              )");
}


void dlog_addrul::on_pushButton_clicked()
{
    //添加规则
    QString str_name = ui->lineEdit_name->text();
    int idx = ui->comboBox->currentIndex();
    QString type;
    switch (idx) {
    case 0:
        //FC
        type = "FC";
        break;
    case 1:
        //aurora
        type = "Aurora";
        break;
    case 2:
        //net
        type = "以太网";
        break;
    default:
        break;
    }
    //存储到ICD路径下
    QString FilePath = QApplication::applicationDirPath() + "/prj/WTBTS/ICD";
    FilePath += QString("/") + str_name + ".json";
    saveICD(FilePath,str_name,type);
    emit sig_addrul();
    close();
}

void dlog_addrul::saveICD(QString path,QString name,QString type)
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
    int row = Table->rowCount();
    for(int i = 0; i < row;i++)
    {
        QString name = Table->item(i,0)->text();
        QComboBox* com = qobject_cast<QComboBox*>(Table->cellWidget(i,1));
        QString type = com->currentText();
        int len = Table->item(i,2)->text().toInt();
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

void dlog_addrul::InitSql()
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

void dlog_addrul::on_pushButton_2_clicked()
{
    close();
}

void dlog_addrul::on_comboBox_currentIndexChanged(int index)
{
    ui->stackedWidget->setCurrentIndex(index);
}

void dlog_addrul::on_pbtn_fcadd_clicked()
{
    //添加fc规则列表内容
    addItem(0);
}

void dlog_addrul::on_pbtn_netadd_clicked()
{
    //添加net规则列表内容
    addItem(2);
}

void dlog_addrul::on_pbtn_auroraadd_clicked()
{
    //添加aurora规则列表内容
    addItem(1);
}

void dlog_addrul::addItem(int idx)
{
    if(idx == 0)
    {
        //FC
        Table = ui->FC_tableWidget;
    }
    if(idx == 1)
    {
        //Aurora
        Table = ui->Aurora_tableWidget;
    }
    if(idx == 2)
    {
        //net
        Table = ui->NET_tableWidget;
    }
    int row = Table->rowCount();
    Table->insertRow(row);
    Table->setRowHeight(row,35);
    //字段名
    QTableWidgetItem* nameItem = new QTableWidgetItem("NULL");
    nameItem->setTextAlignment(Qt::AlignCenter);
    nameItem->setForeground(QBrush(QColor(0,0,0)));
    Table->setItem(row,0,nameItem);
    //类型
    QComboBox* typeCom = new QComboBox();
    typeCom->addItems(QStringList() << "Bit" << "Byte");
    typeCom->setCurrentIndex(0);
    Table->setCellWidget(row,1,typeCom);
    //长度
    QTableWidgetItem* lenItem = new QTableWidgetItem("0");
    lenItem->setTextAlignment(Qt::AlignCenter);
    lenItem->setForeground(QBrush(QColor(0,0,0)));
    Table->setItem(row,2,lenItem);
    //操作
    QPushButton* delBtn = new QPushButton("删除");
    delBtn->setStyleSheet(
                "QPushButton{"
                "border:1px solid rgb(70,90,120);"
                "color: rgb(255, 255, 255);"
                "border-radius:3px;"
                "font: 75 11pt '微软雅黑';"
                "background-color: rgb(255,0,0);"
                "}"
                "QPushButton:hover{"
                "color: rgb(0,0,0);"
                "background-color: rgb(227, 236, 255);"
                "};");
    connect(delBtn,&QPushButton::clicked,this,[this]()
    {
        QPushButton* btn = qobject_cast<QPushButton*>(sender());
        if(!btn)
        {
            return;
        }
        QPoint btnpos = btn->mapToGlobal(QPoint(0,0));
        QPoint tablesPos = Table->mapFromGlobal(btnpos);
        QModelIndex index = Table->indexAt(tablesPos);
        if(index.isValid())
        {
            Table->removeRow(index.row());
        }
//            Table->removeRow(idx);
    });
    Table->setCellWidget(row,3,delBtn);
}

void dlog_addrul::mousePressEvent(QMouseEvent *event)
{
    if(event->button() == Qt::LeftButton)
    {
        m_bDragging = true;
        m_dragPosition = event->globalPos() - frameGeometry().topLeft();
        event->accept();
    }
    QWidget::mousePressEvent(event);
}

void dlog_addrul::mouseDoubleClickEvent(QMouseEvent *event)
{
    if(event->button() == Qt::LeftButton)
    {
    }
    QWidget::mouseDoubleClickEvent(event);
}

void dlog_addrul::mouseMoveEvent(QMouseEvent *event)
{
    if(m_bDragging && (event->buttons() & Qt::LeftButton))
    {
        move(event->globalPos() - m_dragPosition);
        event->accept();
    }
    QWidget::mouseMoveEvent(event);
}

void dlog_addrul::mouseReleaseEvent(QMouseEvent *event)
{
    if(event->button() == Qt::LeftButton)
    {
        m_bDragging = false;
        event->accept();
    }
    QWidget::mouseReleaseEvent(event);
}
