#ifndef DLOG_EXPORT_H
#define DLOG_EXPORT_H

#include <QDialog>
#include <QMouseEvent>
#include <QFileDialog>
namespace Ui {
class dlog_export;
}

class dlog_export : public QDialog
{
    Q_OBJECT

public:
    explicit dlog_export(QStringList list_rul,QWidget *parent = nullptr);
    ~dlog_export();
protected:
    void mousePressEvent(QMouseEvent *event)override;
    void mouseMoveEvent(QMouseEvent *event)override;
    void mouseReleaseEvent(QMouseEvent *event)override;
    void mouseDoubleClickEvent(QMouseEvent *event)override;
signals:
    void sig_selectRul(QString savepath,int index);
private slots:
    void on_pushButton_2_clicked();

    void on_pushButton_clicked();

private:
    Ui::dlog_export *ui;
    bool m_bDragging = false;//拖动标志
    QPoint m_dragPosition;
};

#endif // DLOG_EXPORT_H
