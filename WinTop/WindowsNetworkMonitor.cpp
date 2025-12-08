#include "WindowsNetworkMonitor.h"
#include <QDateTime>
#include <QDebug>
#include <QString>
#include <windows.h>
#include <iphlpapi.h>

WindowsNetworkMonitor::WindowsNetworkMonitor() 
{
    initNetworkCounters();
}

WindowsNetworkMonitor::~WindowsNetworkMonitor() 
{
}

bool WindowsNetworkMonitor::initNetworkCounters() 
{
    _lastUpdateTime = QDateTime::currentMSecsSinceEpoch();
    return true;
}

QList<NetworkInterfaceInfo> WindowsNetworkMonitor::getNetworkInfo() 
{
    QList<NetworkInterfaceInfo> interfaces;

    qint64 currentTime = QDateTime::currentMSecsSinceEpoch();
    double elapsedSec = (currentTime - _lastUpdateTime) / 1000.0;
    _lastUpdateTime = currentTime;

    // Получаем таблицу интерфейсов (из Management Information Base)
    MIB_IFTABLE* pIfTable = nullptr;
    ULONG dwSize = 0;
    DWORD dwRetVal = 0;

    dwRetVal = GetIfTable(pIfTable, &dwSize, FALSE);
    if (dwRetVal == ERROR_INSUFFICIENT_BUFFER) 
    {
        pIfTable = (MIB_IFTABLE*)malloc(dwSize);
        if (!pIfTable) 
        {
            return interfaces;
        }

        dwRetVal = GetIfTable(pIfTable, &dwSize, FALSE);
    }

    if (dwRetVal == NO_ERROR) {
        for (DWORD i = 0; i < pIfTable->dwNumEntries; i++) 
        {
            MIB_IFROW& row = pIfTable->table[i];

            // Пропускаем loopback
            if (row.dwType == IF_TYPE_SOFTWARE_LOOPBACK) 
            {
                continue;
            }

            // Фильтруем только Ethernet и Wi-Fi адаптеры
            bool isEthernet = (row.dwType == IF_TYPE_ETHERNET_CSMACD);
            bool isWireless = (row.dwType == IF_TYPE_IEEE80211);

            if (!isEthernet && !isWireless) 
            {
                continue;
            }

            NetworkInterfaceInfo info;
            info.name = QString::fromWCharArray((wchar_t*)row.wszName);
            info.description = QString::fromLocal8Bit((char*)row.bDescr, row.dwDescrLen);
            info.bytesReceived = row.dwInOctets;
            info.bytesSent = row.dwOutOctets;
            info.packetsReceived = row.dwInUcastPkts;
            info.packetsSent = row.dwOutUcastPkts;

            // Вычисляем скорость
            QString key = QString::number(row.dwIndex);
            quint64 lastReceived = _lastBytesReceived.value(key, 0);
            quint64 lastSent = _lastBytesSent.value(key, 0);

            if (elapsedSec > 0) 
            {
                info.receiveBytesPerSec = (info.bytesReceived - lastReceived) / elapsedSec;
                info.sendBytesPerSec = (info.bytesSent - lastSent) / elapsedSec;
            }

            _lastBytesReceived[key] = info.bytesReceived;
            _lastBytesSent[key] = info.bytesSent;

            interfaces.append(info);
        }
    }

    if (pIfTable) 
    {
        free(pIfTable);
    }

    return interfaces;
}