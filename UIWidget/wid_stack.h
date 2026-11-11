#ifndef WID_STACK_H
#define WID_STACK_H

#include <QWidget>
#include <QMenu>
#include <QTableView>
#include <QFile>
#include <QTimer>
#include <QJsonDocument>
#include <QProcess>
#include <QJsonObject>
#include <QMutex>
#include <QTreeWidgetItem>
#include <QRandomGenerator>
#include "wid_rulwindow.h"
#include "wid_msgmonitor.h"
#include "wid_msgrecv.h"
#include "wid_datacontrol.h"
#include "wid_reviewset.h"
#include "wid_scp.h"
#include "wid_datamonitor.h"
#include "dlog_devadd.h"
#include "dlog_info.h"
#include <QStorageInfo>
#include <QCoreApplication>
#include <WTContrl.h>
#include <wtgl.h>
#include <IPluginV2.h>
#include <windows.h>
#include "dlog_editnet.h"
#include "tools/lhwm-cpp-wrapper.h"
#include "datadefine.h"
#include "myTools/cputemperaturereader.h"
#include "wid_datasource.h"
#include "wid_review.h"


namespace Ui {
class wid_stack;
}

class wid_stack : public QWidget
{
    Q_OBJECT

public:
    explicit wid_stack(qint64 ipriID,int id,QWidget *parent = nullptr);
    ~wid_stack();
    void updateDBTable();
protected:
    void closeEvent(QCloseEvent *event);
private:
    void InitUI(int id);
    void InitTree();
    void updateTree(int fcstatus);
    void updateATree(int astatus);
    double getFreeGB();
    void showInfo(QTreeWidgetItem *item);
    void InitInfo();
    double getTemp();
    bool launchCoreTempSilent();
    double readCpuTempSharedMem();
    int InitAuroraDev();
    void updateAuroraStatus();

    // 根据读取结果刷新温度文字和错误提示。
    void updateUi(const TemperatureReading &reading);
    // 按温度区间设置标签颜色：正常、偏热、过热。
    void setTemperatureColor(double celsius);

signals:
    void sig_topItemClick(bool top,int row);
    void sig_updateItem(int fcstatus);
    void sig_updateAItem(int astatus);
    void sig_FCOnline(int ch,int type);
    void sig_AuroraOnline(int ch,int type);
    void sig_updateFC(int ch,int type);
    void sig_updateAurora(int ch,int type);
    void sig_scpShow(QList<QString> list);
    void sig_scpData(QList<QString> list);
    void sig_ICDShow();
    void sig_ICDTShow();
public slots:
    void slot_TOPClick(bool top,int row);
    void slot_delItem();
    void slot_copyItem(QString name);
    void slot_addDev(DEVINFO info);
    void slot_updateitem(int fcstatus);
    void slot_updateAitem(int astatus);
    void updateTem();
    void updateMem();
    void checkLoad();
    void FClogin();
    void FCclose();
    void slot_FCOnline(int ch,int type);
    void slot_AuroraOnline(int ch,int type);
    void slot_scpShow(QList<QString> list);
    void slot_scpDs(QList<QString> list);
    void slot_updateICDtable();
    void slot_ICDShow();
    void slot_botspeed(int ch,int speed);
private slots:
    void onTreeContextMenu(const QPoint &pos);
    void onItemClicked(QTreeWidgetItem *item, int column);
    void on_pushButton_fix_clicked();

    void on_comboBox_fcspeed_currentIndexChanged(int index);

    void on_comboBox_FCType_currentIndexChanged(int index);

    void on_btn_open_clicked();

    void on_btn_open_2_clicked();

    void on_btn_open_3_clicked();

    void on_btn_fclogin_clicked();

    void on_comboBox_AuroraType_currentIndexChanged(int index);

    void on_comboBox_auroraspeed_currentIndexChanged(int index);

    void on_comboBox_loopmode_currentIndexChanged(int index);

private:
    Ui::wid_stack *ui;
    qint64 mipriID;
    wid_scp* mwid_scp;

    QMap<QTreeWidgetItem*,QMap<QString,QString>> m_devConfigMap;
    //    QMap<QTreeWidgetItem*,QMap<HANDLE,int>> map_dev;
    QMap<QString,MYHANDLE*> map_FCHandle;
    int AuroraID = -1;
    QMap<QString,int> map_status;// 1在线 0:离线
    QMap<QString,QTreeWidgetItem*> map_devItem;
    int m_id;

    QMutex fcmutex;
    QMutex auroramutex;


    wid_datacontrol* mwid_datacontrol;
    QTimer* timerMem;
    QTimer* timerTem;
    //FC
    QTimer* timerStatus;
    QTimer* timerLogin;
    QTimer* timerFCclose;
    //Aurora
    QTimer* timerAStatus;

    int delIndex = -1;
    int copyIndex = -1;
    bool isConnect = false;
    bool isAConnect = false;

    bool fcStart = false;
    bool auroaStart = false;
    bool netStart = false;

    //login
    bool m_FClogin = false;

    //temp
    CpuTemperatureReader* m_temperatureReader;
    bool m_coreTempStarted = false;

};

#endif // WID_STACK_H
