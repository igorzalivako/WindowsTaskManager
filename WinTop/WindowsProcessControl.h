#pragma once

#include "IProcessControl.h"
#include <Windows.h>

class WindowsProcessControl : public IProcessControl {
public:
    ProcessDetails getProcessDetails(quint32 pid, const QList<ProcessInfo> processes) override;
    bool killProcess(quint32 pId) override;
    QIcon getProcessIcon(quint32 pId);

private:
    QString getProcessPath(quint32 pId);
    quint32 getParentPID(quint32 pId);
    bool killProcessGracefully(quint32 pId);
    quint32 getThreadCount(quint32 pid);
    QDateTime getStartTime(quint32 pid);
    quint32 getChildProcessCount(quint32 pid, const QList<ProcessInfo>& allProcesses);
    quint32 getHandleCount(quint32 pid);
    int getPriority(quint32 pid);
    QString getProcessUserName(quint32 pid);
    QString getProcessPriorityClass(quint32 pid);
};

