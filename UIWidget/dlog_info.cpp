#include "dlog_info.h"
#include "ui_dlog_info.h"

Dlog_info::Dlog_info(int id,QString text,QWidget *parent) :
    QDialog(parent),
    ui(new Ui::Dlog_info)
{
    ui->setupUi(this);
    ui->lbTitle->setText(text);
    setWindowFlags(Qt::Window | Qt::FramelessWindowHint);
    setMouseTracking(true);
    connect(ui->btnClose, &QPushButton::clicked, this, &Dlog_info::close);
    mid = id;
    if(id == 0)
    {
        ui->lineEdit_name->hide();
        ui->label->hide();
    }
    if(id == 2)
    {
        ui->lineEdit_name->hide();
        ui->label->hide();
        ui->pushButton_2->setVisible(false);
    }
}

Dlog_info::~Dlog_info()
{
    delete ui;
}


void Dlog_info::mousePressEvent(QMouseEvent *event)
{
    if(event->button() == Qt::LeftButton)
    {
        m_bDragging = true;
        m_dragPosition = event->globalPos() - frameGeometry().topLeft();
        event->accept();
    }
    QWidget::mousePressEvent(event);
}

void Dlog_info::mouseDoubleClickEvent(QMouseEvent *event)
{
    if(event->button() == Qt::LeftButton)
    {
    }
    QWidget::mouseDoubleClickEvent(event);
}

void Dlog_info::mouseMoveEvent(QMouseEvent *event)
{
    if(m_bDragging && (event->buttons() & Qt::LeftButton))
    {
       move(event->globalPos() - m_dragPosition);
       event->accept();
    }
    QWidget::mouseMoveEvent(event);
}

void Dlog_info::mouseReleaseEvent(QMouseEvent *event)
{
    if(event->button() == Qt::LeftButton)
    {
        m_bDragging = false;
        event->accept();
    }
    QWidget::mouseReleaseEvent(event);
}

void Dlog_info::on_pushButton_clicked()
{
    //确定
    if(mid == 0)
    {
        //删除
        emit sig_delItem();
    }
    if(mid == 1)
    {
        //复制
        if(ui->lineEdit_name->text() == "")
        {
            return;
        }
        emit sig_copyItem(ui->lineEdit_name->text());
    }
    close();
}

void Dlog_info::on_pushButton_2_clicked()
{
    close();
}
