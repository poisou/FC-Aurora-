#ifndef DATADEFINE_H
#define DATADEFINE_H

#include <QtGlobal>
#include <QCoreApplication>
#include <QVariant>
#include <QMetaType>
#define MAXFCDATALEN 2096
#define MAXRECVLEN 2148

//1s = 1000ms = 1000000us = 1000000000ns
struct Int48
{
    quint64 data;//使用64位存储，仅使用低48位
    Int48(quint64 v = 0)
    {
        data = v &((1ULL << 48) - 1);
    }
    //获取低32位
    quint32 low32() const
    {
        return static_cast<quint32>(data & 0xFFFFFFFFULL);
    }
    //获取高16位
    quint16 high16() const
    {
        return static_cast<quint16>(data >> 32);
    }

    //设置低32位
    void setLow32(quint32 val)
    {
        data = (data & ~0xFFFFFFFFULL) | (val & 0xFFFFFFFFULL);
    }

    //设置高16位
    void sethigh16(quint16 val)
    {
        data = (data & ~(0xFFFFULL << 32)) | ((val & 0xFFFFULL) << 32);
    }

    void setValue(quint64 v)
    {
        data = v & ((1ULL << 48) - 1);
    }

    quint64 value() const{
        return data & ((1ULL << 48) - 1);
    }
};

#pragma pack(push,1)
//coreTemp结构体
struct CoreTempShareData
{
    int uiLoad;
    int uiTjMax;
    int uiTjOffset;
    int CoreCnt;
    int Temp[16];
};
#pragma pack(pop)


typedef struct RECVDATA
{
    int ch;
    QByteArray data;//带WT头的数据
    QByteArray mdata;//仅数据部分
    int dataLen;
    Int48 time;
    unsigned long long atime;
    int mdataLen;
    int msgIdx;
    bool err;
    int errc;
}RECVDATA;
Q_DECLARE_METATYPE(RECVDATA)

typedef struct AURORADATA
{
    unsigned int frameIdx;	 //帧序号 接收数据无帧序号，因此读取数据时此变量无效 （若是UART数据则此此项无效，收发数据时可忽略）
    unsigned int framLen;	 //帧长度
    unsigned long long txTime;	 //发送/接收时间	  （若是UART数据则此此项无效，收发数据时可忽略）
    unsigned char frameData[4096];
}AURORADATA;
Q_DECLARE_METATYPE(AURORADATA)

typedef struct AURORATIMEDATA
{
    unsigned int frameIdx;	 //帧序号 接收数据无帧序号，因此读取数据时此变量无效 （若是UART数据则此此项无效，收发数据时可忽略）
    unsigned int framLen;	 //帧长度
    unsigned long long txTime;	 //发送/接收时间	  （若是UART数据则此此项无效，收发数据时可忽略）
}AURORATIMEDATA;
Q_DECLARE_METATYPE(AURORATIMEDATA)

typedef struct ICDDATA
{
    QString name;
    int len;
    QString type;
    QString ICDType;
}ICDDATA;

typedef struct MYDBSTRUCT
{
    QString date;
    QString name;
    QByteArray data;
    QByteArray WTHead;
    QByteArray FCHead;
    int datalen;
    int start;
    int step;
    int groups;
    QString type;
    int bbc;
}MYDBSTRUCT;

typedef struct CREATFCDATA
{
    qint64 datalen = 0;//帧长
//    QByteArray data;//帧数据
    RECVDATA data;//帧数据
}CREATFCDATA;


#endif // DATADEFINE_H
