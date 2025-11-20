#include "WindowsSystemMonitor.h"
#include <tlhelp32.h>
#include <psapi.h>
#include <QDebug>
#include <QDateTime>

WindowsSystemMonitor::WindowsSystemMonitor(IDiskMonitor* diskMonitor, INetworkMonitor* networkMonitor, IGPUMonitor* gpuMonitor)
{
    _diskMonitor = diskMonitor;
    _networkMonitor = networkMonitor;
    _gpuMonitor = gpuMonitor;
    initCpuCoreCounters();
}

bool WindowsSystemMonitor::calculateCpuUsage(double& cpu_usage)
{
    FILETIME idle_time, kernel_time, user_time;
    if (!GetSystemTimes(&idle_time, &kernel_time, &user_time)) 
    {
        return false;
    }

    ULARGE_INTEGER idle, kernel, user;
    idle.LowPart = idle_time.dwLowDateTime;
    idle.HighPart = idle_time.dwHighDateTime;
    kernel.LowPart = kernel_time.dwLowDateTime;
    kernel.HighPart = kernel_time.dwHighDateTime;
    user.LowPart = user_time.dwLowDateTime;
    user.HighPart = user_time.dwHighDateTime;

    ULARGE_INTEGER total = { 0 };
    total.QuadPart = kernel.QuadPart + user.QuadPart;

    if (!_initialized) {
        _lastIdleTime = idle;
        _lastKernelTime = kernel;
        _lastUserTime = user;
        _initialized = true;
        cpu_usage = 0.0;
        return true;
    }

    ULARGE_INTEGER idle_diff = { 0 }, total_diff = { 0 };
    idle_diff.QuadPart = idle.QuadPart - _lastIdleTime.QuadPart;
    total_diff.QuadPart = total.QuadPart - (_lastKernelTime.QuadPart + _lastUserTime.QuadPart);

    if (total_diff.QuadPart == 0) {
        cpu_usage = 0.0;
        return true;
    }

    cpu_usage = 100.0 - (idle_diff.QuadPart * 100.0 / total_diff.QuadPart);

    _lastIdleTime = idle;
    _lastKernelTime = kernel;
    _lastUserTime = user;

    return true;
}

bool WindowsSystemMonitor::calculateProcessCpuUsage(HANDLE hProc, ProcessTimeInfo& time_info, quint32 pid) {
    FILETIME creation_time, exit_time, kernel_time, user_time;
    if (!GetProcessTimes(hProc, &creation_time, &exit_time, &kernel_time, &user_time)) {
        return false;
    }

    time_info.creation_time.LowPart = creation_time.dwLowDateTime;
    time_info.creation_time.HighPart = creation_time.dwHighDateTime;
    time_info.exit_time.LowPart = exit_time.dwLowDateTime;
    time_info.exit_time.HighPart = exit_time.dwHighDateTime;
    time_info.kernel_time.LowPart = kernel_time.dwLowDateTime;
    time_info.kernel_time.HighPart = kernel_time.dwHighDateTime;
    time_info.user_time.LowPart = user_time.dwLowDateTime;
    time_info.user_time.HighPart = user_time.dwHighDateTime;

    // Общее время в 100-наносекундных интервалах
    ULARGE_INTEGER total;
    total.QuadPart = time_info.kernel_time.QuadPart + time_info.user_time.QuadPart;

    // Преобразуем в миллисекунды
    time_info.last_total_time = total.QuadPart / 10000;

    return true;
}

double WindowsSystemMonitor::computeCpuPercentage(const ProcessTimeInfo& old, const ProcessTimeInfo& current, qint64 elapsed_ms) {
    if (elapsed_ms == 0) return 0.0;

    qint64 old_total = old.last_total_time;
    qint64 new_total = current.last_total_time;
    qint64 total_time_diff = new_total - old_total;

    if (total_time_diff <= 0) return 0.0;

    // Процент от времени ЦП (elapsed_ms в миллисекундах, умножаем на 10 для перевода в 100ns)
    double cpu_usage = (total_time_diff * 100.0) / (elapsed_ms * 10.0);
    return cpu_usage;
}

SystemInfo WindowsSystemMonitor::getSystemInfo() {
    SystemInfo info = {};

    // CPU
    calculateCpuUsage(info.cpuUsage);

    info.baseSpeedGHz = getBaseCpuSpeedGHz();
    info.coreCount = getCoreCount();
    info.logicalProcessorCount = getLogicalProcessorCount();
    info.cacheL1KB = getCacheSizeKB(1);
    info.cacheL2KB = getCacheSizeKB(2);
    info.cacheL3KB = getCacheSizeKB(3);
    info.processCount = getProcessCount();
    info.threadCount = getThreadCount();
    info.cpuCoreUsage = getCpuCoreUsage();

    // Memory
    MEMORYSTATUSEX mem_status;
    mem_status.dwLength = sizeof(mem_status);
    if (GlobalMemoryStatusEx(&mem_status)) {
        info.totalMemory = mem_status.ullTotalPhys;
        info.availableMemory = mem_status.ullAvailPhys;
        info.usedMemory = info.totalMemory - info.availableMemory;
    }

    return info;
}

QList<ProcessInfo> WindowsSystemMonitor::getProcesses() {
    QList<ProcessInfo> processes;

    // Получаем текущее время
    qint64 current_time = QDateTime::currentMSecsSinceEpoch();

    // Получаем статистику диска
    auto processDiskInfo = _diskMonitor->getProcessDiskInfo();

    // Получаем статистику GPU
    auto processGPUInfo = _gpuMonitor->getProcessGPUInfo();

    HANDLE h_snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (h_snap == INVALID_HANDLE_VALUE) {
        return processes;
    }

    PROCESSENTRY32 entry;
    entry.dwSize = sizeof(entry);

    if (!Process32First(h_snap, &entry)) {
        CloseHandle(h_snap);
        return processes;
    }

    std::map<quint32, ProcessTimeInfo> current_process_times;

    do {
        quint32 pid = entry.th32ProcessID;
        ProcessInfo info;
        info.pid = pid;
        info.parentPID = entry.th32ParentProcessID;
        info.name = QString::fromWCharArray(entry.szExeFile);

        HANDLE h_proc = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pid);
        if (h_proc) 
        {
            // Memory
            PROCESS_MEMORY_COUNTERS_EX pmc;
            if (GetProcessMemoryInfo(h_proc, (PROCESS_MEMORY_COUNTERS*) &pmc, sizeof(pmc))) 
            {
                info.memoryUsage = pmc.PrivateUsage;
                info.workingSetSize = pmc.WorkingSetSize;
            }

            // CPU Time
            ProcessTimeInfo time_info = {};
            if (calculateProcessCpuUsage(h_proc, time_info, pid)) 
            {
                current_process_times[pid] = time_info;

                // Если есть предыдущие данные, вычисляем загрузку
                auto it = _processTimes.find(pid);
                if (it != _processTimes.end() && _lastUpdateTime != 0) 
                {
                    qint64 elapsed = current_time - _lastUpdateTime;
                    info.cpuUsage = computeCpuPercentage(it->second, time_info, elapsed);
                }
            }

            CloseHandle(h_proc);
        }

        // === Добавляем информацию о диске ===
        if (processDiskInfo.contains(pid)) 
        {
            const auto& diskInfo = processDiskInfo[pid];
            info.diskReadBytes = diskInfo.bytesRead;
            info.diskWriteBytes = diskInfo.bytesWritten;
        }

        if (processGPUInfo.contains(pid))
        {
            const auto& gpuInfo = processGPUInfo[pid];
            info.gpuUsage = gpuInfo.gpuUtilization;
        }

        processes.append(info);
    } while (Process32Next(h_snap, &entry));

    CloseHandle(h_snap);

    // Обновляем время и сохраняем текущие значения
    _lastUpdateTime = current_time;
    _processTimes = std::move(current_process_times);

    return processes;
}

quint32 WindowsSystemMonitor::getProcessCount() 
{
    quint32 count = 0;
    HANDLE h_snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (h_snap != INVALID_HANDLE_VALUE) {
        PROCESSENTRY32 entry = { 0 };
        entry.dwSize = sizeof(entry);
        if (Process32First(h_snap, &entry)) {
            do {
                count++;
            } while (Process32Next(h_snap, &entry));
        }
        CloseHandle(h_snap);
    }
    return count;
}

quint32 WindowsSystemMonitor::getThreadCount() 
{
    quint32 count = 0;
    HANDLE h_snap = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
    if (h_snap != INVALID_HANDLE_VALUE) {
        THREADENTRY32 entry = { 0 };
        entry.dwSize = sizeof(entry);
        if (Thread32First(h_snap, &entry)) {
            do {
                count++;
            } while (Thread32Next(h_snap, &entry));
        }
        CloseHandle(h_snap);
    }
    return count;
}

double WindowsSystemMonitor::getBaseCpuSpeedGHz() {
    HKEY hKey;
    DWORD speed = 0;
    DWORD size = sizeof(speed);

    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE,
        L"HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0",
        0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        if (RegQueryValueExW(hKey, L"~MHz", NULL, NULL, (LPBYTE)&speed, &size) == ERROR_SUCCESS) {
            RegCloseKey(hKey);
            return (double)speed / 1000.0; // MHz -> GHz
        }
        RegCloseKey(hKey);
    }
    return 0.0;
}

quint32 WindowsSystemMonitor::getCoreCount() {
    DWORD length = 0;
    GetLogicalProcessorInformation(NULL, &length);
    if (length == 0) return 0;

    PSYSTEM_LOGICAL_PROCESSOR_INFORMATION buffer = (PSYSTEM_LOGICAL_PROCESSOR_INFORMATION)malloc(length);
    if (!buffer) return 0;

    quint32 coreCount = 0;
    if (GetLogicalProcessorInformation(buffer, &length)) {
        DWORD count = length / sizeof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION);
        for (DWORD i = 0; i < count; i++) {
            if (buffer[i].Relationship == RelationProcessorCore) {
                coreCount++;
            }
        }
    }

    free(buffer);
    return coreCount;
}

quint32 WindowsSystemMonitor::getLogicalProcessorCount() {
    DWORD length = 0;
    GetLogicalProcessorInformation(NULL, &length);
    if (length == 0) return 0;

    PSYSTEM_LOGICAL_PROCESSOR_INFORMATION buffer = (PSYSTEM_LOGICAL_PROCESSOR_INFORMATION)malloc(length);
    if (!buffer) return 0;

    if (GetLogicalProcessorInformation(buffer, &length)) {
        DWORD count = length / sizeof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION);
        quint32 logicalCount = 0;
        for (DWORD i = 0; i < count; i++) {
            if (buffer[i].Relationship == RelationProcessorCore) {
                logicalCount += __popcnt64(buffer[i].ProcessorMask);
            }
        }
        free(buffer);
        return logicalCount;
    }

    free(buffer);
    return 0;
}

quint32 WindowsSystemMonitor::getCacheSizeKB(int level) {
    DWORD length = 0;
    GetLogicalProcessorInformation(NULL, &length);
    if (length == 0) return 0;

    PSYSTEM_LOGICAL_PROCESSOR_INFORMATION buffer = (PSYSTEM_LOGICAL_PROCESSOR_INFORMATION)malloc(length);
    if (!buffer) return 0;

    quint32 totalCacheSizeKB = 0;
    if (GetLogicalProcessorInformation(buffer, &length)) {
        DWORD count = length / sizeof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION);
        for (DWORD i = 0; i < count; i++) {
            if (buffer[i].Relationship == RelationCache) {
                PCACHE_DESCRIPTOR cache = &buffer[i].Cache;
                if (cache->Level == level) {
                    totalCacheSizeKB += cache->Size;
                }
            }
        }
    }

    free(buffer);
    return totalCacheSizeKB / 1024;
}

QList<double> WindowsSystemMonitor::getCpuCoreUsage() 
{
    PdhCollectQueryData(m_cpuCoreQuery);
    QList<double> coreUsage;

    quint32 logicalCount = getLogicalProcessorCount();
    for (quint32 i = 0; i < logicalCount && i < m_cpuCoreCounters.size(); i++) 
    {
        PDH_FMT_COUNTERVALUE value;
        if (PdhGetFormattedCounterValue(m_cpuCoreCounters[i], PDH_FMT_DOUBLE, NULL, &value) == ERROR_SUCCESS) 
        {
            coreUsage.append(value.doubleValue);
        }
        else 
        {
            coreUsage.append(0.0);
        }
    }

    return coreUsage;
}

bool WindowsSystemMonitor::initCpuCoreCounters() 
{
    if (PdhOpenQuery(NULL, 0, &m_cpuCoreQuery) != ERROR_SUCCESS) 
    {
        return false;
    }

    quint32 logicalCount = getLogicalProcessorCount();
    for (quint32 i = 0; i < logicalCount; i++) 
    {
        QString counterPath = QString("\\Processor(%1)\\% Processor Time").arg(i);
        PDH_HCOUNTER counter;
        if (PdhAddEnglishCounterW(m_cpuCoreQuery, reinterpret_cast<LPCWSTR>(counterPath.utf16()), 0, &counter) == ERROR_SUCCESS) 
        {
            m_cpuCoreCounters.append(counter);
        }
    }

    PdhCollectQueryData(m_cpuCoreQuery);
    
    return true;
}