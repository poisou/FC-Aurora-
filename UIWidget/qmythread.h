#ifndef QMYTHREAD_H
#define QMYTHREAD_H

#include <QObject>
#include <QThread>
#include "../include/mystruct.h"

class QMyThread : public QThread
{
Q_OBJECT
public:
    QMyThread();
    ShowType m_etype;

    void *pThread;
    void run();
    bool bRun;
signals:
    void Fun1();
};

#endif // QMYTHREAD_H
