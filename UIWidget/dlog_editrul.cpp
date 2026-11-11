#include "dlog_editrul.h"
#include "ui_dlog_editrul.h"

dlog_editRul::dlog_editRul(QString name,QVector<ICDDATA> vec_ICD,QWidget *parent) :
    QDialog(parent),
    ui(new Ui::dlog_editRul)
{
    ui->setupUi(this);
    InitUI();
    InitTable(name,vec_ICD);
}

dlog_editRul::~dlog_editRul()
{
    delete ui;
}

void dlog_editRul::InitUI()
{
    setWindowFlags(Qt::Window | Qt::FramelessWindowHint);
    setMouseTracking(true);
    ui->pbtn_re->hide();
    ui->label_2->hide();
    ui->comboBox->hide();
    ui->lineEdit_name->setReadOnly(true);
    connect(ui->btnClose, &QPushButton::clicked, this, &dlog_editRul::close);
    ui->tableWidget->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    //    ui->FC_tableWidget->setEditTriggers(QAbstractItemView::NoEditTriggers);
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
}

void dlog_editRul::mousePressEvent(QMouseEvent *event)
{
    if(event->button() == Qt::LeftButton)
    {
        m_bDragging = true;
        m_dragPosition = event->globalPos() - frameGeometry().topLeft();
        event->accept();
    }
    QWidget::mousePressEvent(event);
}

void dlog_editRul::mouseDoubleClickEvent(QMouseEvent *event)
{
    if(event->button() == Qt::LeftButton)
    {
    }
    QWidget::mouseDoubleClickEvent(event);
}

void dlog_editRul::mouseMoveEvent(QMouseEvent *event)
{
    if(m_bDragging && (event->buttons() & Qt::LeftButton))
    {
        move(event->globalPos() - m_dragPosition);
        event->accept();
    }
    QWidget::mouseMoveEvent(event);
}

void dlog_editRul::mouseReleaseEvent(QMouseEvent *event)
{
    if(event->button() == Qt::LeftButton)
    {
        m_bDragging = false;
        event->accept();
    }
    QWidget::mouseReleaseEvent(event);
}


void dlog_editRul::on_pushButton_clicked()
{
    //保存 修改规则
    QString name = ui->lineEdit_name->text();
    QString TableName = QString("TABLE_ICD_%1").arg(name);
    //保存ICD数据内容到数据库中
    QSqlQuery query;
    QString strsql;
    strsql = QString("DELETE FROM %1;").arg(TableName);
    if(!query.exec(strsql))
    {
        return;
    }
    Table = ui->tableWidget;
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
            Dlog_info* mflog = new Dlog_info(2,"数据修改失败！");
            mflog->exec();
            return;
        }
    }
    close();
}

void dlog_editRul::on_pushButton_2_clicked()
{
    close();
}

void dlog_editRul::InitTable(QString name, QVector<ICDDATA> vec_ICD)
{
    ui->lineEdit_name->setText(name);
    Table = ui->tableWidget;
    for(int i = 0;i < vec_ICD.count();i++)
    {
        int row = Table->rowCount();
        Table->insertRow(row);
        Table->setRowHeight(row,35);
        //字段名
        QTableWidgetItem* nameItem = new QTableWidgetItem(vec_ICD.at(i).name);
        nameItem->setTextAlignment(Qt::AlignCenter);
        nameItem->setForeground(QBrush(QColor(0,0,0)));
        Table->setItem(row,0,nameItem);
        //类型
        QComboBox* typeCom = new QComboBox();
        typeCom->addItems(QStringList() << "Byte" << "Bit");
        if(vec_ICD.at(i).type == "Bit")
        {
            typeCom->setCurrentIndex(1);
        }
        else {
            typeCom->setCurrentIndex(0);
        }
        Table->setCellWidget(row,1,typeCom);
        //长度
        QTableWidgetItem* lenItem = new QTableWidgetItem(QString("%1").arg(vec_ICD.at(i).len));
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
        });
        Table->setCellWidget(row,3,delBtn);
    }
}

void dlog_editRul::on_pbtn_add_clicked()
{
    addItem(0);
}

void dlog_editRul::addItem(int idx)
{
    Table = ui->tableWidget;
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
    typeCom->addItems(QStringList() << "Byte" << "Bit");
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
