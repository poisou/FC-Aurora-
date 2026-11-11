#ifndef DLOG_FCDATAINFO_H
#define DLOG_FCDATAINFO_H

#include <QDialog>
#include <QMouseEvent>
#include <QTableWidget>
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
class dlog_FCdatainfo;
}

class dlog_FCdatainfo : public QDialog
{
    Q_OBJECT

public:
    explicit dlog_FCdatainfo(QWidget *parent = nullptr);
    ~dlog_FCdatainfo();
protected:
    void mousePressEvent(QMouseEvent *event)override;
    void mouseMoveEvent(QMouseEvent *event)override;
    void mouseReleaseEvent(QMouseEvent *event)override;
    void mouseDoubleClickEvent(QMouseEvent *event)override;
public slots:
    void onItemchange(QTableWidgetItem* item);
private slots:
    void on_pushButton_2_clicked();

    void on_pushButton_clicked();

    void on_pushButton_creat_clicked();

    void on_pushButton_clear_clicked();

    void on_comboBox_type_currentIndexChanged(int index);

private:
    void Init();
    void setupValidtors(QLineEdit* edit,int maxHexChars,bool isMSGl = false);
    void bindSpandLineedit(QSpinBox* spin,QLineEdit* edit,int hexwidth);

private:
    Ui::dlog_FCdatainfo *ui;
    bool m_bDragging = false;//拖动标志
    QPoint m_dragPosition;
};

#endif // DLOG_FCDATAINFO_H
