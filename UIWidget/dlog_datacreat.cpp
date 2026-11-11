#include "dlog_datacreat.h"
#include "ui_dlog_datacreat.h"

dlog_datacreat::dlog_datacreat(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::dlog_datacreat)
{
    ui->setupUi(this);
    Init();
}

void dlog_datacreat::Init()
{
    setWindowFlags(Qt::Window | Qt::FramelessWindowHint);
    setMouseTracking(true);
    mWTFChead = (WTAPI_TE_DSIM_SEND_HEAD*)malloc(sizeof(WTAPI_TE_DSIM_SEND_HEAD));
    mFCHead = (WTAPI_TE_DSIM_FCHEAD*)malloc(sizeof(WTAPI_TE_DSIM_FCHEAD));
    memset(mWTFChead,0x0,sizeof(WTAPI_TE_DSIM_SEND_HEAD));
    memset(mFCHead,0x0,sizeof(WTAPI_TE_DSIM_FCHEAD));
    mFCedit = new wid_FCData();
    mFCEdit = new wid_FCdataInfo();
    ui->stackedWidget->addWidget(mFCedit);//交付时暂时使用mFCEdit，mFCedit还未完成开发
    //待添加以太网和fc
    mAuroraEdit = new wid_AuroraDataInfo();
    ui->stackedWidget->addWidget(mAuroraEdit);
    mFCEdit3 = new wid_FCdataInfo();
    mFCEdit3->setVis();
    ui->stackedWidget->addWidget(mFCEdit3);
    //0812 应付验收

}

dlog_datacreat::~dlog_datacreat()
{
    free(mWTFChead);
    free(mFCHead);
    delete ui;
}

void dlog_datacreat::on_pushButton_clicked()
{
    //获取数据源名+数据内容
    QString dataName = ui->lineEdit_name->text();
    int type = ui->comboBox_type->currentIndex();//0 fc 1 aurora 2 net
    if(type == 0)
    {
        //FC
        WTAPI_TE_DSIM_SEND_HEAD* WThead = mFCEdit->getWTDSIMHeader();
        memcpy(mWTFChead,WThead,sizeof(WTAPI_TE_DSIM_SEND_HEAD));
        WTAPI_TE_DSIM_FCHEAD* FCHead = mFCEdit->getWTFCHeader();
        memcpy(mFCHead,FCHead,sizeof(WTAPI_TE_DSIM_FCHEAD));
        mFCData = mFCEdit->getData();
        mFCStart = mFCEdit->getStart();
        mFCStep = mFCEdit->getStep();
        mFCgroups = mFCEdit->getGroups();
        bbc = mFCEdit->getbbcValue();
        qDebug()<<"FCData总长："<<mFCData.size();
        qDebug()<<"前16字节："<<mFCData.left(16).toHex(' ');
//        //存储后释放空间

        emit sig_saveFCData(dataName);
    }
}

void dlog_datacreat::on_comboBox_type_currentIndexChanged(int index)
{
    ui->stackedWidget->setCurrentIndex(index);
}

void dlog_datacreat::on_pushButton_return_clicked()
{
    //返回上一级
    emit sig_back();
}

WTAPI_TE_DSIM_SEND_HEAD* dlog_datacreat::getWTDSIMHeader()
{
    return mWTFChead;
}

WTAPI_TE_DSIM_FCHEAD* dlog_datacreat::getWTFCHeader()
{
    return mFCHead;
}

QByteArray dlog_datacreat::getData()
{
    return mFCData;
}

int dlog_datacreat::getFCStart()
{
    return mFCStart;
}

int dlog_datacreat::getbbc()
{
    return bbc;
}

int dlog_datacreat::getFCStep()
{
    return mFCStep;
}

int dlog_datacreat::getFCGroups()
{
    return mFCgroups;
}
