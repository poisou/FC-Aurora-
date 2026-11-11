#ifndef DLOG_DEVADD_H
#define DLOG_DEVADD_H

#include <QDialog>
#include <QMouseEvent>
#include "dlog_info.h"
#include <WTContrl.h>
#include <wtgl.h>
#include <IPluginV2.h>
#include "../WTBusInterfacepro/FC/WTFCSim/fcdatadefine.h"
#include "../WTBusInterfacepro/Aurora/WTAuroraSim/auroradatadefine.h"
namespace Ui {
class dlog_devadd;
}

class dlog_devadd : public QDialog
{
    Q_OBJECT

public:
    explicit dlog_devadd(QWidget *parent = nullptr);
    ~dlog_devadd();
protected:
    void mousePressEvent(QMouseEvent *event)override;
    void mouseMoveEvent(QMouseEvent *event)override;
    void mouseReleaseEvent(QMouseEvent *event)override;
    void mouseDoubleClickEvent(QMouseEvent *event)override;
signals:
    void sig_addDev(DEVINFO info);
private slots:
    void on_comboBox_currentIndexChanged(int index);

    void on_pushButton_2_clicked();

    void on_pushButton_clicked();

private:
    Ui::dlog_devadd *ui;
    bool m_bDragging = false;//拖动标志
    QPoint m_dragPosition;
};

#endif // DLOG_DEVADD_H
