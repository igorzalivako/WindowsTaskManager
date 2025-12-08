#pragma once

#include "IProcessTreeBuilder.h"

class WindowsProcessTreeBuilder : public IProcessTreeBuilder
{
public: 
	ProcessTree buildTree(const QList<ProcessInfo>& processes) override;
};