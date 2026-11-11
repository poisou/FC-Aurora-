#ifndef WID_VIEWWID_H
#define WID_VIEWWID_H

#include <QWidget>
#include <QDebug>
#include <WTContrl.h>
#include <wtgl.h>
#include <IPluginV2.h>
#include <QByteArray>
#include <QThread>
#include "myTools/mysliderwidget.h"
namespace Ui {
class wid_viewWid;
}

class wid_viewWid : public QWidget
{
    Q_OBJECT

public:
    explicit wid_viewWid(qint64 ipriID,QString text,int ch,QStringList ICDMap,QWidget *parent = nullptr);
    ~wid_viewWid();
    void setValue(QStringList rul,QStringList data);
    void setDataSource(QList<QString> list);
    void setICDSource(QList<QString> list);
    void setStatus(int status);
    void setJD(double v);
    void setTotal(int min,int s);
    void setNow(int min,int s,quint64 datalen);
signals:
    void sig_FCrcType(int ch,int type);
    void sig_AurorarcType(int ch,int type);
    void sig_FCthreadControl(int ch,int type,int rul,QString data,int status);
    void sig_AurorathreadControl(int ch,int type,int rul,QString data,int status);
    void sig_NetthreadControl(int ch,int type,int rul,QString data,int status);
    void sig_bbcset(int ch,int speed);
    void sig_botspeed(int ch,int speed);
    void sig_timestart(int ch,qint64 time);
public slots:
private slots:
    void snapToPresetValue(int currentValue);
private slots:
    void on_comboBox_SRType_currentIndexChanged(int index);

    void on_pbtn_run_clicked();

    void on_pbtn_pause_clicked();

    void on_pbtn_stop_clicked();

    void on_spinBox_valueChanged(int arg1);

    void on_comboBox_data_currentIndexChanged(int index);
    void on_comboBox_currentIndexChanged(int index);

private:
    void InitTimes();
private:
    qint64 mipriID;
    Ui::wid_viewWid *ui;
    QString stext;
    int mch;
    MySliderWidget* mslider;
    QList<qint64> presetValues;
    int showmode = 0;
};

#endif // WID_VIEWWID_H
