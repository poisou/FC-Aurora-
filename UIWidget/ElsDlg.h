#ifndef ELSDLG_H
#define ELSDLG_H

#include <QDialog>
#include <WTContrl.h>
#include <wtgl.h>
#include <IPluginV2.h>
#include <CMessageBlock.h>
#include <QLineEdit>
#include <QTreeWidgetItem>

#define OFFSET 72
namespace Ui {
class ElsDlg;
}

class ElsDlg : public QDialog
{
    Q_OBJECT

public:
    explicit ElsDlg(QWidget *parent = nullptr);
    void initUI(QString sch, MsgBlock cMsgBlock, int type=0); //type 0 新建，1 修改
    void initUI(QString sch, QList<MsgBlock> list, int mode, int index);
    ~ElsDlg();

    void addBlockTree();

    void updateBlockTree();

private slots:
    void on_pbnReq_clicked();

    void on_spinBox_valueChanged(int arg1);

    void on_pbnNew_clicked();

    void on_pbnNew_5_clicked();

    void on_pbnNew_2_clicked();

    void on_treeWidget_itemClicked(QTreeWidgetItem *item, int column);

    void on_pbnNew_3_clicked();

public slots:
    void EditData(int len ,char*data);

signals:
    void sigInit(int,char*);
private:
    Ui::ElsDlg *ui;
    QString m_sch;
    WTContrl *m_pCtrl;
    //MsgBlock m_cMsgBlock;
    QList<QLineEdit*> LineEditList;
    unsigned char* m_dataBuffer;

public:
    MsgBlock m_cMsgBlock;

    QList<MsgBlock> m_BlockList;

    bool m_mode=0;    //0新建 1修改
    int m_index=0;
};

#endif // ELSDLG_H
