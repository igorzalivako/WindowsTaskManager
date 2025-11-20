#pragma once

#include <QObject>
#include <QTimer>
#include <QThread>
#include <QList>
#include <memory>
#include "DataStructs.h"
#include "ISystemMonitor.h"
#include "IServiceMonitor.h"
#include "IDiskMonitor.h"
#include "INetworkMonitor.h"
#include "IGPUMonitor.h"
#include <IProcessControl.h>
#include "IProcessTreeBuilder.h"
#include "IServiceControl.h"

struct UpdateData {
    SystemInfo systemInfo;
    QList<ProcessInfo> processes;
    QList<ServiceInfo> services;
    QList<NetworkInterfaceInfo> networkInterfaces;
    DisksInfo disks;
    QList<GPUInfo> gpus;
};

class DataUpdater : public QObject 
{
    Q_OBJECT

public:
    DataUpdater(quint32 updateIntervalMs = 1000);
    void start(); // новый метод
    void stop();

public slots:
    void update();

signals:
    void dataReady(const UpdateData& data);

private:
    QTimer m_timer;
    std::unique_ptr<ISystemMonitor> _monitor;
    std::unique_ptr<IDiskMonitor> _diskMonitor;
    std::unique_ptr<INetworkMonitor> _networkMonitor;
    std::unique_ptr<IGPUMonitor> _gpuMonitor;
    std::unique_ptr<IServiceMonitor> m_serviceMonitor;
};