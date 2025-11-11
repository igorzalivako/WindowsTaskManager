#pragma once

#include "DataStructs.h"

class IGPUMonitor {
public:
    virtual QList<GPUInfo> getGPUInfo() = 0;
    virtual ~IGPUMonitor() = default;
};