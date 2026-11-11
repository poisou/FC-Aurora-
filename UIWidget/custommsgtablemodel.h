#ifndef CUSTOMMSGTABLEMODEL_H
#define CUSTOMMSGTABLEMODEL_H

#include <QAbstractTableModel>

class CustomMsgTableModel : public QAbstractTableModel
{
    Q_OBJECT

public:
    explicit CustomMsgTableModel(QObject *parent = nullptr);
    ~CustomMsgTableModel();
    // Header:
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;

    // Basic functionality:
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    void setHearder(const QStringList &headerList);
    void updateData(const QVector<QStringList> &vdata);
    void updateData(const QStringList &data);
    void reFlush();
    void clearData();
    void moveRow(int index);
    QStringList GetData(int row);
private:
    QStringList m_sListHeader;
    QList<QStringList>m_vData;
};

#endif // CUSTOMMSGTABLEMODEL_H
