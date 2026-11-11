#ifndef DLOG_DATACREAT_H
#define DLOG_DATACREAT_H

#include <QWidget>
#include <QDialog>
#include <QMouseEvent>
#include "myTools/wid_fcdata.h"
#include "myTools/wid_FCdataInfo.h"
#include "myTools/wid_auroradatainfo.h"

#define MAXDATA (2148)
namespace Ui {
class dlog_datacreat;
}

class dlog_datacreat : public QWidget
{
    Q_OBJECT

public:
    explicit dlog_datacreat(QWidget *parent = nullptr);
    ~dlog_datacreat();
    WTAPI_TE_DSIM_SEND_HEAD* getWTDSIMHeader();
    WTAPI_TE_DSIM_FCHEAD* getWTFCHeader();
    QByteArray getData();
    int getFCStart();
    int getFCStep();
    int getFCGroups();
    int getbbc();
private:
    void Init();
signals:
    void sig_addDatadb(QString name,int type);
    void sig_back();
    void sig_saveFCData(QString name);
private slots:
    void on_pushButton_clicked();

    void on_comboBox_type_currentIndexChanged(int index);

    void on_pushButton_return_clicked();

private:
    Ui::dlog_datacreat *ui;
    wid_FCdataInfo* mFCEdit;
    wid_FCData* mFCedit;
    wid_AuroraDataInfo* mAuroraEdit;
    wid_FCdataInfo* mFCEdit3;

    WTAPI_TE_DSIM_SEND_HEAD* mWTFChead;//wt头
    WTAPI_TE_DSIM_FCHEAD* mFCHead;//fc头
    QByteArray mFCData;
    int mFCStart;
    int mFCStep;
    int mFCgroups;
    int bbc;
};

#endif // DLOG_DATACREAT_H
