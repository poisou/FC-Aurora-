#ifndef WID_DATAMONITOR_H
#define WID_DATAMONITOR_H

#include <QWidget>

namespace Ui {
class wid_datamonitor;
}

class wid_datamonitor : public QWidget
{
    Q_OBJECT

public:
    explicit wid_datamonitor(qint64 ipriID,QWidget *parent = nullptr);
    ~wid_datamonitor();
private:
    void Init();
private:
    Ui::wid_datamonitor *ui;
    qint64 mipriID;
};

#endif // WID_DATAMONITOR_H
