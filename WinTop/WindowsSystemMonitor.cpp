#include "WindowsSystemMonitor.h"
#include <tlhelp32.h>
#include <psapi.h>
#include <QDebug>
#include <QDateTime>

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
        info.name = QString::fromWCharArray(entry.szExeFile);

        HANDLE h_proc = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pid);
        if (h_proc) {
            // Memory
            PROCESS_MEMORY_COUNTERS_EX pmc;
            if (GetProcessMemoryInfo(h_proc, (PROCESS_MEMORY_COUNTERS*) &pmc, sizeof(pmc))) {
                info.memoryUsage = pmc.PrivateUsage;
            }

            // CPU Time
            ProcessTimeInfo time_info = {};
            if (calculateProcessCpuUsage(h_proc, time_info, pid)) {
                current_process_times[pid] = time_info;

                // Если есть предыдущие данные, вычисляем загрузку
                auto it = _processTimes.find(pid);
                if (it != _processTimes.end() && _lastUpdateTime != 0) {
                    qint64 elapsed = current_time - _lastUpdateTime;
                    info.cpuUsage = computeCpuPercentage(it->second, time_info, elapsed);
                }
            }

            CloseHandle(h_proc);
        }

        processes.append(info);
    } while (Process32Next(h_snap, &entry));

    CloseHandle(h_snap);

    // Обновляем время и сохраняем текущие значения
    _lastUpdateTime = current_time;
    _processTimes = std::move(current_process_times);

    return processes;
}

QString WindowsSystemMonitor::getProcessPath(quint32 pid) {
    QString path = "N/A";
    HANDLE h_proc = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pid);
    if (h_proc) {
        wchar_t buffer[MAX_PATH];
        DWORD size = MAX_PATH;
        if (QueryFullProcessImageNameW(h_proc, 0, buffer, &size)) {
            path = QString::fromWCharArray(buffer);
        }
        CloseHandle(h_proc);
    }
    return path;
}

quint32 WindowsSystemMonitor::getParentPID(quint32 pid) {
    HANDLE h_snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (h_snap == INVALID_HANDLE_VALUE) {
        return 0;
    }

    PROCESSENTRY32 entry;
    entry.dwSize = sizeof(entry);

    if (Process32First(h_snap, &entry)) {
        do {
            if (entry.th32ProcessID == pid) {
                quint32 ppid = entry.th32ParentProcessID;
                CloseHandle(h_snap);
                return ppid;
            }
        } while (Process32Next(h_snap, &entry));
    }

    CloseHandle(h_snap);
    return 0;
}

ProcessDetails WindowsSystemMonitor::getProcessDetails(quint32 pId) {
    ProcessDetails details;

    // Найдём базовую информацию из текущего списка процессов
    auto processes = getProcesses();
    for (const auto& proc : processes) {
        if (proc.pid == pId) {
            details.pid = proc.pid;
            details.name = proc.name;
            details.cpuUsage = proc.cpuUsage;
            details.memoryUsage = proc.memoryUsage;
            break;
        }
    }

    details.path = getProcessPath(pId);
    details.parentPID = getParentPID(pId);

    return details;
}

bool WindowsSystemMonitor::killProcess(quint32 pid) {
    HANDLE h_proc = OpenProcess(PROCESS_TERMINATE, FALSE, pid);
    if (!h_proc) {
        return false;
    }

    bool success = TerminateProcess(h_proc, 0) != 0;
    CloseHandle(h_proc);
    return success;
}
