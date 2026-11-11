#ifndef WID_MSGRECV_H
#define WID_MSGRECV_H

#include <QWidget>

namespace Ui {
class wid_msgrecv;
}

class wid_msgrecv : public QWidget
{
    Q_OBJECT

public:
    explicit wid_msgrecv(qint64 ipriID,QWidget *parent = nullptr);
    ~wid_msgrecv();

private slots:
    void on_pushButton_clicked();

private:
    Ui::wid_msgrecv *ui;
    qint64 mipriID;
};

#endif // WID_MSGRECV_H
