#ifndef FCDATADEFINE_H
#define FCDATADEFINE_H

#endif // FCDATADEFINE_H
#include <QMetaType>
#include <windows.h>
#include <QString>
typedef struct DEVINFO
{
    int type;
    QString devname;
    int id;
    QString N_Port = "";
    QString set = "";
    QString IP = "";
    QString Port = "";
    QString Mac = "";
    QString status = "0";
    QString ch = "";
}DEVINFO;

typedef struct DATAMSG
{
    char data[2048];
}DATAMSG;

typedef struct MYHANDLE
{
    HANDLE HandleFC;
    QVariant scmd;
}MYHANDLE;
Q_DECLARE_METATYPE(MYHANDLE)
