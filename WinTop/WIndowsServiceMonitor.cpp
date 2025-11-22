#include "WIndowsServiceMonitor.h"

WindowsServiceMonitor::WindowsServiceMonitor() 
{
}

WindowsServiceMonitor::~WindowsServiceMonitor() 
{
}

QList<ServiceInfo> WindowsServiceMonitor::getServices() 
{
    QList<ServiceInfo> services;

    SC_HANDLE hSCM = OpenSCManager(NULL, NULL, SC_MANAGER_ENUMERATE_SERVICE);
    if (!hSCM) 
    {
        return services;
    }

    DWORD bytesNeeded = 0;
    DWORD serviceCount = 0;
    DWORD resumeHandle = 0;

    // Получаем размер буфера
    BOOL result = EnumServicesStatusW(hSCM, SERVICE_WIN32 | SERVICE_DRIVER, SERVICE_STATE_ALL, NULL, 0, &bytesNeeded, &serviceCount, 0);
    if (result != ERROR_SUCCESS)
    {
        return services;
    }

    if (bytesNeeded > 0) 
    {
        auto* buffer = (LPENUM_SERVICE_STATUSW)malloc(bytesNeeded);
        if (buffer) 
        {
            if (EnumServicesStatusW(hSCM, SERVICE_WIN32, SERVICE_STATE_ALL, buffer, bytesNeeded, &bytesNeeded, &serviceCount, 0)) 
            {
                for (DWORD i = 0; i < serviceCount; i++) 
                {
                    ENUM_SERVICE_STATUSW& svc = buffer[i];

                    ServiceInfo info;
                    info.name = QString::fromWCharArray(svc.lpServiceName);
                    info.status = getStatusString(svc.ServiceStatus.dwCurrentState);
                    // Описание
                    SC_HANDLE hSvc = OpenServiceW(hSCM, svc.lpServiceName, SERVICE_QUERY_CONFIG);
                    if (hSvc) 
                    {
                        DWORD descriptionSize = 0;
                        BOOL result = QueryServiceConfig2W(hSvc, SERVICE_CONFIG_DESCRIPTION, NULL, 0, &descriptionSize);
                        if ((result == ERROR_SUCCESS) && (descriptionSize > 0))
                        {
                            SERVICE_DESCRIPTION* descriptionBuffer = (SERVICE_DESCRIPTION*)malloc(descriptionSize);
                            if (descriptionBuffer) 
                            {
                                if (QueryServiceConfig2W(hSvc, SERVICE_CONFIG_DESCRIPTION, (LPBYTE)descriptionBuffer, descriptionSize, &descriptionSize)) 
                                {
                                    if (descriptionBuffer) 
                                    {
                                        info.description = QString::fromWCharArray(descriptionBuffer->lpDescription);
                                    }
                                }
                                free(descriptionBuffer);
                            }
                        }
                        CloseServiceHandle(hSvc);
                    }

                    services.append(info);
                }
            }
            free(buffer);
        }
    }

    CloseServiceHandle(hSCM);
    return services;
}

ServiceStatus WindowsServiceMonitor::getStatusString(DWORD state) 
{
    switch (state) 
    {
    case SERVICE_RUNNING:
        return ServiceStatus::ssRunning;
    case SERVICE_STOPPED:
        return ServiceStatus::ssStopped;
    case SERVICE_PAUSED:
        return ServiceStatus::ssPaused;
    case SERVICE_START_PENDING:
        return ServiceStatus::ssStartPending;
    case SERVICE_STOP_PENDING:
        return ServiceStatus::ssStopPending;
    case SERVICE_CONTINUE_PENDING:
        return ServiceStatus::ssContinuePending;
    case SERVICE_PAUSE_PENDING:
        return ServiceStatus::ssPausePending;
    default:
        return ServiceStatus::ssUnknown;
    }
}