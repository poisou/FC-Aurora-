#include "dlog_export.h"
#include "ui_dlog_export.h"

dlog_export::dlog_export(QStringList list_rul,QWidget *parent) :
    QDialog(parent),
    ui(new Ui::dlog_export)
{
    ui->setupUi(this);
    ui->comboBox->addItems(list_rul);
    setWindowFlags(Qt::Window | Qt::FramelessWindowHint);
    setMouseTracking(true);
    connect(ui->btnClose, &QPushButton::clicked, this, &dlog_export::close);
}

dlog_export::~dlog_export()
{
    delete ui;
}


void dlog_export::mousePressEvent(QMouseEvent *event)
{
    if(event->button() == Qt::LeftButton)
    {
        m_bDragging = true;
        m_dragPosition = event->globalPos() - frameGeometry().topLeft();
        event->accept();
    }
    QWidget::mousePressEvent(event);
}

void dlog_export::mouseDoubleClickEvent(QMouseEvent *event)
{
    if(event->button() == Qt::LeftButton)
    {
    }
    QWidget::mouseDoubleClickEvent(event);
}

void dlog_export::mouseMoveEvent(QMouseEvent *event)
{
    if(m_bDragging && (event->buttons() & Qt::LeftButton))
    {
       move(event->globalPos() - m_dragPosition);
       event->accept();
    }
    QWidget::mouseMoveEvent(event);
}

void dlog_export::mouseReleaseEvent(QMouseEvent *event)
{
    if(event->button() == Qt::LeftButton)
    {
        m_bDragging = false;
        event->accept();
    }
    QWidget::mouseReleaseEvent(event);
}

void dlog_export::on_pushButton_2_clicked()
{
    close();
}

void dlog_export::on_pushButton_clicked()
{
    //确定导出的路径
    QString dirpath = QFileDialog::getExistingDirectory(
                this,
                "选择文件夹",
                QDir::homePath(),
                QFileDialog::ShowDirsOnly |
                QFileDialog::DontResolveSymlinks
                );
    if(dirpath != "")
    {
        //获取导出的规则
        int index = ui->comboBox->currentIndex();
        emit sig_selectRul(dirpath,index);
    }
    close();
}
