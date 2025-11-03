#pragma once

#include "DataStructs.h"

class IDiskMonitor {
public:
    virtual QList<DiskInfo> getDiskInfo() = 0;
    virtual QMap<quint32, ProcessDiskInfo> getProcessDiskInfo() = 0;
    virtual ~IDiskMonitor() = default;
};