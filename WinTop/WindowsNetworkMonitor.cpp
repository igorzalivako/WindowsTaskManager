#include "WindowsNetworkMonitor.h"
#include <QDateTime>
#include <QDebug>
#include <QString>
#include <windows.h>
#include <iphlpapi.h>

WindowsNetworkMonitor::WindowsNetworkMonitor() {
    initNetworkCounters();
}

WindowsNetworkMonitor::~WindowsNetworkMonitor() {
    // nothing to clean up
}

bool WindowsNetworkMonitor::initNetworkCounters() {
    m_lastUpdateTime = QDateTime::currentMSecsSinceEpoch();
    return true;
}

QList<NetworkInterfaceInfo> WindowsNetworkMonitor::getNetworkInfo() {
    QList<NetworkInterfaceInfo> interfaces;

    qint64 currentTime = QDateTime::currentMSecsSinceEpoch();
    double elapsedSec = (currentTime - m_lastUpdateTime) / 1000.0;
    m_lastUpdateTime = currentTime;

    // Получаем таблицу интерфейсов
    MIB_IFTABLE* pIfTable = nullptr;
    ULONG dwSize = 0;
    DWORD dwRetVal = 0;

    dwRetVal = GetIfTable(pIfTable, &dwSize, FALSE);
    if (dwRetVal == ERROR_INSUFFICIENT_BUFFER) {
        pIfTable = (MIB_IFTABLE*)malloc(dwSize);
        if (!pIfTable) {
            return interfaces;
        }

        dwRetVal = GetIfTable(pIfTable, &dwSize, FALSE);
    }

    if (dwRetVal == NO_ERROR) {
        for (DWORD i = 0; i < pIfTable->dwNumEntries; i++) {
            MIB_IFROW& row = pIfTable->table[i];

            // Пропускаем loopback и неактивные интерфейсы
            if (row.dwType == IF_TYPE_SOFTWARE_LOOPBACK) {
                continue;
            }

            // Фильтруем только Ethernet и Wi-Fi адаптеры
            bool isEthernet = (row.dwType == IF_TYPE_ETHERNET_CSMACD);
            bool isWireless = (row.dwType == IF_TYPE_IEEE80211);

            if (!isEthernet && !isWireless) {
                continue;
            }

            NetworkInterfaceInfo info;
            info.name = QString::fromWCharArray((wchar_t*)row.wszName);
            info.description = QString::fromLocal8Bit((char*)row.bDescr, row.dwDescrLen);
            info.bytesReceived = row.dwInOctets;
            info.bytesSent = row.dwOutOctets;
            info.packetsReceived = row.dwInUcastPkts;
            info.packetsSent = row.dwOutUcastPkts;
            info.isUp = (row.dwOperStatus == IF_OPER_STATUS_CONNECTED);

            // Вычисляем скорость
            QString key = QString::number(row.dwIndex);
            quint64 lastReceived = m_lastBytesReceived.value(key, 0);
            quint64 lastSent = m_lastBytesSent.value(key, 0);

            if (elapsedSec > 0) {
                info.receiveBytesPerSec = (info.bytesReceived - lastReceived) / elapsedSec;
                info.sendBytesPerSec = (info.bytesSent - lastSent) / elapsedSec;
            }

            m_lastBytesReceived[key] = info.bytesReceived;
            m_lastBytesSent[key] = info.bytesSent;

            interfaces.append(info);
        }
    }

    if (pIfTable) {
        free(pIfTable);
    }

    return interfaces;
}

QMap<quint32, ProcessNetworkInfo> WindowsNetworkMonitor::getProcessNetworkInfo()
{
    QMap<quint32, ProcessNetworkInfo> processNetMap;

    // Получаем TCP таблицу
    PMIB_TCPTABLE2 pTcpTable = nullptr;
    ULONG ulSize = 0;
    DWORD dwRetVal = 0;

    dwRetVal = GetExtendedTcpTable(pTcpTable, &ulSize, TRUE, AF_INET, TCP_TABLE_OWNER_PID_ALL, 0);
    if (dwRetVal == ERROR_INSUFFICIENT_BUFFER) {
        pTcpTable = (PMIB_TCPTABLE2)malloc(ulSize);
        if (!pTcpTable) {
            return processNetMap;
        }

        dwRetVal = GetExtendedTcpTable(pTcpTable, &ulSize, TRUE, AF_INET, TCP_TABLE_OWNER_PID_ALL, 0);
    }

    if (dwRetVal == NO_ERROR) {
        for (DWORD i = 0; i < pTcpTable->dwNumEntries; i++) {
            MIB_TCPROW2& row = pTcpTable->table[i];
            quint32 pid = row.dwOwningPid;

            // Пропускаем системные процессы
            if (pid == 0 || pid == 4) continue;

            // Считаем байты (условно, т.к. Windows не предоставляет байты напрямую по процессу)
            // Пока просто увеличиваем счётчик
            processNetMap[pid].bytesReceived += 1000; // условно
            processNetMap[pid].bytesSent += 1000; // условно
        }
    }

    if (pTcpTable) {
        free(pTcpTable);
    }

    // То же самое для UDP
    PMIB_UDPTABLE_OWNER_PID pUdpTable = nullptr;
    ulSize = 0;
    dwRetVal = GetExtendedUdpTable(pUdpTable, &ulSize, TRUE, AF_INET, UDP_TABLE_OWNER_PID, 0);
    if (dwRetVal == ERROR_INSUFFICIENT_BUFFER) {
        pUdpTable = (PMIB_UDPTABLE_OWNER_PID)malloc(ulSize);
        if (!pUdpTable) {
            return processNetMap;
        }

        dwRetVal = GetExtendedUdpTable(pUdpTable, &ulSize, TRUE, AF_INET, UDP_TABLE_OWNER_PID, 0);
    }

    if (dwRetVal == NO_ERROR) {
        for (DWORD i = 0; i < pUdpTable->dwNumEntries; i++) {
            MIB_UDPROW_OWNER_PID& row = pUdpTable->table[i];
            quint32 pid = row.dwOwningPid;

            // Пропускаем системные процессы
            if (pid == 0 || pid == 4) continue;

            // Считаем байты (условно)
            processNetMap[pid].bytesReceived += 500; // условно
            processNetMap[pid].bytesSent += 500; // условно
        }
    }

    if (pUdpTable) {
        free(pUdpTable);
    }

    return processNetMap;
}
