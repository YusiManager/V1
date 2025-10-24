#pragma once
#include <QtCore>
#include <QtGui>
#include <QtWidgets>
#include "profiles_model.h"

class GroupHeaderProxy : public QAbstractProxyModel {
    Q_OBJECT
public:
    explicit GroupHeaderProxy(QObject *parent=nullptr);
    void setSourceModel(QAbstractItemModel *sourceModel) override;
    QModelIndex mapToSource(const QModelIndex &proxyIndex) const override;
    QModelIndex mapFromSource(const QModelIndex &sourceIndex) const override;
    int rowCount(const QModelIndex &parent=QModelIndex()) const override;
    int columnCount(const QModelIndex &parent=QModelIndex()) const override;
    QModelIndex index(int row, int column, const QModelIndex &parent=QModelIndex()) const override;
    QModelIndex parent(const QModelIndex &child) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role) const override;
    Qt::ItemFlags flags(const QModelIndex &index) const override;
    bool showUngroupedHeader() const;
    void setShowUngroupedHeader(bool on);
    bool isHeaderRow(int proxyRow) const;
    QString headerName(int proxyRow) const;
    int sourceRowForProxy(int proxyRow) const;

private:
    QVector<int> m_headerRows;
    QVector<QString> m_headerNames;
    bool m_showUngroupedHeader = true;
    void rebuildCache();
private slots:
    void handleSourceReset();
};
