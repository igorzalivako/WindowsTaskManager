#pragma once

#include <QStandardItemModel>
#include <memory>
#include "IProcessTreeBuilder.h"
#include <IProcessControl.h>
#include <QHash>

struct ProcessItemRow 
{
    QStandardItem* nameItem;
    QStandardItem* pidItem;
    QStandardItem* cpuItem;
    QStandardItem* memItem;
    QStandardItem* diskReadBytes;
    QStandardItem* diskWriteBytes;
    QStandardItem* gpuUsage;

    ProcessItemRow(QStandardItem* n, QStandardItem* p, QStandardItem* c, QStandardItem* m, QStandardItem* diskReadBytes, QStandardItem* diskWriteBytes, QStandardItem* gpuUsage)
        : nameItem(n), pidItem(p), cpuItem(c), memItem(m), diskReadBytes(diskReadBytes), diskWriteBytes(diskWriteBytes), gpuUsage(gpuUsage) {
    }

    ProcessItemRow() = default;
};

class ProcessTreeModel : public QStandardItemModel 
{
    Q_OBJECT

public:
    explicit ProcessTreeModel(QObject* parent = nullptr);

    void setTreeBuilder(std::unique_ptr<IProcessTreeBuilder> builder);
    void setProcessControl(IProcessControl* controller);
    void updateData(const QList<ProcessInfo>& data);
    QVariant data(const QModelIndex& index, int role) const;
private:
    std::unique_ptr<IProcessTreeBuilder> _treeBuilder;

    QHash<quint32, ProcessItemRow> _pidToRow;

    QList<FlatProcessNode> _currentFlatTree;

    IProcessControl* _processControl = nullptr;
    QHash<quint32, QIcon> _iconCache;

    void updateTreeFromNewFlatList(const QList<FlatProcessNode>& newTree);
    QList<QStandardItem*> createTreeRow(const ProcessInfo& processInfo);
    void clearImageCashe(QSet<quint32> pidToRemove);
    ProcessItemRow createProcessItemRow(QList<QStandardItem*> row);
};