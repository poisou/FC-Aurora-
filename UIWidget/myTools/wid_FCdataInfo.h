#ifndef WID_FCDATAINFO_H
#define WID_FCDATAINFO_H

#include <QWidget>
#include <QRandomGenerator>
#include <QHeaderView>
#include <QTableWidgetItem>
#include <WTContrl.h>
#include <wtgl.h>
#include <IPluginV2.h>
#include "../WTBusInterfacepro/FC/WTFCSim/WTAPI_TE_DSIM.h"
#include <QRegularExpressionValidator>
#include "../datadefine.h"
#include <QSpinBox>


namespace Ui {
class wid_FCdataInfo;
}

class wid_FCdataInfo : public QWidget
{
    Q_OBJECT

public:
    explicit wid_FCdataInfo(QWidget *parent = nullptr);
    ~wid_FCdataInfo();
    WTAPI_TE_DSIM_SEND_HEAD* getWTDSIMHeader();
    WTAPI_TE_DSIM_FCHEAD* getWTFCHeader();
    QByteArray getData();
    int getStart();
    int getStep();
    int getGroups();
    int getbbcValue();
    void setVis();
private slots:
    void on_pushButton_creat_clicked();
    void on_pushButton_clear_clicked();
    void on_comboBox_type_currentIndexChanged(int index);
    void on_comboBox_time_currentIndexChanged(int index);
public slots:
    void onItemchange(QTableWidgetItem* item);
private:
    void Init();
    void setupValidtors(QLineEdit* edit,int maxHexChars,bool isMSGl = false);
    void bindSpandLineedit(QSpinBox* spin,QLineEdit* edit,int hexwidth);
private:
    Ui::wid_FCdataInfo *ui;

};

#endif // WID_FCDATAINFO_H
