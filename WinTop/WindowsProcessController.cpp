#include "WindowsProcessController.h"
#include <TlHelp32.h>

const quint32 GRACEFUL_KILL_PROCESS_TIMEOUT = 3000;

QString WindowsProcessControl::getProcessPath(quint32 pid) {
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

quint32 WindowsProcessControl::getParentPID(quint32 pid) {
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

ProcessDetails WindowsProcessControl::getProcessDetails(quint32 pid) {
    ProcessDetails details;
    details.pid = pid;
    details.path = getProcessPath(pid);
    details.parentPID = getParentPID(pid);
    // Загрузка ЦП, память и т.д. могут быть получены отдельно
    return details;
}

bool WindowsProcessControl::killProcess(quint32 pId) {

    if (killProcessGracefully(pId)) 
    {
        return true;
    }

    HANDLE hProc = OpenProcess(PROCESS_TERMINATE, FALSE, pId);
    if (!hProc) {
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

QIcon getStandardExeIcon() {
    SHFILEINFOW sfi = { 0 };
    // Используем пустую строку с флагом, чтобы получить иконку для .exe
    if (SHGetFileInfoW(L"*.exe", FILE_ATTRIBUTE_NORMAL, &sfi, sizeof(sfi),
        SHGFI_ICON | SHGFI_USEFILEATTRIBUTES | SHGFI_SMALLICON)) {
        QImage img = qimageFromHICON(sfi.hIcon);
        DestroyIcon(sfi.hIcon);
        if (!img.isNull()) {
            return QIcon(QPixmap::fromImage(img));
        }
    }
    return QIcon(); // если не удалось, возвращаем пустую
}

QIcon WindowsProcessControl::getProcessIcon(quint32 pId)
{
    QString path = getProcessPath(pId);
    if (path == "N/A")
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

BOOL CALLBACK EnumWindowsProc(HWND hwnd, LPARAM lParam) {
    DWORD pid = 0;
    GetWindowThreadProcessId(hwnd, &pid);

    if (pid == (DWORD)lParam) {
        // Проверим, является ли окно главным (не дочерним, не невидимым)
        if (GetParent(hwnd) == NULL && IsWindowVisible(hwnd)) {
            *(HWND*)lParam = hwnd; // Сохраняем дескриптор
            return FALSE; // Нашли, останавливаем перечисление
        }
    }
    return TRUE; // Продолжаем искать
}

bool WindowsProcessControl::killProcessGracefully(quint32 pId) {
    HWND hwnd = 0;
    LPARAM lParam = (LPARAM)&hwnd;
    EnumWindows(EnumWindowsProc, lParam);

    if (hwnd != 0) {
        PostMessage(hwnd, WM_CLOSE, 0, 0);
        HANDLE hProc = OpenProcess(SYNCHRONIZE, FALSE, pId);
        if (hProc) {
            DWORD result = WaitForSingleObject(hProc, GRACEFUL_KILL_PROCESS_TIMEOUT);
            CloseHandle(hProc);
            if (result == WAIT_OBJECT_0) {
                return true;
            }
        }
    }

    return false; // Окно не найдено или процесс не завершился
}