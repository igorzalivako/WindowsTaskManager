#pragma once

#include <QAbstractTableModel>
#include <QList>
#include "DataStructs.h"

class ProcessTableModel : public QAbstractTableModel {
    Q_OBJECT

public:
    explicit ProcessTableModel(QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    int columnCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;
    ProcessInfo getProcessByRow(int row) const;

    void updateData(const QList<ProcessInfo>& data);

private:
    QList<ProcessInfo> _processes;
};


