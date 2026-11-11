#include "fcsendreadthreadpro.h"

fcsendreadthreadpro::fcsendreadthreadpro(qint64 ipriID,int ch,QObject* parent)
    : QObject(parent)
{
    mch = ch;
    mipriID = ipriID;
    sendInfo = (WTAPI_TE_DSIM_SEND_TIME*)malloc(sizeof(WTAPI_TE_DSIM_SEND_TIME));
    memset(sendInfo,0x0,sizeof(WTAPI_TE_DSIM_SEND_TIME));
    QStringList chList = PM()->GetCHLs(mipriID,"FC");
    pCtrl = (WTContrl *)(PM()->GetWTCtrl(chList.at(ch)));
}

fcsendreadthreadpro::~fcsendreadthreadpro()
{
    free(sendInfo);
}

void fcsendreadthreadpro::dowork()
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
                //收到时标值
                int msgid = sendInfo->uiFrameNum;
                ilen = 0;
                emit sig_sendInfo(mch,msgid,sendInfo->uiSHTime,sendInfo->uiSLTime);
            }
            QThread::msleep(10);
        }
        QThread::msleep(10);
    }
}

void fcsendreadthreadpro::setStart(bool Start)
{
    isStart = Start;
}

void fcsendreadthreadpro::setStatus(int type)
{
    mtype = type;
}
