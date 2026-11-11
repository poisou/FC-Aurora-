#ifndef SENDELSDLG_H
#define SENDELSDLG_H

#include <QDialog>
#include <WTContrl.h>
#include <QDebug>
#include <wtgl.h>
#include <QMenu>
#include <QAction>
#include <QPushButton>
#include <QCloseEvent>
#include <QTableWidgetItem>

namespace Ui {
class SendElsDlg;
}

class SendElsDlg : public QDialog
{
    Q_OBJECT

public:
    explicit SendElsDlg(QWidget *parent = nullptr);
    ~SendElsDlg();
    void init(QString sch);

    void sendElsDataThread();

    void closeEvent(QCloseEvent *e);

    void AddMsg(QList<MsgBlock> list);

    void MessageInfo(const MsgBlock *par_msg,int ilen, int pos=0);

signals:
    void SendFinish(int msgid);
public slots:
    void slot_menu();

    void msgBtnClicked();

    void slotSendFinish(int index);
private slots:
    void on_tableWidget_customContextMenuRequested(const QPoint &pos);

    void on_tableWidget_itemClicked(QTableWidgetItem *item);

    void on_tableWidget_itemDoubleClicked(QTableWidgetItem *item);

public:
    QList<MsgBlock> m_blockList;
private:
    Ui::SendElsDlg *ui;
    QString m_sch;
    WTContrl *m_Pctrl=nullptr;
    QMenu m_CreateMsgMenu;
    QModelIndex m_CurModeIndex;

    QMenu m_ClearMenu;

};

#endif // SENDELSDLG_H
