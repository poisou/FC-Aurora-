#ifndef FCNETFORM_H
#define FCNETFORM_H

#include <QDialog>
#include <QMap>
#include <QTimer>

namespace Ui {
class FCNETForm;
}

class FCNETForm : public QDialog
{
    Q_OBJECT

public:
    explicit FCNETForm(QWidget *parent = nullptr);
    ~FCNETForm();
    QMap<QString,QString>mapCmd;
    QMap<int,QWidget*>MapStatus;
    void Init(QString sch,QWidget *parent);
    void updateID(QString PortID);
        QWidget *m_parent;
    void stopWDT();
    void startWDT();
public slots:
    void slot_ToolButton();
    void slot_ToolButton1();
    void MousePress(QString);
    void SlotTimer();
private slots:
    void on_pushButton_clicked();
    void on_cmBox_Channels_currentIndexChanged(const QString &arg1);

    void on_comboBox_currentIndexChanged(int index);

    void on_comboBox_currentIndexChanged(const QString &arg1);

    void on_lineEdit_11_textChanged(const QString &arg1);

    void on_toolButton_clicked();

Q_SIGNALS:
    void setTitle(QString);
private:
    Ui::FCNETForm *ui;
    QTimer *m_pTimer;
    bool b_InitMsg = false;
    QString m_sCh;
};

#endif // FCNETFORM_H
