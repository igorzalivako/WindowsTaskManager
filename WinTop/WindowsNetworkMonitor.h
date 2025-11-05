#pragma once

#include "INetworkMonitor.h"
#include "WindowsNetworkMonitor.h"
#include "DataStructs.h"

class WindowsNetworkMonitor : public INetworkMonitor {
public:
    WindowsNetworkMonitor();
    ~WindowsNetworkMonitor();
    QList<NetworkInterfaceInfo> getNetworkInfo() override;
    QMap<quint32, ProcessNetworkInfo> getProcessNetworkInfo() override;

private:
    QMap<QString, quint64> m_lastBytesReceived;
    QMap<QString, quint64> m_lastBytesSent;
    qint64 m_lastUpdateTime = 0;

    bool initNetworkCounters();
};