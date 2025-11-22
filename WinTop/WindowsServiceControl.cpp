#include "WindowsServiceControl.h"

WindowsServiceControl::WindowsServiceControl()
{
}

WindowsServiceControl::~WindowsServiceControl()
{
}

bool WindowsServiceControl::startService(const QString& serviceName) 
{
    SC_HANDLE hSCM = OpenSCManager(NULL, NULL, SC_MANAGER_CONNECT);
    if (!hSCM)
    {
        return false;
    }

    SC_HANDLE hSvc = OpenServiceW(hSCM, reinterpret_cast<LPCWSTR>(serviceName.utf16()), SERVICE_START);
    if (!hSvc) 
    {
        CloseServiceHandle(hSCM);
        return false;
    }

    bool success = StartServiceW(hSvc, 0, NULL);

    CloseServiceHandle(hSvc);
    CloseServiceHandle(hSCM);

    return success;
}

bool WindowsServiceControl::stopService(const QString& serviceName)
{
    SC_HANDLE hSCM = OpenSCManager(NULL, NULL, SC_MANAGER_CONNECT);
    if (!hSCM)
    {
        return false;
    }

    SC_HANDLE hSvc = OpenServiceW(hSCM, reinterpret_cast<LPCWSTR>(serviceName.utf16()), SERVICE_STOP);
    if (!hSvc)
    {
        CloseServiceHandle(hSCM);
        return false;
    }

    SERVICE_STATUS status;
    bool success = ControlService(hSvc, SERVICE_CONTROL_STOP, &status);

    CloseServiceHandle(hSvc);
    CloseServiceHandle(hSCM);

    return success;
}