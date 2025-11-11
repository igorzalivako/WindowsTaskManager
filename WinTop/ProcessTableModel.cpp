#include "ProcessTableModel.h"
#include <QApplication>

const int COLUMNS_COUNT = 6;

ProcessTableModel::ProcessTableModel(QObject* parent)
    : QAbstractTableModel(parent) {
}

int ProcessTableModel::rowCount(const QModelIndex& parent) const {
    return _processes.size();
}

int ProcessTableModel::columnCount(const QModelIndex& parent) const {
    return COLUMNS_COUNT; // PID, Name, CPU, Memory, Disk Read, Disk Write, Network In, Network Out
}

QVariant ProcessTableModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid() || index.row() >= _processes.size()) 
    {
        return QVariant();
    }

    const auto& proc = _processes[index.row()];

    if (role == Qt::DisplayRole) 
    {
        switch (index.column()) 
        {
        case 0:
            return proc.pid;
        case 1:
            return proc.name;
        case 2:
            return QString::number(proc.cpuUsage, 'f', 2) + "%";
        case 3:
            return QString::number(proc.memoryUsage / 1024 / 1024) + " MB";
        case 4: return QString::number(proc.diskReadBytes / 1024 / 1024) + " MB";
        case 5: return QString::number(proc.diskWriteBytes / 1024 / 1024) + " MB";
        default:
            return QVariant();
        }
    }
    // Для сортировки
    if (role == Qt::UserRole)
    {
        switch (index.column()) 
        {
        case 0: // PID
            return proc.pid;
        case 1:
            return proc.name;
        case 2: // CPU %
            return proc.cpuUsage;
        case 3: // Memory (MB)
            return proc.memoryUsage;
        case 4: return proc.diskReadBytes; 
        case 5: return proc.diskWriteBytes; 
        default:
            return QVariant();
        }
    }

    if (role == Qt::DecorationRole && index.column() == 1) { // колонка с именем
        if (_processControl) {
            // Проверяем кэш
            if (_iconCache.contains(proc.pid)) {
                return _iconCache[proc.pid];
            }

            // Получаем иконку и кэшируем
            QIcon icon = _processControl->getProcessIcon(proc.pid);
            _iconCache[proc.pid] = icon;
            return icon;
        }
    }

    return QVariant();
}

QVariant ProcessTableModel::headerData(int section, Qt::Orientation orientation, int role) const {
    if (role == Qt::DisplayRole && orientation == Qt::Horizontal) {
        switch (section) {
        case 0:
            return "PID";
        case 1:
            return "Name";
        case 2:
            return "CPU %";
        case 3:
            return "RAM (MB)";
        case 4: return "Disk Read (MB)";
        case 5: return "Disk Write (MB)";
        default:
            return QVariant();
        }
    }
    return QAbstractTableModel::headerData(section, orientation, role);
}

ProcessInfo ProcessTableModel::getProcessByRow(int row) const
{
    if (row < 0 || row >= _processes.size()) {
        return ProcessInfo{};
    }
    return _processes[row];
}

void ProcessTableModel::setProcessControl(IProcessControl* control)
{
    _processControl = control;
}

void ProcessTableModel::updateData(const QList<ProcessInfo>& data) {
    beginResetModel();
    _processes = data;
    _iconCache.clear();
    endResetModel();
}