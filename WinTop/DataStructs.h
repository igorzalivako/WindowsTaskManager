#pragma once

#include<qstring.h>
#include<qlist.h>

struct SystemInfo 
{
	double cpuUsage = 0.0;
	quint64 totalMemory = 0;
	quint64 availableMemory = 0;
	quint64 usedMemory = 0;
};

struct ProcessInfo 
{
	quint32 pid = 0;
	quint32 parentPID = 0;
	QString name;
	double cpuUsage = 0.0;
	quint64 memoryUsage = 0;
	QString status;
	qint64 updateTimestamp = 0;
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
};

struct FlatProcessNode {
	ProcessInfo info;
	quint32 parentPID;
};