#ifndef WID_REVIEWSET_H
#define WID_REVIEWSET_H

#include <QWidget>
#include "dlog_info.h"
#include <wtgl.h>
#include <IPluginV2.h>
#include <WTContrl.h>
#include <QSettings>
#include <QFileDialog>
#include <QTableWidgetItem>
#include "datadefine.h"
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
namespace Ui {
class wid_reviewset;
}

class wid_reviewset : public QWidget
{
    Q_OBJECT

public:
    explicit wid_reviewset(qint64 ipriID,QWidget *parent = nullptr);
    ~wid_reviewset();
private:
    void Init();
    void readIni();
    void setDataInfo(const QByteArray& data);//设置详细数据表格数据
    void updateDatainfo();
protected:
    void showEvent(QShowEvent *event);
public slots:
    void slot_itemClick(int row,int colum);
    void onItemchange(QTableWidgetItem* item);
private slots:
    void on_radioButton_2_clicked();

    void on_radioButton_3_clicked();

    void on_radioButton_4_clicked();

    void on_pushButton_clicked();

    void on_pushButton_3_clicked();

    void on_pushButton_2_clicked();

    void on_toolButton_choosePath_clicked();

    void on_comboBox_type_currentIndexChanged(int index);


    void on_pbtn_turnTo_clicked();

    void on_pbtn_FirstPage_clicked();

    void on_pbtn_up_clicked();

    void on_pbtn_down_clicked();

    void on_pbtn_end_clicked();

private:
    Ui::wid_reviewset *ui;
    qint64 mipriID;
    int mtype;
    int mclickRow = -1;
    QByteArray m_data;
    int kCols = 16;
    int kRowsPerPage = 50;
    int KItemsPerPage = 800;
    int m_currentPage = 0;
    int m_totalPage = 1;

//临时
    QList<MYDBSTRUCT> mDataList;
    void readData();
    QList<MYDBSTRUCT> getALLData();
    QSqlDatabase mdb;
    QString tname = "table_data_FCsource";

};

#endif // WID_REVIEWSET_H
