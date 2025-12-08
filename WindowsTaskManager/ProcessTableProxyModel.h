#pragma once

#include <QSortFilterProxyModel>

class ProcessTableProxyModel : public QSortFilterProxyModel 
{
    Q_OBJECT

public:
    explicit ProcessTableProxyModel(QObject* parent = nullptr);

protected:
    QVariant data(const QModelIndex& index, int role) const override;
};