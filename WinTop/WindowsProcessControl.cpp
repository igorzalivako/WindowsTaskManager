#include "WindowsProcessControl.h"
#include <TlHelp32.h>
#include <winternl.h>
#include <windows.h>

const quint32 GRACEFUL_KILL_PROCESS_TIMEOUT = 3000;

QString WindowsProcessControl::getProcessPath(quint32 pid) 
{
    QString path = "";
    HANDLE h_proc = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pid);
    if (h_proc) 
    {
        wchar_t buffer[MAX_PATH];
        DWORD size = MAX_PATH;
        if (QueryFullProcessImageNameW(h_proc, 0, buffer, &size)) 
        {
            path = QString::fromWCharArray(buffer);
        }
        CloseHandle(h_proc);
    }
    return path;
}

quint32 WindowsProcessControl::getParentPID(quint32 pid) 
{
    HANDLE h_snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (h_snap == INVALID_HANDLE_VALUE) 
    {
        return 0;
    }

    PROCESSENTRY32 entry;
    entry.dwSize = sizeof(entry);

    if (Process32First(h_snap, &entry)) 
    {
        do 
        {
            if (entry.th32ProcessID == pid) 
            {
                quint32 ppid = entry.th32ParentProcessID;
                CloseHandle(h_snap);
                return ppid;
            }
        } while (Process32Next(h_snap, &entry));
    }

    CloseHandle(h_snap);
    return 0;
}

ProcessDetails WindowsProcessControl::getProcessDetails(quint32 pid, const QList<ProcessInfo> processes) 
{
    ProcessDetails details;
    ProcessInfo info;
    for (auto& processInfo : processes) 
    {
        if (processInfo.pid == pid) 
        {
            info = processInfo;
        }
    }

    details.pid = pid;
    details.path = getProcessPath(pid);
    details.cpuUsage = info.cpuUsage;
    details.memoryUsage = info.memoryUsage;
    details.workingSetSize = info.workingSetSize;
    details.name = info.name;
    details.parentPID = getParentPID(pid);
    details.threadCount = getThreadCount(pid);
    details.startTime = getStartTime(pid);
    details.childProcessesCount = getChildProcessCount(pid, processes);
    details.userName = getProcessUserName(pid);
    details.priorityClass = getProcessPriorityClass(pid);
    

    details.handleCount = getHandleCount(pid);

    return details;
}

bool WindowsProcessControl::killProcess(quint32 pId) 
{

    if (killProcessGracefully(pId)) 
    {
        return true;
    }

    HANDLE hProc = OpenProcess(PROCESS_TERMINATE, FALSE, pId);
    if (!hProc) 
    {
        return false;
    }

    bool success = TerminateProcess(hProc, 0) != 0;
    CloseHandle(hProc);
    return success;
}

static QImage qimageFromHICON(HICON hIcon)
{
    if (!hIcon) 
        return QImage();

    ICONINFO iconInfo;
    if (!GetIconInfo(hIcon, &iconInfo))
        return QImage();

    // Получим размеры
    BITMAP bmColor = {};
    GetObject(iconInfo.hbmColor, sizeof(bmColor), &bmColor);
    const int w = bmColor.bmWidth;
    const int h = bmColor.bmHeight;

    // Подготовим DIB секцию для извлечения пикселей в формате BGRA
    BITMAPINFO bmi = {};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = w;
    bmi.bmiHeader.biHeight = -h; // top-down
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    void* bits = nullptr;
    HDC hdc = GetDC(nullptr);
    HBITMAP dib = CreateDIBSection(hdc, &bmi, DIB_RGB_COLORS, &bits, nullptr, 0);
    HDC memdc = CreateCompatibleDC(hdc);
    HGDIOBJ oldBmp = SelectObject(memdc, dib);

    // Растеризуем иконку в DIB
    DrawIconEx(memdc, 0, 0, hIcon, w, h, 0, nullptr, DI_NORMAL);

    // Копируем в QImage (формат RGBA8888 или premultiplied)
    QImage img(w, h, QImage::Format_ARGB32_Premultiplied);
    if (img.isNull()) {
        SelectObject(memdc, oldBmp);
        DeleteDC(memdc);
        DeleteObject(dib);
        ReleaseDC(nullptr, hdc);
        DeleteObject(iconInfo.hbmColor);
        DeleteObject(iconInfo.hbmMask);
        return QImage();
    }

    // bits сейчас в BGRA; QImage::Format_ARGB32_Premultiplied — тоже BGRA на little-endian,
    // так что можно копировать напрямую.
    memcpy(img.bits(), bits, size_t(w) * size_t(h) * 4);

    // Освобождение ресурсов
    SelectObject(memdc, oldBmp);
    DeleteDC(memdc);
    DeleteObject(dib);
    ReleaseDC(nullptr, hdc);

    if (iconInfo.hbmColor) DeleteObject(iconInfo.hbmColor);
    if (iconInfo.hbmMask)  DeleteObject(iconInfo.hbmMask);

    return img;
}

QIcon getStandardExeIcon() 
{
    SHFILEINFOW sfi = { 0 };
    // Используем пустую строку с флагом, чтобы получить иконку для .exe
    if (SHGetFileInfoW(L"*.exe", FILE_ATTRIBUTE_NORMAL, &sfi, sizeof(sfi),
        SHGFI_ICON | SHGFI_USEFILEATTRIBUTES | SHGFI_SMALLICON)) 
    {
        QImage img = qimageFromHICON(sfi.hIcon);
        DestroyIcon(sfi.hIcon);
        if (!img.isNull()) 
        {
            return QIcon(QPixmap::fromImage(img));
        }
    }
    return QIcon(); // если не удалось, возвращаем пустую
}

QIcon WindowsProcessControl::getProcessIcon(quint32 pId)
{
    QString path = getProcessPath(pId);
    if (path == "")
        return getStandardExeIcon();

    SHFILEINFOW sfi = { 0 };
    if (SHGetFileInfoW(reinterpret_cast<LPCWSTR>(path.utf16()),
        0, &sfi, sizeof(sfi),
        SHGFI_ICON | SHGFI_SMALLICON))
    {
        QImage img = qimageFromHICON(sfi.hIcon);
        DestroyIcon(sfi.hIcon);
        if (!img.isNull())
            return QIcon(QPixmap::fromImage(img));
    }
    return QIcon();
}

struct FindWindowContext 
{
    DWORD pid;
    HWND  hwnd; // результат
};

static BOOL CALLBACK EnumWindowsProc(HWND hwnd, LPARAM lParam) 
{
    FindWindowContext* ctx = reinterpret_cast<FindWindowContext*>(lParam);
    if (!ctx) return TRUE;

    DWORD pid = 0;
    GetWindowThreadProcessId(hwnd, &pid);
    if (pid != ctx->pid) return TRUE;

    // Окно верхнего уровня
    if (!IsWindowVisible(hwnd)) return TRUE;
    if (GetWindow(hwnd, GW_OWNER) != NULL) 
        return TRUE; // исключим owned windows (диалоги/вспомогательные)

    /*// тул-окна
    LONG_PTR ex = GetWindowLongPtr(hwnd, GWL_EXSTYLE);
    if (ex & WS_EX_TOOLWINDOW) return TRUE;*/

    ctx->hwnd = hwnd;
    return FALSE; // нашли подходящее окно
}

bool WindowsProcessControl::killProcessGracefully(quint32 pId) 
{
    FindWindowContext ctx{};
    ctx.pid = static_cast<DWORD>(pId);
    ctx.hwnd = nullptr;

    EnumWindows(EnumWindowsProc, reinterpret_cast<LPARAM>(&ctx));

    if (ctx.hwnd) 
    {
        PostMessage(ctx.hwnd, WM_CLOSE, 0, 0);

        HANDLE hProc = OpenProcess(SYNCHRONIZE, FALSE, static_cast<DWORD>(pId));
        if (hProc) 
        {
            DWORD result = WaitForSingleObject(hProc, GRACEFUL_KILL_PROCESS_TIMEOUT);
            CloseHandle(hProc);
            if (result == WAIT_OBJECT_0) 
            {
                return true;
            }
        }
    }

    return false;
}

quint32 WindowsProcessControl::getThreadCount(quint32 pid) 
{
    quint32 count = 0;
    HANDLE h_snap = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
    if (h_snap != INVALID_HANDLE_VALUE) 
    {
        THREADENTRY32 te = { 0 };
        te.dwSize = sizeof(te);
        if (Thread32First(h_snap, &te)) 
        {
            do {
                if (te.th32OwnerProcessID == pid) 
                {
                    count++;
                }
            } while (Thread32Next(h_snap, &te));
        }
        CloseHandle(h_snap);
    }
    return count;
}

QDateTime WindowsProcessControl::getStartTime(quint32 pid) 
{
    QDateTime dt;
    HANDLE h_proc = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, pid);
    if (h_proc) 
    {
        FILETIME creation_time, exit_time, kernel_time, user_time;
        if (GetProcessTimes(h_proc, &creation_time, &exit_time, &kernel_time, &user_time)) 
        {
            // Преобразуем FILETIME в QDateTime
            ULARGE_INTEGER uli;
            uli.LowPart = creation_time.dwLowDateTime;
            uli.HighPart = creation_time.dwHighDateTime;
            // FILETIME - время в 100-наносекундных интервалах с 1601-01-01 (UTC)
            // QDateTime ожидает миллисекунды с 1970-01-01 (Unix epoch)
            qint64 epoch_offset = 11644473600LL * 10000000LL; // разница между 1601 и 1970 в 100ns
            qint64 unix_time_100ns = uli.QuadPart - epoch_offset;
            qint64 unix_time_ms = unix_time_100ns / 10000;
            dt = QDateTime::fromMSecsSinceEpoch(unix_time_ms);
        }
        CloseHandle(h_proc);
    }
    return dt;
}

quint32 WindowsProcessControl::getChildProcessCount(quint32 pid, const QList<ProcessInfo>& allProcesses) 
{
    quint32 count = 0;
    for (const auto& proc : allProcesses) 
    {
        if (proc.parentPID == pid) 
        {
            count++;
        }
    }
    return count;
}

quint32 WindowsProcessControl::getHandleCount(quint32 pid) 
{
    quint32 count = 0;
    HANDLE h_proc = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, pid);
    if (h_proc) 
    {
        DWORD handle_count = 0;
        if (GetProcessHandleCount(h_proc, &handle_count)) 
        {
            count = handle_count;
        }
        CloseHandle(h_proc);
    }
    return count;
}

int WindowsProcessControl::getPriority(quint32 pid) 
{
    int priority = 0;
    HANDLE h_proc = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, pid);
    if (h_proc) 
    {
        int priority_class = GetPriorityClass(h_proc);
        priority = priority_class;
        CloseHandle(h_proc);
    }
    return priority;
}

QString WindowsProcessControl::getProcessUserName(quint32 pid) 
{
    QString userName = "Не определен";
    HANDLE h_proc = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, pid);
    if (h_proc) 
    {
        HANDLE h_token = nullptr;
        if (OpenProcessToken(h_proc, TOKEN_QUERY, &h_token)) 
        {
            DWORD size = 0;
            GetTokenInformation(h_token, TokenUser, nullptr, 0, &size);
            if (size > 0) 
            {
                auto* buffer = (PTOKEN_USER)malloc(size);
                if (buffer) 
                {
                    if (GetTokenInformation(h_token, TokenUser, buffer, size, &size)) 
                    {
                        SID_NAME_USE sid_use;
                        wchar_t name[256];
                        wchar_t domain[256];
                        DWORD name_len = 256;
                        DWORD domain_len = 256;
                        if (LookupAccountSidW(nullptr, buffer->User.Sid, name, &name_len, domain, &domain_len, &sid_use)) {
                            userName = QString::fromWCharArray(name);
                        }
                    }
                    free(buffer);
                }
            }
            CloseHandle(h_token);
        }
        CloseHandle(h_proc);
    }
    return userName;
}

QString WindowsProcessControl::getProcessPriorityClass(quint32 pid) 
{
    QString priority = "Не определен";
    HANDLE h_proc = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, pid);
    if (h_proc) 
    {
        DWORD priority_class = GetPriorityClass(h_proc);
        if (priority_class == NORMAL_PRIORITY_CLASS) 
        {
            priority = "Обычный";
        }
        else if (priority_class == HIGH_PRIORITY_CLASS) 
        {
            priority = "Высокий";
        }
        else if (priority_class == IDLE_PRIORITY_CLASS) 
        {
            priority = "Низкий";
        }
        else if (priority_class == REALTIME_PRIORITY_CLASS) 
        {
            priority = "Реального времени";
        }
        CloseHandle(h_proc);
    }
    return priority;
}