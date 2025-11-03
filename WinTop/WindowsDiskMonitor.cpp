#include "WindowsDiskMonitor.h"

#include <QDir>
#include <QDateTime>
#include <QDebug>
#include <windows.h>
#include <TlHelp32.h>

WindowsDiskMonitor::WindowsDiskMonitor() {
    initDiskCounters();
}

WindowsDiskMonitor::~WindowsDiskMonitor() {
    if (m_diskQuery) {
        PdhCloseQuery(m_diskQuery);
    }
}

bool WindowsDiskMonitor::initDiskCounters() {
    if (PdhOpenQuery(NULL, 0, &m_diskQuery) != ERROR_SUCCESS) {
        return false;
    }

    QList<QString> drives = getLogicalDriveStrings();
    for (const QString& drive : drives) {
        QString driveLetter = drive.left(1); // "C:"
        QString counterPath = QString("\\PhysicalDisk(0 %1:)\\Disk Read Bytes/sec").arg(driveLetter);
        PDH_HCOUNTER counterRead;
        if (PdhAddCounterW(m_diskQuery, reinterpret_cast<LPCWSTR>(counterPath.utf16()), 0, &counterRead) == ERROR_SUCCESS) {
            m_diskCountersRead[drive] = counterRead;
        }

        counterPath = QString("\\PhysicalDisk(0 %1:)\\Disk Write Bytes/sec").arg(driveLetter);
        PDH_HCOUNTER counterWrite;
        if (PdhAddCounterW(m_diskQuery, reinterpret_cast<LPCWSTR>(counterPath.utf16()), 0, &counterWrite) == ERROR_SUCCESS) {
            m_diskCountersWrite[drive] = counterWrite;
        }
    }

    // Collect initial data
    PdhCollectQueryData(m_diskQuery);
    m_lastDiskUpdateTime = QDateTime::currentMSecsSinceEpoch();

    return true;
}

QList<DiskInfo> WindowsDiskMonitor::getDiskInfo() {
    QList<DiskInfo> disks;

    qint64 currentTime = QDateTime::currentMSecsSinceEpoch();
    double elapsedSec = (currentTime - m_lastDiskUpdateTime) / 1000.0;
    m_lastDiskUpdateTime = currentTime;

    // Collect data
    if (PdhCollectQueryData(m_diskQuery) != ERROR_SUCCESS) {
        return disks;
    }

    for (const QString& drive : getLogicalDriveStrings()) {
        DiskInfo info;
        info.name = drive;
        info.totalBytes = getFileSystemTotalSpace(drive);
        info.freeBytes = getFileSystemFreeSpace(drive);

        // Get raw values
        PDH_FMT_COUNTERVALUE valueRead, valueWrite;
        PDH_HCOUNTER counterRead = m_diskCountersRead.value(drive, nullptr);
        PDH_HCOUNTER counterWrite = m_diskCountersWrite.value(drive, nullptr);

        if (counterRead && PdhGetFormattedCounterValue(counterRead, PDH_FMT_LARGE, NULL, &valueRead) == ERROR_SUCCESS) {
            quint64 currentRead = valueRead.largeValue;
            quint64 lastRead = m_lastDiskReadBytes.value(drive, 0);
            info.readBytesPerSec = (currentRead - lastRead) / elapsedSec;
            if (elapsedSec > 0) {
                info.readBytesPerSec = currentRead; // PDH уже возвращает rate
            }
            else {
                info.readBytesPerSec = 0;
            }
            m_lastDiskReadBytes[drive] = currentRead;
        }

        if (counterWrite && PdhGetFormattedCounterValue(counterWrite, PDH_FMT_LARGE, NULL, &valueWrite) == ERROR_SUCCESS) {
            quint64 currentWrite = valueWrite.largeValue;
            quint64 lastWrite = m_lastDiskWriteBytes.value(drive, 0);
            info.writeBytesPerSec = (currentWrite - lastWrite) / elapsedSec;
            if (elapsedSec > 0) {
                info.writeBytesPerSec = currentWrite; // PDH уже возвращает rate
            }
            else {
                info.writeBytesPerSec = 0;
            }
            m_lastDiskWriteBytes[drive] = currentWrite;
        }

        info.ioBytesPerSec = info.readBytesPerSec + info.writeBytesPerSec;

        disks.append(info);
    }

    return disks;
}

QMap<quint32, ProcessDiskInfo> WindowsDiskMonitor::getProcessDiskInfo() {
    QMap<quint32, ProcessDiskInfo> processDiskMap;

    qint64 currentTime = QDateTime::currentMSecsSinceEpoch();
    double elapsedSec = (currentTime - m_lastProcessUpdateTime) / 1000.0;
    m_lastProcessUpdateTime = currentTime;

    HANDLE h_snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (h_snap == INVALID_HANDLE_VALUE) {
        return processDiskMap;
    }

    PROCESSENTRY32 entry;
    entry.dwSize = sizeof(entry);

    if (Process32First(h_snap, &entry)) {
        do {
            quint32 pid = entry.th32ProcessID;
            if (pid == 0 || pid == 4) continue;

            HANDLE h_proc = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, pid);
            if (h_proc) {
                IO_COUNTERS io_counters = { 0 };
                if (GetProcessIoCounters(h_proc, &io_counters)) {
                    ProcessDiskInfo proc_disk;
                    proc_disk.pid = pid;
                    proc_disk.bytesRead = io_counters.ReadTransferCount;
                    proc_disk.bytesWritten = io_counters.WriteTransferCount;
                    proc_disk.readOperations = io_counters.ReadOperationCount;
                    proc_disk.writeOperations = io_counters.WriteOperationCount;

                    // Вычисляем скорость
                    quint64 lastRead = m_lastProcessReadBytes.value(pid, 0);
                    quint64 lastWrite = m_lastProcessWriteBytes.value(pid, 0);

                    proc_disk.bytesRead = (io_counters.ReadTransferCount - lastRead) / (elapsedSec > 0 ? elapsedSec : 1);
                    proc_disk.bytesWritten = (io_counters.WriteTransferCount - lastWrite) / (elapsedSec > 0 ? elapsedSec : 1);

                    // Обновляем кэш
                    m_lastProcessReadBytes[pid] = io_counters.ReadTransferCount;
                    m_lastProcessWriteBytes[pid] = io_counters.WriteTransferCount;

                    processDiskMap[pid] = proc_disk;
                }
                CloseHandle(h_proc);
            }
        } while (Process32Next(h_snap, &entry));
    }

    CloseHandle(h_snap);

    return processDiskMap;
}

QList<QString> WindowsDiskMonitor::getLogicalDriveStrings() {
    QList<QString> drives;
    DWORD size = GetLogicalDriveStringsW(0, nullptr);
    if (size == 0) return drives;

    wchar_t* buffer = new wchar_t[size];
    if (GetLogicalDriveStringsW(size, buffer)) {
        wchar_t* current = buffer;
        while (*current) {
            QString drive = QString::fromWCharArray(current);
            drives.append(drive);
            current += wcslen(current) + 1;
        }
    }
    delete[] buffer;
    return drives;
}

quint64 WindowsDiskMonitor::getFileSystemFreeSpace(const QString& drive) {
    ULARGE_INTEGER freeBytes;
    if (GetDiskFreeSpaceExW(
        reinterpret_cast<const wchar_t*>(drive.utf16()),
        &freeBytes,
        nullptr,
        nullptr
    )) {
        return freeBytes.QuadPart;
    }
    return 0;
}

quint64 WindowsDiskMonitor::getFileSystemTotalSpace(const QString& drive) {
    ULARGE_INTEGER totalBytes;
    if (GetDiskFreeSpaceExW(
        reinterpret_cast<const wchar_t*>(drive.utf16()),
        nullptr,
        &totalBytes,
        nullptr
    )) {
        return totalBytes.QuadPart;
    }
    return 0;
}