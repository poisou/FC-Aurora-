#ifndef DLOG_EDITNET_H
#define DLOG_EDITNET_H

#include <QDialog>
#include <QMouseEvent>
namespace Ui {
class dlog_editnet;
}

class dlog_editnet : public QDialog
{
    Q_OBJECT

public:
    explicit dlog_editnet(QWidget *parent = nullptr);
    ~dlog_editnet();
private:
    void Init();
private slots:
    void on_pushButton_2_clicked();

    void on_pushButton_clicked();
protected:
    void mousePressEvent(QMouseEvent *event)override;
    void mouseMoveEvent(QMouseEvent *event)override;
    void mouseReleaseEvent(QMouseEvent *event)override;
    void mouseDoubleClickEvent(QMouseEvent *event)override;
private:
    Ui::dlog_editnet *ui;
    bool m_bDragging = false;//拖动标志
    QPoint m_dragPosition;
};

#endif // DLOG_EDITNET_H
