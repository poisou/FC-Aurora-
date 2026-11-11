#ifndef WID_RUWINDOWL_H
#define WID_RUWINDOWL_H

#include <QWidget>
#include "wid_action.h"
#include "dlog_addrul.h"
#include "dlog_export.h"
#include <wtgl.h>
#include <IPluginV2.h>
#include <WTContrl.h>
#include <QFont>
#include <QDir>
#include <QBrush>
#include <QHBoxLayout>
#include <QTableWidgetItem>
#include <QXmlStreamWriter>
#include <QFileInfoList>
#include <QStringList>
#include <QJsonDocument>
#include <QJsonObject>
#include "datadefine.h"
#include "dlog_editrul.h"

#define TABLESHOWMAX 50
namespace Ui {
class wid_rulwindow;
}

class wid_rulwindow : public QWidget
{
    Q_OBJECT

public:
    explicit wid_rulwindow(qint64 ipriID,QWidget *parent = nullptr);
    ~wid_rulwindow();
private slots:
    void on_pbtn_inport_clicked();

    void on_pbtn_export_clicked();

    void on_pbtn_newrul_clicked();

    void on_pushButton_clicked();

    void on_pbtn_rulFirst_clicked();

    void on_pbtn_rulUp_clicked();

    void on_pbtn_rulDown_clicked();

    void on_pbtn_rulend_clicked();

    void on_pbtn_turn_clicked();

signals:
    void sig_updateICDtable();
public slots:
    void slot_delICD(int row);
    void slot_editICD(int row);
    void slot_exrul(QString savepath,int index);
    void slot_addrul();
private:
    void Init();
    void InitTable();
    void InitSql();
    void InitVec();
    void updateTable();
    QStringList findJsonFiles(const QString &floderPath);
    QJsonDocument readJsonFile(const  QString &filePath);
    bool copyFile(QString source,QString dst);
    void saveICD();//将map存储到ctrl
    void delICDTable(QString tableName);
    bool exportToXML(QVector<ICDDATA> vec,QString savePath,QString name);
    void saveICD(QString path,QString name,QString type,QVector<ICDDATA> data);
private:
    Ui::wid_rulwindow *ui;
    qint64 mipriID;
    QSqlDatabase mdb;
    QStringList FileList;//存储ICD文件路径下所有的ICD文件名
    QMap<QString,QString> m_ICDPath;//存储文件路径和对应的ICD格式  例如FC_1.json和其中的解析ICD
    QMap<QString,QVector<ICDDATA>> map_ICDData;

    //分页相关参数
    int m_PageSize = 10;
    int m_currentPage = 0;
    int m_totalPage = 1;
};

#endif // WID_RUWINDOWL_H
