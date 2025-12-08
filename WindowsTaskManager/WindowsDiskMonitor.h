#pragma once

#include "IDiskMonitor.h"
#include <pdh.h>
#include <Windows.h>
#include <QList>
#include <QMap>

class WindowsDiskMonitor : public IDiskMonitor {
public:
    WindowsDiskMonitor();
    ~WindowsDiskMonitor();
    DisksInfo getDisksInfo() override;
    QMap<quint32, ProcessDiskInfo> getProcessDiskInfo() override;

private:
    // Для статистики дисков
    PDH_HQUERY _diskQuery = nullptr;
    QMap<QString, PDH_HCOUNTER> _diskCountersRead;
    QMap<QString, PDH_HCOUNTER> _diskCountersWrite;
    QMap<QString, quint64> _lastDiskReadBytes;
    QMap<QString, quint64> _lastDiskWriteBytes;
    qint64 _lastDiskUpdateTime = 0;

    // Для статистики процессов
    QMap<quint32, quint64> _lastProcessReadBytes;
    QMap<quint32, quint64> _lastProcessWriteBytes;
    qint64 _lastProcessUpdateTime = 0;

    // Вспомогательные
    QList<QString> getLogicalDriveStrings();
    quint64 getFileSystemFreeSpace(const QString& drive);
    quint64 getFileSystemTotalSpace(const QString& drive);

    // Инициализация PDH
    bool initDiskCounters();
};
