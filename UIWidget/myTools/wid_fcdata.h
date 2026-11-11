#ifndef WID_FCDATA_H
#define WID_FCDATA_H

#include <QWidget>
#include <QHeaderView>
#include <QTableWidgetItem>
#include <QSpinBox>
#include "dlog_fcdatainfo.h"
#include "../wid_action.h"
#include "../datadefine.h"

namespace Ui {
class wid_FCData;
}

class wid_FCData : public QWidget
{
    Q_OBJECT

public:
    explicit wid_FCData(QWidget *parent = nullptr);
    ~wid_FCData();
private slots:
    void on_pushButton_creat_clicked();

    void on_pushButton_up_clicked();

    void on_pushButton_down_clicked();

    void on_checkBox_times_stateChanged(int arg1);

    void on_pbtn_tunTo_clicked();

public slots:
    void slot_delICD(int row);
    void slot_editICD(int row);
private:
    void Init();
    void CreatData(int row);
    void updateTable();
    void updateTAB();
private:
    Ui::wid_FCData *ui;
    QVector<CREATFCDATA> vec_data;

    //分页相关参数
    int m_PageSize = 50;
    int m_currentPage = 0;
    int m_totalPage = 1;
};

#endif // WID_FCDATA_H
