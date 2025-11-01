#pragma once

#include "IMonitor.h"
#include "IProcessControl.h"
#include <windows.h>
#include <map>

struct ProcessTimeInfo {
	ULARGE_INTEGER creation_time = {};
	ULARGE_INTEGER exit_time = {};
	ULARGE_INTEGER kernel_time = {};
	ULARGE_INTEGER user_time = {};
	qint64 last_total_time = 0;
	double cpu_usage = 0.0;
};

class WindowsSystemMonitor : public IMonitor, public IProcessControl
{
public:
	// IMonitor
	SystemInfo getSystemInfo() override;
	QList<ProcessInfo> getProcesses() override;

	//IProcessControl
	ProcessDetails getProcessDetails(quint32 pId) override;
	bool killProcess(quint32 pId) override;

	WindowsSystemMonitor() = default;
	~WindowsSystemMonitor() override = default;
private:
	ULARGE_INTEGER _lastIdleTime = {};
	ULARGE_INTEGER _lastKernelTime = {};
	ULARGE_INTEGER _lastUserTime = {};
	bool _initialized = false;
	quint64 _lastUpdateTime = 0;

	std::map<quint32, ProcessTimeInfo> _processTimes;

	bool calculateCpuUsage(double& cpu_usage);
	bool calculateProcessCpuUsage(HANDLE _proc, ProcessTimeInfo& timeInfo, quint32 pId);
	double computeCpuPercentage(const ProcessTimeInfo& old, const ProcessTimeInfo& current, qint64 elapsedMs);
	QString getProcessPath(quint32 pid);
	quint32 getParentPID(quint32 pid);
};