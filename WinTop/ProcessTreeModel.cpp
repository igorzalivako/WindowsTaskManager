#include "ProcessTreeModel.h"
#include <QStandardItem>
#include <QIcon>
#include <QApplication>
#include <QStyle>

ProcessTreeModel::ProcessTreeModel(QObject* parent)
    : QStandardItemModel(parent)
{
    setHorizontalHeaderLabels({ "Name", "PID", "CPU %", "Memory (MB)" });
}

void ProcessTreeModel::setTreeBuilder(std::unique_ptr<IProcessTreeBuilder> builder) {
    _treeBuilder = std::move(builder);
}

void ProcessTreeModel::updateData(const QList<ProcessInfo>& data) {
    if (!_treeBuilder) return;

    auto newTree = _treeBuilder->buildTree(data);
    auto flatTree = newTree.getFlatTree();

    if (_currentFlatTree.isEmpty()) {
        // Полная инициализация
        clear();
        setHorizontalHeaderLabels({ "Name", "PID", "CPU %", "Memory (MB)" });
        _pidToRow.clear();

        // Двухфазная вставка: сначала корни, потом дети
        QHash<quint32, FlatProcessNode> byPid;
        for (const auto& n : flatTree) byPid.insert(n.info.pid, n);

        // Фаза 1: добавить корневые (parentPID == 0)
        for (const auto& n : flatTree) {
            if (n.parentPID != 0) continue;
            auto* nameItem = new QStandardItem(n.info.name);
            nameItem->setData(n.info.pid, Qt::UserRole + 1);
            auto* pidItem = new QStandardItem(QString::number(n.info.pid));
            auto* cpuItem = new QStandardItem(QString::number(n.info.cpuUsage, 'f', 2) + "%");
            auto* memItem = new QStandardItem(QString::number(n.info.memoryUsage / 1024 / 1024) + " MB");
            QList<QStandardItem*> row{ nameItem, pidItem, cpuItem, memItem };
            invisibleRootItem()->appendRow(row);
            _pidToRow.insert(n.info.pid, ProcessItemRow(nameItem, pidItem, cpuItem, memItem));
        }

        // Фаза 2: итеративно добавлять потомков, когда появится родитель
        bool progress = true;
        QSet<quint32> placed;
        for (const auto& n : flatTree) if (n.parentPID == 0) placed.insert(n.info.pid);

        while (progress) {
            progress = false;
            for (const auto& n : flatTree) {
                if (placed.contains(n.info.pid)) continue;
                // Проверим, есть ли родитель в модели (или это корень)
                if (n.parentPID == 0 || _pidToRow.contains(n.parentPID)) {
                    QStandardItem* parentItem = (n.parentPID == 0)
                        ? invisibleRootItem()
                        : _pidToRow[n.parentPID].nameItem;

                    auto* nameItem = new QStandardItem(n.info.name);
                    nameItem->setData(n.info.pid, Qt::UserRole + 1);
                    auto* pidItem = new QStandardItem(QString::number(n.info.pid));
                    auto* cpuItem = new QStandardItem(QString::number(n.info.cpuUsage, 'f', 2) + "%");
                    auto* memItem = new QStandardItem(QString::number(n.info.memoryUsage / 1024 / 1024) + " MB");
                    QList<QStandardItem*> row{ nameItem, pidItem, cpuItem, memItem };
                    parentItem->appendRow(row);
                    _pidToRow.insert(n.info.pid, ProcessItemRow(nameItem, pidItem, cpuItem, memItem));
                    placed.insert(n.info.pid);
                    progress = true;
                }
            }
        }

        _currentFlatTree = flatTree;
        return;
    }

    // Частичное обновление
    updateTreeFromNewFlatList(flatTree);
    _currentFlatTree = flatTree;
}

static void collectChildrenPids(const QHash<quint32, ProcessItemRow>& pidToRow,
    QStandardItem* parentNameItem,
    QSet<quint32>& outPids)
{
    if (!parentNameItem) return;
    for (int r = 0; r < parentNameItem->rowCount(); ++r) {
        QStandardItem* childNameItem = parentNameItem->child(r, 0);
        if (!childNameItem) continue;
        bool ok = false;
        quint32 childPid = childNameItem->data(Qt::UserRole + 1).toUInt(&ok);
        if (!ok) continue;
        if (!outPids.contains(childPid)) {
            outPids.insert(childPid);
            collectChildrenPids(pidToRow, childNameItem, outPids);
        }
    }
}

void ProcessTreeModel::updateTreeFromNewFlatList(const QList<FlatProcessNode>& newTree) {
    // Построим быстрый доступ по PID и список новых PID
    QHash<quint32, FlatProcessNode> newByPid;
    newByPid.reserve(newTree.size());
    QSet<quint32> newPids;
    for (const auto& node : newTree) {
        newByPid.insert(node.info.pid, node);
        newPids.insert(node.info.pid);
    }

    // 1) Обновить метрики существующих узлов
    for (auto it = _pidToRow.begin(); it != _pidToRow.end(); ++it) {
        quint32 pid = it.key();
        if (!newByPid.contains(pid)) continue;
        const auto& node = newByPid[pid];
        ProcessItemRow& row = it.value();
        if (row.cpuItem) row.cpuItem->setText(QString::number(node.info.cpuUsage, 'f', 2) + "%");
        if (row.memItem) row.memItem->setText(QString::number(node.info.memoryUsage / 1024 / 1024) + " MB");
        // Обновим имя, если могло измениться
        if (row.nameItem && row.nameItem->text() != node.info.name) row.nameItem->setText(node.info.name);
    }

    // 2) Добавить новые узлы с корректным родительством (двухфазная логика)
    QSet<quint32> toPlace;
    for (const auto& node : newTree) {
        if (!_pidToRow.contains(node.info.pid)) toPlace.insert(node.info.pid);
    }

    // Сначала корни
    QList<quint32> placedNow;
    for (quint32 pid : toPlace) {
        const auto& node = newByPid[pid];
        if (node.parentPID == 0) {
            auto* nameItem = new QStandardItem(node.info.name);
            nameItem->setData(node.info.pid, Qt::UserRole + 1);
            auto* pidItem = new QStandardItem(QString::number(node.info.pid));
            auto* cpuItem = new QStandardItem(QString::number(node.info.cpuUsage, 'f', 2) + "%");
            auto* memItem = new QStandardItem(QString::number(node.info.memoryUsage / 1024 / 1024) + " MB");
            QList<QStandardItem*> row{ nameItem, pidItem, cpuItem, memItem };
            invisibleRootItem()->appendRow(row);
            _pidToRow.insert(node.info.pid, ProcessItemRow(nameItem, pidItem, cpuItem, memItem));
            placedNow.append(pid);
        }
    }
    for (quint32 pid : placedNow) toPlace.remove(pid);
    placedNow.clear();

    // Затем дети, итеративно, пока есть прогресс
    bool progress = true;
    while (progress && !toPlace.isEmpty()) {
        progress = false;
        for (auto it = toPlace.begin(); it != toPlace.end(); ) {
            quint32 pid = *it;
            const auto& node = newByPid[pid];
            if (node.parentPID == 0 || _pidToRow.contains(node.parentPID)) {
                QStandardItem* parentItem = (node.parentPID == 0)
                    ? invisibleRootItem()
                    : _pidToRow[node.parentPID].nameItem;

                auto* nameItem = new QStandardItem(node.info.name);
                nameItem->setData(node.info.pid, Qt::UserRole + 1);
                auto* pidItem = new QStandardItem(QString::number(node.info.pid));
                auto* cpuItem = new QStandardItem(QString::number(node.info.cpuUsage, 'f', 2) + "%");
                auto* memItem = new QStandardItem(QString::number(node.info.memoryUsage / 1024 / 1024) + " MB");
                QList<QStandardItem*> row{ nameItem, pidItem, cpuItem, memItem };
                parentItem->appendRow(row);
                _pidToRow.insert(node.info.pid, ProcessItemRow(nameItem, pidItem, cpuItem, memItem));

                it = toPlace.erase(it);
                progress = true;
            }
            else {
                ++it;
            }
        }
    }
    // Если остались непоставленные из-за отсутствующего родителя, прикрепим к корню как fallback
    if (!toPlace.isEmpty()) {
        for (quint32 pid : toPlace) {
            const auto& node = newByPid[pid];
            auto* nameItem = new QStandardItem(node.info.name);
            nameItem->setData(node.info.pid, Qt::UserRole + 1);
            auto* pidItem = new QStandardItem(QString::number(node.info.pid));
            auto* cpuItem = new QStandardItem(QString::number(node.info.cpuUsage, 'f', 2) + "%");
            auto* memItem = new QStandardItem(QString::number(node.info.memoryUsage / 1024 / 1024) + " MB");
            QList<QStandardItem*> row{ nameItem, pidItem, cpuItem, memItem };
            invisibleRootItem()->appendRow(row);
            _pidToRow.insert(node.info.pid, ProcessItemRow(nameItem, pidItem, cpuItem, memItem));
        }
    }

    // 3) Удалить отсутствующие узлы вместе с поддеревьями
    // Сначала соберем PID, которых нет в newTree
    QSet<quint32> missingPids;
    for (auto it = _pidToRow.constBegin(); it != _pidToRow.constEnd(); ++it) {
        quint32 pid = it.key();
        if (!newPids.contains(pid)) missingPids.insert(pid);
    }

    // Развернём каждый в поддерево, чтобы удалить детей до родителя
    QSet<quint32> allToRemove;
    for (quint32 pid : std::as_const(missingPids)) {
        if (!_pidToRow.contains(pid)) continue;
        QStandardItem* nameItem = _pidToRow[pid].nameItem;
        if (!nameItem) continue;
        allToRemove.insert(pid);
        collectChildrenPids(_pidToRow, nameItem, allToRemove);
    }

    // Удаляем постфактум: сначала листья
    // Для стабильности удалим в порядке «глубокие сначала»
    // Подсчитаем глубину по иерархии QStandardItem
    auto depthOf = [](QStandardItem* item) {
        int d = 0;
        QStandardItem* cur = item;
        while (cur && cur->parent()) { cur = cur->parent(); ++d; }
        return d;
        };
    struct Rem { quint32 pid; int depth; };
    QVector<Rem> removals;
    removals.reserve(allToRemove.size());
    for (quint32 pid : std::as_const(allToRemove)) {
        QStandardItem* ni = _pidToRow.contains(pid) ? _pidToRow[pid].nameItem : nullptr;
        removals.push_back({ pid, ni ? depthOf(ni) : 0 });
    }
    std::sort(removals.begin(), removals.end(), [](const Rem& a, const Rem& b) { return a.depth > b.depth; });

    for (const auto& r : removals) {
        if (!_pidToRow.contains(r.pid)) continue;
        ProcessItemRow row = _pidToRow[r.pid]; // копия, чтобы не держать ссылку
        QStandardItem* nameItem = row.nameItem;
        if (!nameItem) {
            _pidToRow.remove(r.pid);
            continue;
        }
        QStandardItem* parent = nameItem->parent();
        if (parent) {
            // Найти фактический индекс по указателю, т.к. row() может быть устаревшим
            int idx = -1;
            for (int i = 0; i < parent->rowCount(); ++i) {
                if (parent->child(i, 0) == nameItem) { idx = i; break; }
            }
            if (idx >= 0) parent->removeRow(idx);
        }
        else {
            // Узел на корне
            QStandardItem* root = invisibleRootItem();
            int idx = -1;
            for (int i = 0; i < root->rowCount(); ++i) {
                if (root->child(i, 0) == nameItem) { idx = i; break; }
            }
            if (idx >= 0) root->removeRow(idx);
        }
        _pidToRow.remove(r.pid);
    }
}