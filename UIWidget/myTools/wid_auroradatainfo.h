#ifndef WID_AURORADATAINFO_H
#define WID_AURORADATAINFO_H

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
class wid_AuroraDataInfo;
}

class wid_AuroraDataInfo : public QWidget
{
    Q_OBJECT

public:
    explicit wid_AuroraDataInfo(QWidget *parent = nullptr);
    ~wid_AuroraDataInfo();

private:
    Ui::wid_AuroraDataInfo *ui;
};

#endif // WID_AURORADATAINFO_H
