#ifndef ELSFORM_H
#define ELSFORM_H

#include <QDialog>
#include"custommsgtablemodel.h"
#include <QTimer>
#include <QQueue>
#include <WTContrl.h>
#include <QDebug>
#include <wtgl.h>
#include <QMutex>
#include <QCloseEvent>
#include <QTableWidgetItem>
#include <QMenu>
#include <QPushButton>
#include "ElsDlg.h"
#include "qmythread.h"
struct ELSPaket
{
    uint seq=0;
    uint len=0;
    uchar data[4096];

};
namespace Ui {
class ELSForm;
}

class ELSForm : public QDialog
{
    Q_OBJECT

public:
    explicit ELSForm(QWidget *parent = nullptr);
    void init(QString sch);
    ~ELSForm();

    void showElsData(ELSPaket *pkt);

    void readElsDataThread();

    void sendElsDataThread();

    void setThreadStop();

    void readThreadStart();

    void readThreadStop();

    void closeEvent(QCloseEvent *e);

    void MessageInfo(const MsgBlock *par_msg,int ilen,int pos=0);

    void showReadDataHex(unsigned char*data,int len);

    void showReadDataHexHighLight(unsigned char*data, int len, int spos, int epos);

    void AddMsg(QList<MsgBlock> list);

    void clearData();

    void showTree(unsigned char *data, int len);

signals:
    void SendFinish(int index);
public slots:
    void FlushTimeOut();

    void msgBtnClicked();

    void slotSendFinish(int index);
private slots:
    void on_tableWidget_customContextMenuRequested(const QPoint &pos);

    void on_tableWidget_itemDoubleClicked(QTableWidgetItem *item);

    void on_tableWidget_itemClicked(QTableWidgetItem *item);

    void slot_menu();

    void on_tableView_clicked(const QModelIndex &index);

    void on_treeWidget_itemClicked(QTreeWidgetItem *item, int column);

    void on_tableView_customContextMenuRequested(const QPoint &pos);

private:
    Ui::ELSForm *ui;

    CustomMsgTableModel *m_MsgQueModel;
    QTimer FlushTimer;

    QQueue<ELSPaket> m_queueElsData;

    QVector<ELSPaket> m_showElsData;

    ELSPaket m_curPaket;

    QString m_sch;

    QMyThread *m_ReadElsDataThread;

    QMyThread *m_SendElsDataThread;

    WTContrl *m_Pctrl=nullptr;

    QMutex mutex;

    uint m_seqCout=0;

    QMap<int ,MsgBlock> m_SendBlockMap;

    QList<MsgBlock> m_blockList;

    QTableWidgetItem *mpClickItem;

    QMenu m_CreateMsgMenu;
    QModelIndex m_CurModeIndex;

    QMutex sendMutex;

    QMenu m_ClearMenu;


};

#endif // ELSFORM_H
