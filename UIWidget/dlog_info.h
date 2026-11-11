#ifndef DLOG_INFO_H
#define DLOG_INFO_H

#include <QDialog>
#include <QMouseEvent>
namespace Ui {
class Dlog_info;
}

class Dlog_info : public QDialog
{
    Q_OBJECT

public:
    explicit Dlog_info(int id,QString text,QWidget *parent = nullptr);
    ~Dlog_info();
protected:
    void mousePressEvent(QMouseEvent *event)override;
    void mouseMoveEvent(QMouseEvent *event)override;
    void mouseReleaseEvent(QMouseEvent *event)override;
    void mouseDoubleClickEvent(QMouseEvent *event)override;
signals:
    void sig_delItem();
    void sig_copyItem(QString name);
private slots:
    void on_pushButton_clicked();
    void on_pushButton_2_clicked();

private:
    Ui::Dlog_info *ui;
    int mid;
    bool m_bDragging = false;//拖动标志
    QPoint m_dragPosition;
};

#endif // DLOG_INFO_H
