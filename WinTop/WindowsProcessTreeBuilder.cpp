#include "WindowsProcessTreeBuilder.h"
#include <QHash>

ProcessTree WindowsProcessTreeBuilder::buildTree(const QList<ProcessInfo>& processes) {
    ProcessTree tree;
    tree.setRoot(std::make_shared<ProcessTreeNode>(ProcessInfo{}));

    // Сначала создадим все узлы
    QHash<quint32, std::shared_ptr<ProcessTreeNode>> nodeMap;
    for (const auto& info : processes) 
    {
        if (info.pid != 0)
        {
            nodeMap[info.pid] = std::make_shared<ProcessTreeNode>(info, tree.getRoot());
        }
    }

    // Теперь построим иерархию
    for (auto it = nodeMap.begin(); it != nodeMap.end(); ++it) {
        quint32 ppid = it.value()->data.parentPID;
        quint32 pid = it.value()->data.pid;
        auto parent = nodeMap.value(ppid, nullptr);
        if (parent) {
            it.value()->parent = parent;
            parent->children.append(it.value());
        }
        else {
            // Если родитель не найден, добавим в корень
            it.value()->parent = tree.getRoot();
            tree.getRoot()->children.append(it.value());
        }
    }

    return tree;
}