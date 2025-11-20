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

static inline int lerp(int a, int b, double t) {
    return static_cast<int>(std::lround(a + (b - a) * t));
}

static inline QColor performanceColor(int value)
{
    int v = std::clamp(value, 0, 100);

    const QColor Y(255, 255, 200); // light yellow
    const QColor O(255, 165, 0); // orange
    const QColor R(200, 0, 0); // deep red

    int r, g, b;
    if (v <= 50) {
        double t = v / 50.0; // 0..1
        r = lerp(Y.red(), O.red(), t);
        g = lerp(Y.green(), O.green(), t);
        b = lerp(Y.blue(), O.blue(), t);
    }
    else {
        double t = (v - 50) / 50.0; // 0..1
        r = lerp(O.red(), R.red(), t);
        g = lerp(O.green(), R.green(), t);
        b = lerp(O.blue(), R.blue(), t);
    }

    return QColor(r, g, b);
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

    if (role == Qt::BackgroundRole) {
        double value = 0;

        switch (index.column()) {
        case 2: // Загрузка ЦП (%)
            value = proc.cpuUsage;
            break;

        case 3: // Память (MB)
            value = proc.memoryUsage / 1024.0 / 1024.0 / 16.0 / 1024.0 * 100;
            break;

        case 4: // GPU
            value = proc.diskReadBytes / 1024.0 / 1024.0 / 1000.0 * 100;
            break;

        case 5: // Disk Read
            value = proc.diskWriteBytes / 1024.0 / 1024.0 / 1000.0 * 100;
            break;

        case 6:
            value = proc.gpuUsage;
            break;

        default:
            return QVariant(); // для других колонок не рисуем цвет
        }

        return performanceColor(value);
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

void ProcessTableModel::updateDataPartial(const QList<ProcessInfo>& newData) {
    if (_processes.isEmpty()) {
        // Первый запуск — просто заполняем
        updateData(newData);
        return;
    }

    // Создаём мапу по PID для быстрого поиска
    QHash<quint32, ProcessInfo> newMap;
    for (const auto& proc : newData) {
        newMap[proc.pid] = proc;
    }

    // Обновляем существующие
    for (int i = 0; i < _processes.size(); ++i) {
        quint32 pid = _processes[i].pid;
        if (newMap.contains(pid)) {
            // Проверим, изменились ли данные
            const ProcessInfo& newProc = newMap[pid];
            bool changed = false;

            if (_processes[i].cpuUsage != newProc.cpuUsage ||
                _processes[i].memoryUsage != newProc.memoryUsage ||
                _processes[i].name != newProc.name) {
                changed = true;
            }

            if (changed) {
                _processes[i] = newProc;
                emit dataChanged(index(i, 0), index(i, columnCount() - 1));
            }
        }
    }

    // Находим новые процессы
    QSet<quint32> oldPids;
    for (const auto& proc : _processes) {
        oldPids.insert(proc.pid);
    }

    QList<ProcessInfo> newProcesses;
    for (const auto& proc : newData) {
        if (!oldPids.contains(proc.pid)) {
            newProcesses.append(proc);
        }
    }

    // Добавляем новые
    if (!newProcesses.isEmpty()) {
        int oldSize = _processes.size();
        beginInsertRows(QModelIndex(), oldSize, oldSize + newProcesses.size() - 1);
        _processes.append(newProcesses);
        endInsertRows();
    }

    // Находим удалённые
    QSet<quint32> newPids;
    for (const auto& proc : newData) {
        newPids.insert(proc.pid);
    }

    QList<int> removedIndices;
    for (int i = _processes.size() - 1; i >= 0; --i) {
        if (!newPids.contains(_processes[i].pid)) {
            removedIndices.append(i);
        }
    }

    // Удаляем удалённые
    if (!removedIndices.isEmpty()) {
        // Сортируем индексы по убыванию
        std::sort(removedIndices.begin(), removedIndices.end(), std::greater<int>());

        for (int idx : removedIndices) {
            beginRemoveRows(QModelIndex(), idx, idx);
            _processes.removeAt(idx);
            endRemoveRows();
        }
    }
}