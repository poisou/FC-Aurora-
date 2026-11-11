#ifndef WID_MSGMONITOR_H
#define WID_MSGMONITOR_H

#include <QWidget>
#include <QTableWidget>
#include "datadefine.h"
#include <QQueue>
#include <QDateTime>
#include <QRandomGenerator>
#include <QVariant>
#include "dlog_info.h"
#include "../Threadpro/WTAPI_TE_DSIM.h"

namespace Ui {
class wid_msgmonitor;
}

class wid_msgmonitor : public QWidget
{
    Q_OBJECT

public:
    explicit wid_msgmonitor(qint64 ipriID,int type,QWidget *parent = nullptr);
    ~wid_msgmonitor();
    void setRecvShow(QQueue<RECVDATA> queue_data);
    void setSendShow(QQueue<RECVDATA> queue_data);
    void setARecvShow(QQueue<RECVDATA> queue_data);
    void setASendShow(QQueue<RECVDATA> queue_data);
    qint64 qtm[1000];
    QString TimeToStr(LONGLONG LTime);
    void setSendClear(bool t);
signals:
    void sig_clearRecv();
    void sig_clearSend();
private:
    void Init();
    void iii();
public slots:
    void showSendValue(QTableWidgetItem *item);
    void showRecvValue(QTableWidgetItem *item);
    void slot_sendInfo(int ch,int datalen,QByteArray bytebuffer,QVariant info);
private slots:
    void on_pbtn_clearsend_clicked();

    void on_pbtn_clearrecv_clicked();

private:
    Ui::wid_msgmonitor *ui;
    qint64 mipriID;
    int mtype;// 0:FC 1:aurora 2:net
    int send_idx;
    int recv_idx = 0;
    int midx = 1;
    int idx = 0;

    int erridx = -1;
    int aerrid = -1;
};

#endif // WID_MSGMONITOR_H
