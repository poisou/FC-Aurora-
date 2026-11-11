#ifndef CDATAEDIT_H
#define CDATAEDIT_H

#include <QWidget>
#include <QLineEdit>

namespace Ui {
class cDataEdit;
}

class cDataEdit : public QWidget
{
    Q_OBJECT

public:
    explicit cDataEdit(QWidget *parent = nullptr);
    ~cDataEdit();
    QVector<QLineEdit*>qvec;
    QByteArray m_Ardata;

    int m_ilen;
    int m_iPage;
    int m_iBLen;
public slots:
    void Init(int,char*);
Q_SIGNALS:
    void sigData(int,char*);
private slots:
    void on_comboBox_currentIndexChanged(int index);

    void on_comboBox_2_currentIndexChanged(int index);

    void on_pushButton_clicked();

    void on_comboBox_3_currentIndexChanged(int index);

    void on_lineEdit_textEdited(const QString &arg1);

    void on_lineEdit_5_editingFinished();

    void on_pbnOK_clicked();

private:
    Ui::cDataEdit *ui;

    void InitOneByte(int len,char*data);
    void InitTwoByte(int len,char*data);
    void InitfourByte(int len,char*data);
};

#endif // CDATAEDIT_H
