#ifndef DLOG_ADDRUL_H
#define DLOG_ADDRUL_H

#include <QDialog>
#include <QMouseEvent>
#include <QTableWidget>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include "dlog_info.h"
#include "datadefine.h"
namespace Ui {
class dlog_addrul;
}

class dlog_addrul : public QDialog
{
    Q_OBJECT

public:
    explicit dlog_addrul(QWidget *parent = nullptr);
    ~dlog_addrul();
private:
    void InitUI();
    void addItem(int idx);
    void InitSql();
    void saveICD(QString path,QString name,QString type);
protected:
    void mousePressEvent(QMouseEvent *event)override;
    void mouseMoveEvent(QMouseEvent *event)override;
    void mouseReleaseEvent(QMouseEvent *event)override;
    void mouseDoubleClickEvent(QMouseEvent *event)override;
signals:
    void sig_addrul();
private slots:
    void on_pushButton_clicked();

    void on_pushButton_2_clicked();

    void on_comboBox_currentIndexChanged(int index);

    void on_pbtn_fcadd_clicked();

    void on_pbtn_netadd_clicked();

    void on_pbtn_auroraadd_clicked();

private:
    Ui::dlog_addrul *ui;
    int mid;
    bool m_bDragging = false;//拖动标志
    QPoint m_dragPosition;
    QTableWidget* Table;
    QSqlDatabase mdb;
};

#endif // DLOG_ADDRUL_H
