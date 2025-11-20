#include "DataUpdater.h"

#include <WindowsSystemMonitor.h>
#include <WindowsGPUMonitor.h>
#include <WindowsNetworkMonitor.h>
#include <WindowsDiskMonitor.h>
#include <WindowsServiceMonitor.h>
#include <QDebug>

DataUpdater::DataUpdater(quint32 updateIntervalMs)
{
    _diskMonitor = std::make_unique<WindowsDiskMonitor>();
    _networkMonitor = std::make_unique<WindowsNetworkMonitor>();
    _gpuMonitor = std::make_unique<WindowsGPUMonitor>();
    _monitor = std::make_unique<WindowsSystemMonitor>(_diskMonitor.get(), _networkMonitor.get(), _gpuMonitor.get());
    m_serviceMonitor = std::make_unique<WindowsServiceMonitor>();
    m_timer.setInterval(updateIntervalMs);
    connect(&m_timer, &QTimer::timeout, this, &DataUpdater::update);
}

void DataUpdater::update() {
    UpdateData data;
    data.systemInfo = _monitor->getSystemInfo();
    data.processes = _monitor->getProcesses();
    data.services = m_serviceMonitor->getServices();
    data.disks = _diskMonitor->getDiskInfo();
    data.gpus = _gpuMonitor->getGPUInfo();
    data.networkInterfaces = _networkMonitor->getNetworkInfo();
    static int i = 0;
    qDebug() << "Данные готовы " << i++;
    emit dataReady(data);
}

void DataUpdater::start() {
    m_timer.start();
}

void DataUpdater::stop() {
    m_timer.stop();
}