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
    QList<DiskInfo> getDiskInfo() override;
    QMap<quint32, ProcessDiskInfo> getProcessDiskInfo() override;

private:
    // Для статистики дисков
    PDH_HQUERY m_diskQuery = nullptr;
    QMap<QString, PDH_HCOUNTER> m_diskCountersRead;
    QMap<QString, PDH_HCOUNTER> m_diskCountersWrite;
    QMap<QString, quint64> m_lastDiskReadBytes;
    QMap<QString, quint64> m_lastDiskWriteBytes;
    qint64 m_lastDiskUpdateTime = 0;

    // Для статистики процессов
    QMap<quint32, quint64> m_lastProcessReadBytes;
    QMap<quint32, quint64> m_lastProcessWriteBytes;
    qint64 m_lastProcessUpdateTime = 0;

    // Вспомогательные
    QList<QString> getLogicalDriveStrings();
    quint64 getFileSystemFreeSpace(const QString& drive);
    quint64 getFileSystemTotalSpace(const QString& drive);

    // Инициализация PDH
    bool initDiskCounters();
};
