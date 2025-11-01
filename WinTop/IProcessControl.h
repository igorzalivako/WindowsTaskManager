#pragma once

#include "DataStructs.h"
#include <QIcon.h>

class IProcessControl {
public:
    virtual ProcessDetails getProcessDetails(quint32 pId) = 0;
    virtual bool killProcess(quint32 pId) = 0;
    virtual QIcon getProcessIcon(quint32 pId) = 0;
    virtual ~IProcessControl() = default;
};