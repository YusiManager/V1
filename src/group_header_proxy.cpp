#include "group_header_proxy.h"

GroupHeaderProxy::GroupHeaderProxy(QObject *parent) : QAbstractProxyModel(parent) {
    connect(this, &QAbstractItemModel::modelReset, this, &GroupHeaderProxy::rebuildCache);
}

void GroupHeaderProxy::setSourceModel(QAbstractItemModel *src) {
    QAbstractProxyModel::setSourceModel(src);
    if (!src) return;
    connect(src, &QAbstractItemModel::modelReset,     this, &GroupHeaderProxy::handleSourceReset);
    connect(src, &QAbstractItemModel::rowsInserted,   this, &GroupHeaderProxy::handleSourceReset);
    connect(src, &QAbstractItemModel::rowsRemoved,    this, &GroupHeaderProxy::handleSourceReset);
    connect(src, &QAbstractItemModel::dataChanged,    this, &GroupHeaderProxy::handleSourceReset);
    connect(src, &QAbstractItemModel::layoutChanged,  this, &GroupHeaderProxy::handleSourceReset);
    handleSourceReset();
}

QModelIndex GroupHeaderProxy::mapToSource(const QModelIndex &proxyIndex) const {
    if (!proxyIndex.isValid()) return {};
    if (isHeaderRow(proxyIndex.row())) return {};
    int srow = sourceRowForProxy(proxyIndex.row());
    return sourceModel()->index(srow, proxyIndex.column());
}

QModelIndex GroupHeaderProxy::mapFromSource(const QModelIndex &sourceIndex) const {
    if (!sourceIndex.isValid()) return {};
    int srow = sourceIndex.row();
    int add = 0;
    for (int hr : m_headerRows) if (hr <= srow + add) add++;
    return index(srow + add, sourceIndex.column());
}

int GroupHeaderProxy::rowCount(const QModelIndex &) const {
    if (!sourceModel()) return 0;
    return sourceModel()->rowCount() + m_headerRows.size();
}

int GroupHeaderProxy::columnCount(const QModelIndex &parent) const {
    Q_UNUSED(parent);
    if (!sourceModel()) return 0;
    return sourceModel()->columnCount();
}

QModelIndex GroupHeaderProxy::index(int row, int column, const QModelIndex &parent) const {
    if (parent.isValid()) return {};
    if (row < 0 || column < 0 || row >= rowCount() || column >= columnCount()) return {};
    return createIndex(row, column, nullptr);
}

QModelIndex GroupHeaderProxy::parent(const QModelIndex &) const { return {}; }

QVariant GroupHeaderProxy::data(const QModelIndex &idx, int role) const {
    if (!idx.isValid()) return {};
    if (isHeaderRow(idx.row())) {
        if (role == Qt::DisplayRole && idx.column()==ProfilesModel::ColName) {
            int h = m_headerRows.indexOf(idx.row());
            QString name = m_headerNames.value(h);
            if (name.trimmed().isEmpty()) name = QString::fromUtf8("— Ungrouped —");
            return name;
        }
        if (role == Qt::BackgroundRole) return QBrush(QColor(50,50,50,180));
        if (role == Qt::ForegroundRole) return QBrush(Qt::white);
        if (role == Qt::TextAlignmentRole) return int(Qt::AlignLeft | Qt::AlignVCenter);
        if (role == Qt::UserRole) return -1;
        if (role == Qt::UserRole+3) return true;
        return {};
    }
    if (!sourceModel()) return {};
    QModelIndex s = mapToSource(idx);
    QVariant v = sourceModel()->data(s, role);
    if (role == Qt::UserRole+3) return false;
    return v;
}

QVariant GroupHeaderProxy::headerData(int section, Qt::Orientation orientation, int role) const {
    return sourceModel() ? sourceModel()->headerData(section, orientation, role) : QVariant();
}

Qt::ItemFlags GroupHeaderProxy::flags(const QModelIndex &index) const {
    if (isHeaderRow(index.row())) return Qt::NoItemFlags;
    if (!sourceModel()) return Qt::NoItemFlags;
    return sourceModel()->flags(mapToSource(index));
}

bool GroupHeaderProxy::showUngroupedHeader() const { return m_showUngroupedHeader; }

void GroupHeaderProxy::setShowUngroupedHeader(bool on) {
    if (m_showUngroupedHeader == on) return;
    m_showUngroupedHeader = on;
    rebuildCache();
    beginResetModel();
    endResetModel();
}

bool GroupHeaderProxy::isHeaderRow(int proxyRow) const { return m_headerRows.contains(proxyRow); }

QString GroupHeaderProxy::headerName(int proxyRow) const {
    int i = m_headerRows.indexOf(proxyRow);
    return m_headerNames.value(i);
}

int GroupHeaderProxy::sourceRowForProxy(int proxyRow) const {
    int shift = 0;
    for (int hr : m_headerRows) {
        if (proxyRow > hr) shift++;
        else break;
    }
    return proxyRow - shift;
}

void GroupHeaderProxy::rebuildCache() {
    m_headerRows.clear();
    m_headerNames.clear();
    auto src = sourceModel();
    if (!src) return;

    const int n = src->rowCount();
    if (n <= 0) return;

    QVector<int> starts;
    QVector<QString> names;
    QString prev;
    for (int i=0;i<n;++i) {
        QModelIndex idx = src->index(i, 0);
        QString g = idx.data(Qt::UserRole+2).toString();
        if (i==0 || g.compare(prev, Qt::CaseInsensitive)!=0) {
            starts.push_back(i);
            names.push_back(g);
            prev = g;
        }
    }

    bool hasNamed = false;
    for (const auto &nm : names) if (!nm.trimmed().isEmpty()) { hasNamed = true; break; }

    int acc = 0;
    for (int i=0;i<starts.size();++i) {
        const QString g = names[i];
        if (!m_showUngroupedHeader && !hasNamed && g.trimmed().isEmpty()) continue;
        int proxyPos = starts[i] + acc;
        m_headerRows.push_back(proxyPos);
        m_headerNames.push_back(g);
        acc++;
    }
}

void GroupHeaderProxy::handleSourceReset() {
    beginResetModel();
    rebuildCache();
    endResetModel();
}
