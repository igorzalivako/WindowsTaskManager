#include "ProcessTableProxyModel.h"

ProcessTableProxyModel::ProcessTableProxyModel(QObject* parent)
    : QSortFilterProxyModel(parent)
{
}

QVariant ProcessTableProxyModel::data(const QModelIndex& index, int role) const {
    if (role == Qt::BackgroundRole || role == Qt::ForegroundRole) {
        // Передаём роль в исходную модель
        return sourceModel()->data(mapToSource(index), role);
    }
    return QSortFilterProxyModel::data(index, role);
}