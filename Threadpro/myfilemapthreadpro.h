#ifndef MYFILEMAPTHREADPRO_H
#define MYFILEMAPTHREADPRO_H

#include <QObject>
#include <QFile>
#include <QString>

class myFilemapthreadpro : public QObject
{
    Q_OBJECT
public:
    myFilemapthreadpro(qint64 ipriID,QString path,QObject* parent = nullptr);
    ~myFilemapthreadpro();
    void setConfig(int start,int end);
    qint64 getFileSize();
private:
    void Init();
    void dowork();
private:
    qint64 mipriID;
    QFile mfile;
    int mstart;
    int mend;
    uchar* data;
};

#endif // MYFILEMAPTHREADPRO_H
