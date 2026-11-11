#include "fcrecvthreadpro.h"

FCRecvThreadpro::FCRecvThreadpro(qint64 ipriID,int ch,QObject* parent)
    : QObject(parent)
{
    mch = ch;
    mipriID = ipriID;
    QStringList chList = PM()->GetCHLs(mipriID,"FC");
    pCtrl = (WTContrl *)(PM()->GetWTCtrl(chList.at(ch)));
    ucBuffer = (unsigned char*)malloc(2156 * sizeof(unsigned char));
}
FCRecvThreadpro::~FCRecvThreadpro()
{
    free(ucBuffer);
}

void FCRecvThreadpro::dowork()
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
                //获取接收时标
                quint32 low;
                quint32 high;
                memcpy(&low,ucBuffer,sizeof(quint32));
                memcpy(&high,ucBuffer + sizeof(quint32),sizeof(quint32));
                UINT32 de;
                memcpy(&de,ucBuffer + 8,sizeof(UINT32));
                if(de != WTDEFINE)
                {
                    return;
                }
//                int wtlen = sizeof(WTAPI_TE_DSIM_SEND_HEAD);
//                int fclen = sizeof(WTAPI_TE_DSIM_FCHEAD);

                QByteArray ba(reinterpret_cast<const char*>(ucBuffer),ilen);
                QByteArray bb(reinterpret_cast<const char*>(ucBuffer + 56),ilen -60);

                data.time.sethigh16(high);
                data.time.setLow32(low);
                data.ch = mch;
                data.mdataLen = ilen;
                data.dataLen = ilen - 60;
                data.data = ba;
                data.mdata = bb;
                data.msgIdx =msgid;
                msgid++;
                emit sig_recvFCData(data);
            }
        }
        QThread::msleep(20);
    }
    msgid = 0;
}

void FCRecvThreadpro::setControl(int type)
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

void FCRecvThreadpro::setStart(bool Start)
{
    isStart = Start;
}

void FCRecvThreadpro::setStatus(int type)
{
    mtype = type;
}


