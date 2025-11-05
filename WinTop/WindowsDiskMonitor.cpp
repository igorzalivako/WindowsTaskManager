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
    if (_diskQuery) {
        PdhCloseQuery(_diskQuery);
    }
}

bool WindowsDiskMonitor::initDiskCounters() {
    if (PdhOpenQuery(NULL, 0, &_diskQuery) != ERROR_SUCCESS) {
        qDebug() << "PdhOpenQuery failed";
        return false;
    }

    QString counterPathRead = QString("\\PhysicalDisk(_Total)\\Disk Read Bytes/sec");
    QString counterPathWrite = QString("\\PhysicalDisk(_Total)\\Disk Write Bytes/sec");

    PDH_HCOUNTER counterRead, counterWrite;

    PDH_STATUS statusRead = PdhAddEnglishCounterW(_diskQuery, (LPCWSTR)counterPathRead.utf16(), 0, &counterRead);
    if (statusRead == ERROR_SUCCESS) {
        _diskCountersRead["_Total"] = counterRead;
    }
    else {
        qDebug() << "PdhAddCounter Read failed with status:" << statusRead;
    }

    PDH_STATUS statusWrite = PdhAddEnglishCounterW(_diskQuery, reinterpret_cast<LPCWSTR>(counterPathWrite.utf16()), 0, &counterWrite);
    if (statusWrite == ERROR_SUCCESS) {
        _diskCountersWrite["_Total"] = counterWrite;
    }
    else {
        qDebug() << "PdhAddCounter Write failed with status:" << statusWrite;
    }

    PdhCollectQueryData(_diskQuery);

    return true;
}

DisksInfo WindowsDiskMonitor::getDiskInfo() {
    DisksInfo disksInfo;
    qint64 currentTime = QDateTime::currentMSecsSinceEpoch();
    double elapsedSec = (currentTime - _lastDiskUpdateTime) / 1000.0;
    _lastDiskUpdateTime = currentTime;

    PDH_STATUS status = PdhCollectQueryData(_diskQuery);
    if (status != ERROR_SUCCESS) {
        qDebug() << "PdhCollectQueryData failed with status:" << status;

        for (const QString& drive : getLogicalDriveStrings()) {
            DiskInfo info;
            info.name = drive;
            info.totalBytes = getFileSystemTotalSpace(drive);
            info.freeBytes = getFileSystemFreeSpace(drive);
            disksInfo.disks.append(info);
        }
        return disksInfo;
    }

    // Ждём немного, чтобы данные успели обновиться
    Sleep(1);

    PDH_FMT_COUNTERVALUE valueRead, valueWrite;
    PDH_HCOUNTER counterRead = _diskCountersRead.value("_Total", nullptr);
    PDH_HCOUNTER counterWrite = _diskCountersWrite.value("_Total", nullptr);

    if (counterRead) {
        PDH_STATUS statusRead = PdhGetFormattedCounterValue(counterRead, PDH_FMT_LARGE, NULL, &valueRead);
        if (statusRead == ERROR_SUCCESS) {
            disksInfo.readBytesPerSec = valueRead.largeValue;
        }
        else {
            qDebug() << "PdhGetFormattedCounterValue Read failed with status:" << statusRead;
        }
    }

    if (counterWrite) {
        PDH_STATUS statusWrite = PdhGetFormattedCounterValue(counterWrite, PDH_FMT_LARGE, NULL, &valueWrite);
        if (statusWrite == ERROR_SUCCESS) {
            disksInfo.writeBytesPerSec = valueWrite.largeValue;
        }
        else {
            qDebug() << "PdhGetFormattedCounterValue Write failed with status:" << statusWrite;
        }
    }

    disksInfo.ioBytesPerSec = disksInfo.readBytesPerSec + disksInfo.writeBytesPerSec;

    // Также добавим информацию по отдельным дискам
    for (const QString& drive : getLogicalDriveStrings()) {
        DiskInfo driveInfo;
        driveInfo.name = drive;
        driveInfo.totalBytes = getFileSystemTotalSpace(drive);
        driveInfo.freeBytes = getFileSystemFreeSpace(drive);
        disksInfo.disks.append(driveInfo);
        // Для отдельных дисков I/O нужно получать отдельно, это сложнее
        // Оставим пока что только общую статистику
    }

    return disksInfo;
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