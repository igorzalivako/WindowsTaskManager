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
    _systemMonitor = std::make_unique<WindowsSystemMonitor>(_diskMonitor.get(), _networkMonitor.get(), _gpuMonitor.get());
    _serviceMonitor = std::make_unique<WindowsServiceMonitor>();
    _timer.setInterval(updateIntervalMs);
    connect(&_timer, &QTimer::timeout, this, &DataUpdater::update);
}

void DataUpdater::update() 
{
    UpdateData data;
    data.systemInfo = _systemMonitor->getSystemInfo();
    data.processes = _systemMonitor->getProcesses();
    data.services = _serviceMonitor->getServices();
    data.disks = _diskMonitor->getDisksInfo();
    data.gpus = _gpuMonitor->getGPUInfo();
    data.networkInterfaces = _networkMonitor->getNetworkInfo();
    emit dataReady(data);
}

void DataUpdater::start() 
{
    _timer.start();
}

void DataUpdater::stop() 
{
    _timer.stop();
}