#include "filter_proxy.h"

ProfilesFilterProxy::ProfilesFilterProxy(QObject *parent) : QSortFilterProxyModel(parent) {
    setFilterCaseSensitivity(Qt::CaseInsensitive);
    setSortCaseSensitivity(Qt::CaseInsensitive);
    setDynamicSortFilter(true);
}

void ProfilesFilterProxy::setNeedle(const QString &s) {
    m_needle = s.trimmed().toLower();
    invalidateFilter();
}

bool ProfilesFilterProxy::filterAcceptsRow(int source_row, const QModelIndex &source_parent) const {
    if (m_needle.isEmpty()) return true;
    auto idx = [&](int col){ return sourceModel()->index(source_row, col, source_parent); };
    for (int c=0;c<sourceModel()->columnCount();++c) {
        const QString v = sourceModel()->data(idx(c), Qt::DisplayRole).toString().toLower();
        if (v.contains(m_needle)) return true;
    }
    const QString grp = sourceModel()->data(sourceModel()->index(source_row, 0), Qt::UserRole+2).toString().toLower();
    if (grp.contains(m_needle)) return true;
    return false;
}
