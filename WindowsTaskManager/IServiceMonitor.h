#pragma once

#include <DataStructs.h>

class IServiceMonitor 
{
public:
    virtual QList<ServiceInfo> getServices() = 0;
    virtual ~IServiceMonitor() = default;
};