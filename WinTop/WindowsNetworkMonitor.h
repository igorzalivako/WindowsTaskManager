#pragma once

#include "INetworkMonitor.h"
#include "WindowsNetworkMonitor.h"
#include "DataStructs.h"

class WindowsNetworkMonitor : public INetworkMonitor {
public:
    WindowsNetworkMonitor();
    ~WindowsNetworkMonitor();
    QList<NetworkInterfaceInfo> getNetworkInfo() override;

private:
    QMap<QString, quint64> _lastBytesReceived;
    QMap<QString, quint64> _lastBytesSent;
    qint64 _lastUpdateTime = 0;

    bool initNetworkCounters();
};