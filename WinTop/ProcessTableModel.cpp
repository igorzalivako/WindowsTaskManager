#include "ProcessTableModel.h"
#include <QApplication>

const int COLUMNS_COUNT = 7;

enum ProcessTableColumns { ptcPID, ptcName, ptcCPUUsage, ptcMemoryUsage, ptcDiskReadBytes, ptcDiskWriteBytes, ptcGPUUsage };

ProcessTableModel::ProcessTableModel(QObject* parent)
    : QAbstractTableModel(parent) 
{
}

int ProcessTableModel::rowCount(const QModelIndex& parent) const 
{
    return _processes.size();
}

int ProcessTableModel::columnCount(const QModelIndex& parent) const 
{
    return COLUMNS_COUNT; // PID, Name, CPU, Memory, Disk Read, Disk Write, GPUUsage
}

QVariant ProcessTableModel::data(const QModelIndex& index, int role) const 
{
    if (!index.isValid() || index.row() >= _processes.size()) 
    {
        return QVariant();
    }

    const auto& proc = _processes[index.row()];

    if (role == Qt::DisplayRole) 
    {
        switch (index.column()) 
        {
        case ptcPID: return proc.pid;
        case ptcName: return proc.name;
        case ptcCPUUsage: return QString::number(proc.cpuUsage, 'f', 2) + "%";
        case ptcMemoryUsage: return QString::number(proc.memoryUsage / 1024 / 1024) + " MB";
        case ptcDiskReadBytes: return QString::number(proc.diskReadBytes / 1024 / 1024) + " MB";
        case ptcDiskWriteBytes: return QString::number(proc.diskWriteBytes / 1024 / 1024) + " MB";
        case ptcGPUUsage: return QString::number(proc.gpuUsage) + "%";
        default: return QVariant();
        }
    }
    // Для сортировки
    if (role == Qt::UserRole)
    {
        switch (index.column()) 
        {
        case ptcPID: return proc.pid;
        case ptcName: return proc.name;
        case ptcCPUUsage: return proc.cpuUsage;
        case ptcMemoryUsage: return proc.memoryUsage;
        case ptcDiskReadBytes: return proc.diskReadBytes;
        case ptcDiskWriteBytes: return proc.diskWriteBytes;
        case ptcGPUUsage: return proc.gpuUsage;
        default: return QVariant();
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
        case ptcPID: return "PID";
        case ptcName: return "Name";
        case ptcCPUUsage: return "CPU %";
        case ptcMemoryUsage: return "RAM (MB)";
        case ptcDiskReadBytes: return "Disk Read (MB)";
        case ptcDiskWriteBytes: return "Disk Write (MB)";
        case ptcGPUUsage: return "GPU %";
        default: return QVariant();
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