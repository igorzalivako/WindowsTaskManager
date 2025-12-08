#pragma once

#include "DataStructs.h"
#include "ProcessTree.h"

class IProcessTreeBuilder
{
public:
	virtual ProcessTree buildTree(const QList<ProcessInfo>& processes) = 0;
	virtual ~IProcessTreeBuilder() = default;
};