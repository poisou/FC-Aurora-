#include "aurorasendthreadpro.h"

Aurorasendthreadpro::Aurorasendthreadpro(qint64 ipriID,int ch,QObject* parent)
    : QObject(parent)
{
    mch = ch;
    mipriID = ipriID;
    QStringList chList = PM()->GetCHLs(mipriID,"Aurora");
    pCtrl = (WTContrl *)(PM()->GetWTCtrl(chList.at(ch)));
    a_data = (AURORADATA*)malloc(sizeof(AURORADATA));
    memset(a_data,0x0,sizeof(AURORADATA));
}

Aurorasendthreadpro::~Aurorasendthreadpro()
{

}

void Aurorasendthreadpro::dowork()
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
                    //Aurora
                    startTime +=  timestep;
                    //当前单帧的时间
                    quint64 nowLen = dataLen/groups;
                    quint64 nowSeconds = startTime * 0.0000001;
                    int min = nowSeconds / 60;
                    int seconds = nowSeconds % 60;
                    emit sig_nowTimes(mch,min,seconds,nowLen);
                    int totalBytes = 16 + dataLen;
                    //使用index来进行数据分段
#if 1
                    char* dbuffer = (char*)malloc(totalBytes);//单次发送的数据
                    memset(dbuffer,0x0,totalBytes);
                    memcpy(&(a_data->frameData),dataByte.constData()+index,dataLen/groups);
                    a_data->frameIdx = m_msgid;
                    a_data->framLen = dataLen/groups;
                    a_data->txTime = startTime;
                    memcpy(dbuffer,a_data,sizeof(AURORADATA));
                    int ssss =m_msgid;
                    //转换为qbytearray
                    QByteArray ba(dataByte.constData() + index,dataLen/groups);
                    QByteArray bytebuffer(dbuffer,totalBytes);
                    QVariant scmd = QVariant::fromValue(bytebuffer);
#endif
                    RECVDATA sdata;
                    sdata.ch = mch;
                    sdata.data = bytebuffer;
                    sdata.dataLen = totalBytes;
                    sdata.mdata = ba;
                    sdata.mdataLen = dataLen/groups;
                    sdata.msgIdx = m_msgid;
                    sdata.err = false;
                    sdata.atime = startTime;
                    pCtrl->Contrl("sendData",scmd);
                    m_msgid++;
                    index += dataLen/groups;
                    double v = i * 100/groups;
                    emit sig_sendJD(v);//发送进度
                    emit sig_sendData(sdata);
                    free(dbuffer);
                    QThread::msleep(5);
                }
                emit sig_sendJD(100);//发送进度
                index = 0;
                startTime = 0;
                sendFinish = true;
                if(!timesSend)
                {
                    emit sig_finish();
                }
            }
        }
    }
}

void Aurorasendthreadpro::setDataName(QString name)
{
    //获取ctrl中的发送data并完成组包
    if(mSourecname == name)
    {
        sendFinish = false;
    }
    else {
        mSourecname = name;
        QStringList chList = PM()->GetCHLs(mipriID,"Aurora");
        WTContrl *pCtrl = (WTContrl *)(PM()->GetWTCtrl(chList.at(mch)));
        MsgBlock mblock = pCtrl->cMessageBlock->mapsTTabel.value(mSourecname);//获取ctrl中的发送数据源
        dataLen = mblock.iTxLen;//数据长度
        groups = mblock.ITxCnt;//组数
        startTime = mblock.iTotal;//初始时标
        timestep = mblock.period;//时标步进
        //数据段
        dataByte = mblock.sData;
        sendFinish = false;
    }
    index = 0;
}

void Aurorasendthreadpro::setStart(bool Start)
{
    isStart = Start;
}

void Aurorasendthreadpro::setStatus(int type)
{
    pause = false;
    mtype = type;
}

void Aurorasendthreadpro::setSendRecvControl(int type)
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
