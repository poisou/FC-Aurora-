#include "aurorarecvthreadpro.h"

Aurorarecvthreadpro::Aurorarecvthreadpro(qint64 ipriID,int ch,QObject* parent)
    : QObject(parent)
{
    mch = ch;
    mipriID = ipriID;
    QStringList chList = PM()->GetCHLs(mipriID,"Aurora");
    pCtrl = (WTContrl *)(PM()->GetWTCtrl(chList.at(ch)));
    ucBuffer = (unsigned char*)malloc(MAX_BUFF_BYTE * sizeof(unsigned char));
}

Aurorarecvthreadpro::~Aurorarecvthreadpro()
{
    free(ucBuffer);
}

void Aurorarecvthreadpro::dowork()
{
    while(isStart)
    {
        if(mtype == 0)//开始
        {
            //获取数据
            int type = 0;
            UINT ilen = 0;
            pCtrl->ReadData(type,ucBuffer,ilen);
            if(ilen > 0)
            {
                //解包
                RECVDATA data;
                PWT_AURORA_FRAME ss = (PWT_AURORA_FRAME)ucBuffer;
                QByteArray ba(reinterpret_cast<const char*>(ucBuffer + 16),ss->framLen);
                data.ch = mch;
                data.mdataLen = ss->framLen;
                data.data = ba;
                data.atime = ss->txTime;
                data.msgIdx =msgid;
                msgid++;
                emit sig_recvAuroraData(data);
            }
        }
        QThread::msleep(10);
    }
    msgid = 0;
}

void Aurorarecvthreadpro::setControl(int type)
{
    if(type == 0)
    {
        //使能
        QVariant scmd = 2;
        pCtrl->Contrl("setRSType",scmd);
    }
    if(type == -1)
    {
        //不使能
        QVariant scmd = -1;
        pCtrl->Contrl("setRSType",scmd);
    }
}

void Aurorarecvthreadpro::setStart(bool Start)
{
    isStart = Start;
}

void Aurorarecvthreadpro::setStatus(int type)
{
    mtype = type;
}
