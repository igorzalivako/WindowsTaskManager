#pragma once

#include <QAbstractTableModel>
#include <QList>
#include "IServiceMonitor.h"

class ServiceTableModel : public QAbstractTableModel {
    Q_OBJECT

public:
    explicit ServiceTableModel(QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    int columnCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;

    void updateData(const QList<ServiceInfo>& data);

private:
    QList<ServiceInfo> m_services;
};