#pragma once

#include<qstring.h>
#include<qlist.h>
#include <QDateTime>

struct SystemInfo 
{
	double cpuUsage = 0.0;
	quint64 totalMemory = 0;
	quint64 availableMemory = 0;
	quint64 usedMemory = 0;

	quint32 processCount = 0;
	quint32 threadCount = 0;
	double baseSpeedGHz = 0.0;
	quint32 coreCount = 0;
	quint32 logicalProcessorCount = 0;
	quint32 cacheL1KB = 0;
	quint32 cacheL2KB = 0;
	quint32 cacheL3KB = 0;

	QList<double> cpuCoreUsage;
};

struct ProcessInfo 
{
	quint32 pid = 0;
	quint32 parentPID = 0;
	QString name;
	double cpuUsage = 0.0;
	quint64 memoryUsage = 0;
	quint64 workingSetSize = 0;
	QString status;
	qint64 updateTimestamp = 0;

	quint64 diskReadBytes = 0;
	quint64 diskWriteBytes = 0;

	quint64 gpuUsage = 0;
};

struct ProcessDetails 
{
	quint32 pid = 0;
	QString name;
	QString path;
	quint32 parentPID = 0;
	double cpuUsage = 0.0;
	quint64 memoryUsage = 0;
	quint64 workingSetSize = 0;

	quint64 threadCount = 0;
	QDateTime startTime;
	quint32 childProcessesCount = 0;
	quint32 handleCount = 0;

	QString userName;
	QString priorityClass;
};

struct DiskInfo 
{
	QString name; // например, "C:"
	quint64 totalBytes = 0;
	quint64 freeBytes = 0;
};

struct DisksInfo
{
	QList<DiskInfo> disks;
	double readBytesPerSec = 0.0;
	double writeBytesPerSec = 0.0;
	double ioBytesPerSec = 0.0; // сумма
};

struct ProcessDiskInfo {
	quint32 pid = 0;
	quint64 bytesRead = 0;
	quint64 bytesWritten = 0;
	quint64 readOperations = 0;
	quint64 writeOperations = 0;
};

struct NetworkInterfaceInfo {
	QString name; // например, "Ethernet", "Wi-Fi"
	QString description;
	QString ipAddress;
	quint64 bytesReceived = 0;
	quint64 bytesSent = 0;
	quint64 packetsReceived = 0;
	quint64 packetsSent = 0;
	double receiveBytesPerSec = 0.0;
	double sendBytesPerSec = 0.0;
	bool isUp = false;
};

struct GPUInfo 
{
	QString vendor;
	QString name;
	double loadPercent = 0.0;
	quint64 totalMemoryBytes = 0;
	quint64 usedMemoryBytes = 0;
	double temperatureCelsius = 0.0;
	quint32 usage = 0;
	quint32 powerUsage = 0;
	quint32 fanSpeed = 0;
	QString driverVersion;
};

struct ProcessGPUInfo
{
	quint32 pid = 0;
	qint32 gpuUtilization = 0;
};

struct FlatProcessNode {
	ProcessInfo info;
	quint32 parentPID = 0;
};

enum ServiceStatus { ssRunning, ssStopped, ssPaused, ssStartPending, ssStopPending, ssContinuePending, ssPausePending, ssUnknown };
inline const char* ServiceStatusString[] { "Выполняется", "Остановлено", "Приостановлено", "Запускается", "Останавливается", "Возобновляется", "Приостанавливается", "Неизвестен" };

struct ServiceInfo {
	QString name;
	quint32 processId = 0;
	QString description;
	ServiceStatus status;
};