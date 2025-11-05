#pragma once

#include "IMonitor.h"
#include "IProcessControl.h"
#include <windows.h>
#include <map>
#include <IDiskMonitor.h>
#include <INetworkMonitor.h>

struct ProcessTimeInfo {
	ULARGE_INTEGER creation_time = {};
	ULARGE_INTEGER exit_time = {};
	ULARGE_INTEGER kernel_time = {};
	ULARGE_INTEGER user_time = {};
	qint64 last_total_time = 0;
	double cpu_usage = 0.0;
};

class WindowsSystemMonitor : public IMonitor
{
public:
	SystemInfo getSystemInfo() override;
	QList<ProcessInfo> getProcesses() override;

	WindowsSystemMonitor(IDiskMonitor* diskMonitor, INetworkMonitor* networkMonitor);
	~WindowsSystemMonitor() override = default;
private:
	IDiskMonitor* m_diskMonitor;
	INetworkMonitor* _networkMonitor;
	ULARGE_INTEGER _lastIdleTime = {};
	ULARGE_INTEGER _lastKernelTime = {};
	ULARGE_INTEGER _lastUserTime = {};
	bool _initialized = false;
	quint64 _lastUpdateTime = 0;

	std::map<quint32, ProcessTimeInfo> _processTimes;
	bool calculateCpuUsage(double& cpu_usage);
	bool calculateProcessCpuUsage(HANDLE _proc, ProcessTimeInfo& timeInfo, quint32 pId);
	double computeCpuPercentage(const ProcessTimeInfo& old, const ProcessTimeInfo& current, qint64 elapsedMs);
};