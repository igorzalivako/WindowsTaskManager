#pragma once

#include "DataStructs.h"

struct ProcessTreeNode {
    ProcessInfo data;
    std::shared_ptr<ProcessTreeNode> parent;
    QList<std::shared_ptr<ProcessTreeNode>> children;

    explicit ProcessTreeNode(const ProcessInfo& info, std::shared_ptr<ProcessTreeNode> p = nullptr)
        : data(info), parent(p) {
    }
};

class ProcessTree {
public:
    std::shared_ptr<ProcessTreeNode> getRoot();
    QList<FlatProcessNode> getFlatTree();
    void setRoot(std::shared_ptr<ProcessTreeNode> newRoot);
private:
    std::shared_ptr<ProcessTreeNode> _root;
};