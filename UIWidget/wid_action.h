#ifndef WID_ACTION_H
#define WID_ACTION_H

#include <QWidget>
#include "dlog_info.h"
namespace Ui {
class wid_action;
}

class wid_action : public QWidget
{
    Q_OBJECT

public:
    explicit wid_action(int row,QString text,QWidget *parent = nullptr);
    ~wid_action();
signals:
    void sig_editICD(int mrow);
    void sig_delICD(int mrow);
private slots:
    void on_pushButton_clicked();

    void on_pushButton_2_clicked();
public slots:
    void slot_delICD();
private:
    Ui::wid_action *ui;
    int mrow;
    QString mtext;
};

#endif // WID_ACTION_H
