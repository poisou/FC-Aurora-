#ifndef TXCTRLDIALOG_H
#define TXCTRLDIALOG_H

#include <QDialog>

namespace Ui {
class TXCtrlDialog;
}

class TXCtrlDialog : public QDialog
{
    Q_OBJECT

public:
    explicit TXCtrlDialog(QWidget *parent = nullptr);
    ~TXCtrlDialog();
    bool isPer = false;
    int m_iTXCnt;           //!
    int m_iPerTm;           //!周期值
    QWidget *m_widget;
private slots:
    void on_radioButton_clicked();

    void on_radioButton_2_clicked();

    void on_pbnOK_clicked();
Q_SIGNALS:
    void setTitle(QString);
private:
    Ui::TXCtrlDialog *ui;
};

#endif // TXCTRLDIALOG_H
