#include "custommsgtablemodel.h"
#include<QDebug>
CustomMsgTableModel::CustomMsgTableModel(QObject *parent)
    : QAbstractTableModel(parent)
{
}

CustomMsgTableModel::~CustomMsgTableModel()
{
    m_vData.clear();
}

QVariant CustomMsgTableModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    // FIXME: Implement me!
    if(role ==Qt::DisplayRole && orientation ==Qt::Horizontal)
    {
        return m_sListHeader.at(section);
    }
     return QVariant();
}

int CustomMsgTableModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;
    if(m_vData.size() >0)
    {
         return m_vData.size();
    }
   return 0;
    // FIXME: Implement me!
}

int CustomMsgTableModel::columnCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;
    if(!m_sListHeader.isEmpty())
    {
        return m_sListHeader.size();
    }
    return 0;


    // FIXME: Implement me!
}

QVariant CustomMsgTableModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid())
        return QVariant();

    // FIXME: Implement me!

      if(role == Qt::DisplayRole)
      {
          if(m_vData.isEmpty())
          {
                return QVariant();
          }
          int iRow =index.row()%m_vData.size();
          if(m_vData.size() < iRow)
          {
              return QVariant();
          }
          QStringList rowDara = m_vData.at(iRow);
          return rowDara.at(index.column());
      }
    return QVariant();
}

void CustomMsgTableModel::setHearder(const QStringList &headerList)
{
    if(headerList.isEmpty())
    {
        return;
    }
    m_sListHeader = headerList;
}

void CustomMsgTableModel::updateData(const QVector<QStringList> &vdata)
{
    if(vdata.isEmpty())
    {
        return;
    }
  //  m_vData =vdata;

}

void CustomMsgTableModel::updateData(const QStringList &data)
{
    if(data.isEmpty() || data.size() != m_sListHeader.size())
    {
        return;
    }
    if(m_vData.size()>50)
    {
        m_vData.first().clear();
        m_vData.removeFirst();
    }
    m_vData.append(data);
}



void CustomMsgTableModel::reFlush()
{
  beginResetModel();
  endResetModel();
}

void CustomMsgTableModel::clearData()
{
    m_vData.clear();
}

void CustomMsgTableModel::moveRow(int index)
{
    if(m_vData.count()>0)
    {       
        m_vData.removeAt(index);
    }
}

QStringList CustomMsgTableModel::GetData(int row)
{
    QStringList qls;
    if(row<m_vData.count())
    {
        return m_vData.at(row);
    }
    return qls;
}
