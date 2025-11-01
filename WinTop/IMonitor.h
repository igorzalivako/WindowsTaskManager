#pragma once

#include "DataStructs.h"

class IMonitor {
public:
	virtual ~IMonitor() = default;
	virtual SystemInfo getSystemInfo() = 0;
	virtual QList<ProcessInfo> getProcesses() = 0;
};