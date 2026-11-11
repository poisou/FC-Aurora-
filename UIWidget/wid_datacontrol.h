#ifndef WID_DATACONTROL_H
#define WID_DATACONTROL_H

#include <QWidget>
#include "dlog_datacreat.h"
#include "dlog_info.h"
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QDate>
#include <QDateTime>
#include <algorithm>
#include <WTContrl.h>
#include <wtgl.h>
#include <IPluginV2.h>
#include "datadefine.h"
#include <QFileDialog>

#define MAXDATAROW 131

namespace Ui {
class wid_datacontrol;
}

class wid_datacontrol : public QWidget
{
    Q_OBJECT

public:
    explicit wid_datacontrol(qint64 ipriID,QWidget *parent = nullptr);
    ~wid_datacontrol();
    void updateTable();//更新界面数据源列表
private slots:
    void on_pushButton_2_clicked();
    void slot_back();
    void on_pushButton_del_clicked();
    void on_pushButton_upload_clicked();
    void on_comboBox_type_currentIndexChanged(int index);

    void on_pushButton_modify_clicked();

    void on_pbtn_infoUp_clicked();

    void on_pbtn_infoDown_clicked();

    void on_pbtn_infoFirst_clicked();

    void on_pbtn_infoend_clicked();

    void on_pbtn_turn_clicked();

    void on_pbtn_dataFirst_clicked();

    void on_pbtn_dataUp_clicked();

    void on_pbtn_dataDown_clicked();

    void on_pbtn_dataEnd_clicked();

    void on_pbtn_dataTurn_clicked();

signals:
    void sig_scpData(QList<QString> list);

public slots:
    void onItemchange(QTableWidgetItem* item);
    void slot_itemClick(int row,int colum);
    void slot_delItem();
    void slot_saveFCData(QString name);
private:
    void Init();
    void InitSql();
    void readData();
    QList<MYDBSTRUCT> getALLData();
    QPair<QDateTime,QDateTime> getTimeRange(QList<QDateTime> dateTimes);
    void setDataInfo();//设置详细数据表格数据
    QByteArray getData();
    void once();
private:
    Ui::wid_datacontrol *ui;
    qint64 mipriID;
    QSqlDatabase mdb;
    QString tname = "table_data_FCsource";
    //数据详细显示
     QByteArray m_data;
     int kCols = 16;
     int kRowsPerPage = 50;
     int KItemsPerPage = 800;
     int m_currentPage = 0;
     int m_totalPage = 1;
    //数据库显示数据信息
    QList<MYDBSTRUCT> mDataList;
    int dm_PageSize = 10;
    int dm_currentPage = 0;
    int dm_totalPage = 1;

    dlog_datacreat* mlog_cd;
    int mclickRow = -1;
    QString sname;
};

#endif // WID_DATACONTROL_H
