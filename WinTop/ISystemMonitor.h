#pragma once

#include "DataStructs.h"

class ISystemMonitor {
public:
	virtual ~ISystemMonitor() = default;
	virtual SystemInfo getSystemInfo() = 0;
	virtual QList<ProcessInfo> getProcesses() = 0;
};