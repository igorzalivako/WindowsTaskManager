#include "ProcessTree.h"
#include <QQueue>

std::shared_ptr<ProcessTreeNode> ProcessTree::getRoot()
{
    return _root;
}

QList<FlatProcessNode> ProcessTree::getFlatTree()
{
    QList<FlatProcessNode> flatList;

    if (!_root) {
        return flatList; // возвращаем пустой список, если дерево пустое
    }

    // Используем очередь для обхода в ширину (BFS)
    QQueue<std::shared_ptr<ProcessTreeNode>> queue;
    for (const auto& child : _root->children)
    {
        queue.enqueue(child);
    }

    while (!queue.isEmpty()) {
        auto currentNode = queue.dequeue();

        // Создаем FlatProcessNode для текущего узла
        FlatProcessNode flatNode;
        flatNode.info = currentNode->data;

        // Определяем parentPID
        if (currentNode->parent) {
            flatNode.parentPID = currentNode->parent->data.pid; // предполагая, что ProcessInfo имеет поле pid
        }
        else {
            flatNode.parentPID = 0; // или другое значение, обозначающее корневой процесс
        }

        flatList.append(flatNode);

        // Добавляем всех детей в очередь для обработки
        for (const auto& child : currentNode->children) {
            queue.enqueue(child);
        }
    }

    return flatList;
}

void ProcessTree::setRoot(std::shared_ptr<ProcessTreeNode> newRoot)
{
    _root = newRoot;
}
