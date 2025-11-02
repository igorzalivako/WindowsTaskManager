#pragma once

#include <QStandardItemModel>
#include <memory>
#include "IProcessTreeBuilder.h"

struct ProcessItemRow {
    QStandardItem* nameItem;
    QStandardItem* pidItem;
    QStandardItem* cpuItem;
    QStandardItem* memItem;

    ProcessItemRow(QStandardItem* n, QStandardItem* p, QStandardItem* c, QStandardItem* m)
        : nameItem(n), pidItem(p), cpuItem(c), memItem(m) {
    }

    ProcessItemRow() = default;
};

class ProcessTreeModel : public QStandardItemModel {
    Q_OBJECT

public:
    explicit ProcessTreeModel(QObject* parent = nullptr);

    void setTreeBuilder(std::unique_ptr<IProcessTreeBuilder> builder);
    void updateData(const QList<ProcessInfo>& data);

private:
    std::unique_ptr<IProcessTreeBuilder> _treeBuilder;

    QHash<quint32, ProcessItemRow> _pidToRow;

    QList<FlatProcessNode> _currentFlatTree;

    void updateTreeFromNewFlatList(const QList<FlatProcessNode>& newTree);
};