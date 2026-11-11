#include "wid_FCdataInfo.h"
#include "ui_wid_FCdatainfo.h"

wid_FCdataInfo::wid_FCdataInfo(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::wid_FCdataInfo)
{
    ui->setupUi(this);
    Init();
}

wid_FCdataInfo::~wid_FCdataInfo()
{
    delete ui;
}

void wid_FCdataInfo::Init()
{
//    ui->widget_31->hide();
    ui->tableWidget_FCdata->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    ui->tableWidget_FCdata->setEditTriggers(QAbstractItemView::DoubleClicked | QAbstractItemView::EditKeyPressed);
    connect(ui->tableWidget_FCdata,&QTableWidget::itemChanged,this,&wid_FCdataInfo::onItemchange);
    //设置FC帧头限制
    setupValidtors(ui->lineEdit_D_ID,6);
    setupValidtors(ui->lineEdit_R_CTL,2);
    setupValidtors(ui->lineEdit_S_ID,6);
    setupValidtors(ui->lineEdit_CS_CTL,2);
    setupValidtors(ui->lineEdit_F_CTL,6);
    setupValidtors(ui->lineEdit_TYPE,2);
    setupValidtors(ui->lineEdit_SEQ_CNT,4);
    setupValidtors(ui->lineEdit_DF_CTL,2);
    setupValidtors(ui->lineEdit_SEQ_ID,2);
    setupValidtors(ui->lineEdit_RX_ID,4);
    setupValidtors(ui->lineEdit_OX_ID,4);
    setupValidtors(ui->lineEdit_Parameter,8);
    setupValidtors(ui->lineEdit_MSG_ID,8);
    setupValidtors(ui->lineEdit_RES0,8);
    setupValidtors(ui->lineEdit_RES1,8);
    setupValidtors(ui->lineEdit_MSG_LEN,6);
    setupValidtors(ui->lineEdit_MSG_PRIORITY,2);
    setupValidtors(ui->lineEdit_MSG_L,1,true);

    bindSpandLineedit(ui->spinBox_maxlen,ui->lineEdit_MSG_LEN,6);

//    ui->spinBox_start->setEnabled(false);
//    ui->spinBox_step->setEnabled(false);
}

WTAPI_TE_DSIM_SEND_HEAD* wid_FCdataInfo::getWTDSIMHeader()
{
    WTAPI_TE_DSIM_SEND_HEAD* dhead = (WTAPI_TE_DSIM_SEND_HEAD*)malloc(sizeof(WTAPI_TE_DSIM_SEND_HEAD));
    memset(dhead,0x0,sizeof(WTAPI_TE_DSIM_SEND_HEAD));
    quint64 start = ui->spinBox_start->text().toUInt();
    Int48 time(start);
    dhead->uiSHTime = time.high16();
    dhead->uiSLTime = time.low32();
    int group = ui->lineEdit_allCount->text().toInt();
    dhead->uiFrameNum = 0;
    dhead->uiReserve = group;//记录组数
    //数据长度
    int dataLen = ui->spinBox_maxlen->text().toInt();//单帧
    dhead->uiLength = 6 + 4 + 1 + dataLen / 4;
    return dhead;
}

WTAPI_TE_DSIM_FCHEAD* wid_FCdataInfo::getWTFCHeader()
{
    WTAPI_TE_DSIM_FCHEAD* fhead = (WTAPI_TE_DSIM_FCHEAD*)malloc(sizeof(WTAPI_TE_DSIM_FCHEAD));
    memset(fhead,0x0,sizeof(WTAPI_TE_DSIM_FCHEAD));
    fhead->D_ID = ui->lineEdit_D_ID->text().toUInt(nullptr,16);
    fhead->R_CTL = ui->lineEdit_R_CTL->text().toUInt(nullptr,16);
    fhead->S_ID = ui->lineEdit_S_ID->text().toUInt(nullptr,16);
    fhead->CS_CTL = ui->lineEdit_CS_CTL->text().toUInt(nullptr,16);
    fhead->F_CTL = ui->lineEdit_F_CTL->text().toUInt(nullptr,16);
    fhead->TYPE = ui->lineEdit_TYPE->text().toUInt(nullptr,16);
    fhead->SEQ_CNT = ui->lineEdit_SEQ_CNT->text().toUInt(nullptr,16);
    fhead->DF_CTL = ui->lineEdit_DF_CTL->text().toUInt(nullptr,16);
    fhead->SEQ_ID = ui->lineEdit_SEQ_ID->text().toUInt(nullptr,16);
    fhead->RX_ID = ui->lineEdit_RX_ID->text().toUInt(nullptr,16);
    fhead->OX_ID = ui->lineEdit_OX_ID->text().toUInt(nullptr,16);
    fhead->Parameter = ui->lineEdit_Parameter->text().toUInt(nullptr,16);
    fhead->MSG_ID = ui->lineEdit_MSG_ID->text().toUInt(nullptr,16);
    fhead->RES0 = ui->lineEdit_RES0->text().toUInt(nullptr,16);
    fhead->RES1 = ui->lineEdit_RES1->text().toUInt(nullptr,16);
    fhead->MSG_LEN = ui->lineEdit_MSG_LEN->text().toUInt(nullptr,16);
    fhead->MSG_PRIORITY = ui->lineEdit_MSG_PRIORITY->text().toUInt(nullptr,16);
    fhead->MSG_L = ui->lineEdit_MSG_L->text().toUInt(nullptr,16);
    return fhead;
}

void wid_FCdataInfo::on_pushButton_creat_clicked()
{
    //生成数据
    int groups = ui->lineEdit_allCount->text().toInt();
    int maxLen = ui->spinBox_maxlen->text().toInt();
    int allDataCount = groups * maxLen;
    int colunm = 16;
    int row = allDataCount / colunm;
    ui->tableWidget_FCdata->setRowCount(row);
    int random = ui->checkBox_random->checkState();
    int Flgindex = ui->checkBox_index->checkState();
    if(Flgindex != 0)
    {
        //生成帧计数填充
    }
    //
    if(random != 0)
    {
        //生成随机数
        QRandomGenerator* rng = QRandomGenerator::global();
#if 1
        //生成数据并缓存到内存
#endif

#if 1
        for(int i = 0;i < row;i++)
        {
            for(int j = 0;j < colunm;j++)
            {
                //生成随机字节(0-255)
                int randomByte = rng->bounded(256);
                QTableWidgetItem* item = new QTableWidgetItem();
                item->setTextAlignment(Qt::AlignCenter);
                //存储原始数值
                item->setData(Qt::UserRole,randomByte);
                //默认显示为16进制并补齐两位
                item->setText(QString("%1").arg(randomByte,2,16,QChar('0')).toUpper());
                ui->tableWidget_FCdata->setItem(i,j,item);
            }
        }
#endif
    }
}

void wid_FCdataInfo::on_comboBox_type_currentIndexChanged(int index)
{
    ui->tableWidget_FCdata->setUpdatesEnabled(false);
    int row = ui->tableWidget_FCdata->rowCount();
    int colunm = ui->tableWidget_FCdata->columnCount();
    for(int i = 0;i < row;i++)
    {
        for(int j = 0;j < colunm;j++)
        {
            QTableWidgetItem* item = ui->tableWidget_FCdata->item(i,j);
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
    ui->tableWidget_FCdata->setUpdatesEnabled(true);
}

void wid_FCdataInfo::onItemchange(QTableWidgetItem *item)
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

void wid_FCdataInfo::bindSpandLineedit(QSpinBox *spin, QLineEdit *edit, int hexwidth)
{
    //1.sp->lineedit
    connect(spin,QOverload<int>::of(&QSpinBox::valueChanged),this,
            [edit,hexwidth](int value){
                edit->setText(QString("%1").arg(value,hexwidth,16,QChar('0')).toUpper());
    });

    //2.lineedit->sp
    connect(edit,&QLineEdit::editingFinished,this,[spin,edit]()
    {
        bool ok = false;
        int val = edit->text().toInt(&ok,16);
        if(ok)
        {
            spin->setValue(val);
        }
    });

    //初始化
    edit->setText(QString("%1").arg(spin->value(),hexwidth,16,QChar('0')).toUpper());
}

void wid_FCdataInfo::setupValidtors(QLineEdit *edit, int maxHexChars,bool isMSGl)
{
    if(!edit)
    {
        return;
    }
    //1.构造正则表达式限制
    QString pattern = isMSGl ? "^[01]{0,1}$" : QString("[0-9A-Fa-f]{0,%1}").arg(maxHexChars);
    QRegularExpression re(pattern);
    edit->setValidator(new QRegularExpressionValidator(re,edit));

    //2.占位提示
//    edit->setPlaceholderText(QString("Hex (max %1)").arg(maxHexChars));

    //3.输入时自动转为大写
    connect(edit,&QLineEdit::textEdited,edit,[edit]()
    {
        QString s = edit->text().toUpper();
        if(s != edit->text())
        {
            int pos = edit->cursorPosition();
            edit->setText(s);
            edit->setCursorPosition(qMin(pos,s.length()));
        }
    });

    //4.失去焦点时自动前补零（A3-0000A3）
    connect(edit,&QLineEdit::editingFinished,edit,[edit,maxHexChars]()
    {
        QString s = edit->text().toUpper();
        if(!s.isEmpty() && s.length() < maxHexChars)
        {
            edit->setText(s.rightJustified(maxHexChars,'0'));
        }
    });
}

QByteArray wid_FCdataInfo::getData()
{
    QByteArray result;
    int rows = ui->tableWidget_FCdata->rowCount();
    int cols = ui->tableWidget_FCdata->columnCount();
    for(int row = 0;row <rows;row++)
    {
        for(int col = 0;col < cols;col++)
        {
            QTableWidgetItem* item = ui->tableWidget_FCdata->item(row,col);
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

void wid_FCdataInfo::on_comboBox_time_currentIndexChanged(int index)
{

}

void wid_FCdataInfo::on_pushButton_clear_clicked()
{
    //清空表格
    ui->tableWidget_FCdata->blockSignals(true);
    ui->tableWidget_FCdata->clearContents();
    ui->tableWidget_FCdata->blockSignals(false);
}

int wid_FCdataInfo::getStart()
{
    return ui->spinBox_start->value();
}

int wid_FCdataInfo::getStep()
{
    return ui->spinBox_step->value();
}

int wid_FCdataInfo::getGroups()
{
    return ui->lineEdit_allCount->text().toInt();
}

int wid_FCdataInfo::getbbcValue()
{
    return ui->lineEdit_bbcValue->text().toInt();
}

void wid_FCdataInfo::setVis()
{
    ui->widget_7->hide();
    ui->widget_8->hide();
}
