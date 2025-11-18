#pragma once

#include <DataStructs.h>
#include <IServiceMonitor.h>
#include <Windows.h>

class WindowsServiceMonitor : public IServiceMonitor {
public:
    WindowsServiceMonitor();
    ~WindowsServiceMonitor() override;
    QList<ServiceInfo> getServices() override;

private:
    ServiceStatus getStatusString(DWORD state);
};