#pragma once

#include "DataStructs.h"

class INetworkMonitor {
public:
    virtual QList<NetworkInterfaceInfo> getNetworkInfo() = 0;
    virtual QMap<quint32, ProcessNetworkInfo> getProcessNetworkInfo() = 0;
    virtual ~INetworkMonitor() = default;
};