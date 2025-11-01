#pragma once

#include "IProcessControl.h"
#include <Windows.h>

class WindowsProcessControl : public IProcessControl {
public:
    ProcessDetails getProcessDetails(quint32 pid) override;
    bool killProcess(quint32 pId) override;
    QIcon getProcessIcon(quint32 pId);

private:
    QString getProcessPath(quint32 pId);
    quint32 getParentPID(quint32 pId);
    bool killProcessGracefully(quint32 pId);
};

