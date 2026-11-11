#include "aurorasendreadthreadpro.h"

Aurorasendreadthreadpro::Aurorasendreadthreadpro(qint64 ipriID,int ch,QObject* parent)
    : QObject(parent)
{
    mch = ch;
    mipriID = ipriID;
    sendInfo = (AURORATIMEDATA*)malloc(sizeof(AURORATIMEDATA));
    memset(sendInfo,0x0,sizeof(AURORATIMEDATA));
    QStringList chList = PM()->GetCHLs(mipriID,"Aurora");
    pCtrl = (WTContrl *)(PM()->GetWTCtrl(chList.at(ch)));
}

Aurorasendreadthreadpro::~Aurorasendreadthreadpro()
{
    free(sendInfo);
}

void Aurorasendreadthreadpro::dowork()
{
    while(isStart)
    {
        if(mtype == 0)//开始
        {
            int type = 1;
            UINT ilen = 0;
            pCtrl->ReadData(type,(PUCHAR)sendInfo,ilen);
            if(ilen > 0)
            {
                //收到时标
                int msgid = sendInfo->frameIdx;
//                ilen = 0;
                emit sig_sendInfo(mch,msgid,sendInfo->txTime);
            }
            QThread::msleep(10);
        }
        QThread::msleep(10);
    }
}


void Aurorasendreadthreadpro::setStart(bool Start)
{
    isStart = Start;
}

void Aurorasendreadthreadpro::setStatus(int type)
{
    mtype = type;
}
