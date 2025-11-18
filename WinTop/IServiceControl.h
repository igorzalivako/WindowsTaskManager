#pragma once

#include "DataStructs.h"

class IServiceControl
{
public:
	virtual ~IServiceControl() = default;
	virtual bool startService(const QString& serviceName) = 0;
	virtual bool stopService(const QString& serviceName) = 0;
};