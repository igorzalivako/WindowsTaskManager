#pragma once

#include "IServiceControl.h"
#include <Windows.h>

class WindowsServiceControl : public IServiceControl
{
public:
	WindowsServiceControl();
	~WindowsServiceControl() override;
	bool startService(const QString& serviceName);
	bool stopService(const QString& serviceName);
};