#pragma once
#include <QDialog>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTreeWidget>
#include <QLabel>
#include <QDialogButtonBox>
#include <QScrollArea>
#include <QHeaderView>
#include "DataStructs.h"
#include "IProcessControl.h"

class ProcessDetailsDialog : public QDialog 
{
    Q_OBJECT

public:
    explicit ProcessDetailsDialog(QWidget* parent = nullptr);
    void setProcessDetails(const ProcessDetails& details);
    void setProcessControl(IProcessControl* processControl);

private:
    void setupUI();
    void setupStyles();

    ProcessDetails _details;
    IProcessControl* _processControl;

    // Виджеты
    QLabel* _processIconLabel;
    QLabel* _processNameLabel;
    QTreeWidget* _detailsTree;
    QDialogButtonBox* _buttonBox;
};