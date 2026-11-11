#include "dlog_devadd.h"
#include "ui_dlog_devadd.h"

dlog_devadd::dlog_devadd(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::dlog_devadd)
{
    ui->setupUi(this);
    setWindowFlags(Qt::Window | Qt::FramelessWindowHint);
    setMouseTracking(true);
    connect(ui->btnClose, &QPushButton::clicked, this, &dlog_devadd::close);
    ui->stackedWidget->setCurrentIndex(0);
}

dlog_devadd::~dlog_devadd()
{
    delete ui;
}

void dlog_devadd::on_comboBox_currentIndexChanged(int index)
{
    ui->stackedWidget->setCurrentIndex(index);
}


void dlog_devadd::mousePressEvent(QMouseEvent *event)
{
    if(event->button() == Qt::LeftButton)
    {
        m_bDragging = true;
        m_dragPosition = event->globalPos() - frameGeometry().topLeft();
        event->accept();
    }
    QWidget::mousePressEvent(event);
}

void dlog_devadd::mouseDoubleClickEvent(QMouseEvent *event)
{
    if(event->button() == Qt::LeftButton)
    {
    }
    QWidget::mouseDoubleClickEvent(event);
}

void dlog_devadd::mouseMoveEvent(QMouseEvent *event)
{
    if(m_bDragging && (event->buttons() & Qt::LeftButton))
    {
       move(event->globalPos() - m_dragPosition);
       event->accept();
    }
    QWidget::mouseMoveEvent(event);
}

void dlog_devadd::mouseReleaseEvent(QMouseEvent *event)
{
    if(event->button() == Qt::LeftButton)
    {
        m_bDragging = false;
        event->accept();
    }
    QWidget::mouseReleaseEvent(event);
}

void dlog_devadd::on_pushButton_2_clicked()
{
    close();
}

void dlog_devadd::on_pushButton_clicked()
{
    //确认添加该设备
    QString devName = ui->lineEdit_name->text();
    int typeindex = ui->comboBox->currentIndex();//FC:0 千兆网:1 Aurora:2
    DEVINFO info;
    info.type = typeindex;
    info.devname = devName;
    int idx = devName.indexOf("-");
    if(idx == -1)
    {
        //格式错误
        Dlog_info* mlog = new Dlog_info(2,"设备名格式错误！");
        mlog->exec();
        return;
    }
    info.id = devName.mid(idx+1).toInt();
    if(typeindex != 0)
    {
        //以太网/Aurora
    }
    emit sig_addDev(info);
    close();
}
