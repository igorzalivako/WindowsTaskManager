#pragma once

#include "DataStructs.h"

class IDiskMonitor 
{
public:
    virtual DisksInfo getDisksInfo() = 0;
    virtual QMap<quint32, ProcessDiskInfo> getProcessDiskInfo() = 0;
    virtual ~IDiskMonitor() = default;
};