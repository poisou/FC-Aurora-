#ifndef DLOG_EDITRUL_H
#define DLOG_EDITRUL_H

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
class dlog_editRul;
}

class dlog_editRul : public QDialog
{
    Q_OBJECT

public:
    explicit dlog_editRul(QString name,QVector<ICDDATA> vec_ICD,QWidget *parent = nullptr);
    ~dlog_editRul();
protected:
    void mousePressEvent(QMouseEvent *event)override;
    void mouseMoveEvent(QMouseEvent *event)override;
    void mouseReleaseEvent(QMouseEvent *event)override;
    void mouseDoubleClickEvent(QMouseEvent *event)override;

private slots:
    void on_pushButton_clicked();

    void on_pushButton_2_clicked();

    void on_pbtn_add_clicked();

private:
    void InitUI();
    void addItem(int idx);
    void InitTable(QString name,QVector<ICDDATA> vec_ICD);
private:
    Ui::dlog_editRul *ui;
    bool m_bDragging = false;//拖动标志
    QPoint m_dragPosition;
    QTableWidget* Table;

};

#endif // DLOG_EDITRUL_H
