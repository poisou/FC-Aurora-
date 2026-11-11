#include "fcsendthreadpro.h"

FCSendThreadpro::FCSendThreadpro(qint64 ipriID,int ch,QObject* parent)
    : QObject(parent)
{
    mch = ch;
    mipriID = ipriID;
    QStringList chList = PM()->GetCHLs(mipriID,"FC");
//    pCtrl = (WTContrl *)(PM()->GetWTCtrl(chList.at(ch)));
    pCtrl = (WTContrl *)(PM()->GetWTCtrl(chList.at(ch)));
    mwthead = (WTAPI_TE_DSIM_SEND_HEAD*)malloc(sizeof(WTAPI_TE_DSIM_SEND_HEAD));
    mfchead = (WTAPI_TE_DSIM_FCHEAD*)malloc(sizeof(WTAPI_TE_DSIM_FCHEAD));
    memset(mwthead,0x0,sizeof(WTAPI_TE_DSIM_SEND_HEAD));
    memset(mfchead,0x0,sizeof(WTAPI_TE_DSIM_FCHEAD));
    QRandomGenerator* rng = QRandomGenerator::global();
    errCount = rng->bounded(3,9);
}

FCSendThreadpro::~FCSendThreadpro()
{
    free(mwthead);
    free(mfchead);
}

void FCSendThreadpro::dowork()
{
    while(isStart)
    {
        if(pause)
        {
            continue;
        }
        if(mtype == 0)//开始
        {
            pause = false;
            if(!sendFinish)
            {
                //获取发送数据
                //1.判断组数确定发送次数
                for(int i = 0;i < groups;i++)
                {
                    if(nowTimestart > 0)
                    {
                        quint64 nowSecond = startTime * 0.0000001;
                        nowTimestart -= nowSecond;
                        startTime +=  timestep;
                        index += dataLen/groups;
                        continue;
                    }
                    //WT头
                    m_msgid++;
                    Int48 time;
                    time.setValue(startTime/100);
                    mwthead->uiSHTime = time.high16();
                    mwthead->uiFrameNum = m_msgid;// i + 1;
                    mwthead->uiSLTime = time.low32();
                    mwthead->uiLength = 11 + dataLen/groups/4;
                    startTime +=  timestep;
                    //当前单帧的时间
                    quint64 nowLen = dataLen/groups;
                    quint64 nowSeconds = startTime * 0.0000001;
                    int min = nowSeconds / 60;
                    int seconds = nowSeconds % 60;
                    emit sig_nowTimes(mch,min,seconds,nowLen);
                    int totalBytes = (3 + 6 + 4 + 1)*4 + dataLen/groups;
                    //FC头内容已填充完毕
                    //使用index来进行数据分段
#if 1
                    char* dbuffer = (char*)malloc(totalBytes);//单次发送的数据
                    memset(dbuffer,0x0,totalBytes);
                    memcpy(dbuffer,mwthead,sizeof(WTAPI_TE_DSIM_SEND_HEAD));//wt
                    memcpy(dbuffer + sizeof(WTAPI_TE_DSIM_SEND_HEAD),mfchead,sizeof(WTAPI_TE_DSIM_FCHEAD));//fc
                    int ssss =m_msgid;
                    memcpy(dbuffer + sizeof(WTAPI_TE_DSIM_SEND_HEAD) + sizeof(WTAPI_TE_DSIM_FCHEAD),&ssss,sizeof(int));
                    memcpy(dbuffer + sizeof(WTAPI_TE_DSIM_SEND_HEAD) + sizeof(WTAPI_TE_DSIM_FCHEAD) + sizeof(int),dataByte.constData() + index,dataLen/groups);
                    //转换为qbytearray
                    QByteArray ba(dataByte.constData() + index,dataLen/groups);
                    QByteArray bytebuffer(dbuffer,totalBytes);
                    QVariant scmd = QVariant::fromValue(bytebuffer);
//                    qDebug()<<"send_data:*****"<<dataByte.constData();
#endif

                    RECVDATA sdata;
                    sdata.ch = mch;
                    sdata.data = bytebuffer;
                    sdata.dataLen = totalBytes;
                    sdata.mdata = ba;
                    sdata.mdataLen = dataLen/groups;
                    sdata.msgIdx = ssss;
                    sdata.time.setValue(startTime/100);
                    if(errCount == i)
                    {
                        //发送错误帧时标提示
                        sdata.err = true;
                        sendFinish = true;
                        sdata.errc = errCount;
                        sdata.time.setValue(startTime);
                    }
                    else
                    {
                        sdata.err = false;
                    }
                    pCtrl->Contrl("sendData",scmd);
                    index += dataLen/groups;
                    double v = i * 100/groups;
                    emit sig_sendJD(v);//发送进度
                    emit sig_sendData(sdata);
                    free(dbuffer);
                    if(errCount == i)
                    {
                        sendFinish = true;
                        break;
                    }
                    QThread::msleep(5);
                }
                if(errCount == -1)
                {
                    emit sig_sendJD(100);//发送进度
                }
                index = 0;
                startTime = 0;
                sendFinish = true;
                if(!timesSend)
                {
                    emit sig_finish();
                }
            }
        }
        if(mtype == 1)//暂停
        {
            pause = true;
        }
        if(mtype == 2)//停止
        {
            pause = true;
            index = 0;
        }
        QThread::msleep(10);
    }
    QThread::msleep(10);
//    m_msgid = 0;
}

void FCSendThreadpro::setDataName(QString name)
{
    //获取ctrl中的发送data并完成组包
    if(mSourecname == name)
    {
        sendFinish = false;
    }
    else {
        mSourecname = name;
        QStringList chList = PM()->GetCHLs(mipriID,"FC");
        WTContrl *pCtrl = (WTContrl *)(PM()->GetWTCtrl(chList.at(mch)));
        MsgBlock mblock = pCtrl->cMessageBlock->mapsTTabel.value(mSourecname);//获取ctrl中的发送数据源
        dataLen = mblock.iTxLen;//数据长度
        groups = mblock.ITxCnt;//组数
        startTime = mblock.iTotal;//初始时标
        timestep = mblock.period;//时标步进
        bb = mblock.uprioirity;//bb
        quint64 total = (startTime + groups * timestep);
        quint64 totalSeconds = total * 0.0000001;
        int min = totalSeconds / 60;
        int seconds = totalSeconds % 60;
        //总进度
        emit sig_totalTimes(mch,min,seconds);
        //下发bb
        QVariant scmd = bb;
        pCtrl->Contrl("setbb",scmd);
        //填充WT头
        QByteArray wthByte = mblock.varparam.toByteArray();
        memcpy(mwthead,wthByte.constData(),sizeof(WTAPI_TE_DSIM_SEND_HEAD));
        //填充FC头
        QByteArray fchByte = mblock.varparamini.toByteArray();
        memcpy(mfchead,fchByte.constData(),sizeof(WTAPI_TE_DSIM_FCHEAD));
        //数据段
        dataByte = mblock.sData;
        sendFinish = false;
    }
    index = 0;
}

void FCSendThreadpro::setStart(bool Start)
{
    isStart = Start;
}

void FCSendThreadpro::setStatus(int type)
{
    pause = false;
    mtype = type;
}

void FCSendThreadpro::setStartTime(qint64 time)
{
    nowTimestart = time / 0.0000001;
}

void FCSendThreadpro::setSendRecvControl(int type)
{
    //设置收发使能
    if(type == 0)
    {
        //收发
        QVariant scmd = 0;
        pCtrl->Contrl("setRSType",scmd);
    }
    if(type == 1)
    {
        //发
        QVariant scmd = 1;
        pCtrl->Contrl("setRSType",scmd);
    }
    if(type == 2)
    {
        //收
        QVariant scmd = 2;
        pCtrl->Contrl("setRSType",scmd);
    }
    if(type == -1)
    {
        //完全关闭接收
        QVariant scmd = -1;
        pCtrl->Contrl("setRSType",scmd);
    }
}
