#include "ServiceTableModel.h"

ServiceTableModel::ServiceTableModel(QObject* parent)
    : QAbstractTableModel(parent) {
}

int ServiceTableModel::rowCount(const QModelIndex& parent) const {
    return m_services.size();
}

int ServiceTableModel::columnCount(const QModelIndex& parent) const {
    return 3; // Имя, Описание, Состояние
}

QVariant ServiceTableModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid() || index.row() >= m_services.size()) {
        return QVariant();
    }

    const auto& svc = m_services[index.row()];

    if (role == Qt::DisplayRole) 
    {
        switch (index.column()) {
        case 0:
            return svc.name;
        case 1:
            return svc.description.isEmpty() ? "N/A" : svc.description;
        case 2:
            return ServiceStatusString[svc.status];
        default:
            return QVariant();
        }
    }

    if (role == Qt::UserRole)
    {
        switch (index.column()) 
        {
        case 0:
            return svc.name;
        case 1:
            return svc.description.isEmpty() ? "N/A" : svc.description;
        case 2:
            return ServiceStatusString[svc.status];
        default:
            return QVariant();
        }
    }

    return QVariant();
}

QVariant ServiceTableModel::headerData(int section, Qt::Orientation orientation, int role) const {
    if (role == Qt::DisplayRole && orientation == Qt::Horizontal) {
        switch (section) {
        case 0:
            return "Name";
        case 1:
            return "Description";
        case 2:
            return "Status";
        default:
            return QVariant();
        }
    }
    return QAbstractTableModel::headerData(section, orientation, role);
}

void ServiceTableModel::updateData(const QList<ServiceInfo>& data) {
    beginResetModel();
    m_services = data;
    endResetModel();
}