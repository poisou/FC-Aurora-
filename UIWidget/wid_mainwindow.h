#ifndef WID_MAINWINDOW_H
#define WID_MAINWINDOW_H

#include <QWidget>
#include <QTimer>
#include <QDateTime>
#include <QTabWidget>
#include <QTabBar>
#include <QTreeWidgetItem>
#include <windows.h>
#include <WTContrl.h>
#include <wtgl.h>
#include <IPluginV2.h>
#include <psapi.h>
#include "wid_stack.h"
#include "datadefine.h"

#pragma comment(lib,"psapi.lib")

namespace Ui {
class wid_mainwindow;
}

class wid_mainwindow : public QWidget
{
    Q_OBJECT

public:
    explicit wid_mainwindow(qint64 ipriID,QWidget *parent = nullptr);
    ~wid_mainwindow();
signals:
    void sig_TOPClick(bool top,int row);
    void sig_update(int fcstatus);
    void sig_updateA(int astatus);
public slots:
    void updateDate();
    void slot_topItemClick(bool top,int row);
    void slot_update(int fcstatus);
    void slot_updateA(int astatus);
private:
    void Init();
    void InitUI();
    void InitTimer();
    void InitDev();
private:
    Ui::wid_mainwindow *ui;
    qint64 mipriID;
    QTimer* date_timer = nullptr;

    wid_stack* mwid_dev = nullptr;//设备管理
    wid_stack* mwid_rul = nullptr;//协议规则
    wid_stack* mwid_msgmonitor = nullptr;//报文解析
    wid_stack* mwid_datacontrol = nullptr;//数据源管理
    wid_stack* mwid_reviewset = nullptr;//回放配置
    wid_stack* mwid_scp = nullptr;//场景并行
    wid_stack* mwid_datamonitor = nullptr;//数据监控
};

#endif // WID_MAINWINDOW_H
