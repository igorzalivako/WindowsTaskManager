#pragma once

#include <QMainWindow>
#include <QTabWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QTableView>
#include <QTimer>
#include <QGroupBox>
#include <QFormLayout>
#include <QSortFilterProxyModel>
#include <QApplication.h>
#include <QMessageBox>
#include <QTreeView>

#include <QtCharts/QChartView>
#include <QtCharts/QLineSeries>
#include <QtCharts/QValueAxis>

#include "ui_WinTop.h"
#include <QTimer.h>
#include <memory>
#include <IMonitor.h>
#include "ProcessTableModel.h"
#include <IProcessControl.h>
#include "IProcessTreeBuilder.h"
#include "ProcessTreeModel.h"

class WinTop : public QMainWindow
{
    Q_OBJECT

public:
    WinTop(QWidget *parent = nullptr);
    ~WinTop();

private slots:
    void updateData();
    void onProcessContextMenu(const QPoint& pos);
    void onFilterLineEditTextChanged(const QString &text);
    void onTreeContextMenu(const QPoint& pos);
    void killSelectedProcesses();
    void showProcessDetails();

private:
    void setupUI();
    std::unique_ptr<IMonitor> _monitor;
    std::unique_ptr<IProcessControl> _processControl;
    std::unique_ptr<IProcessTreeBuilder> _treeBuilder;
    QTimer _updateTimer;

    QTabWidget* _tabWidget;
    QWidget* _overviewTab;
    QWidget* _processesTab;

    // CPU chart
    QChartView* _cpuChartView;
    QChart* _cpuChart;
    QLineSeries* _cpuSeries;
    QValueAxis* _cpuAxisX;
    QValueAxis* _cpuAxisY;

    // Memory chart
    QChartView* _memoryChartView;
    QChart* _memoryChart;
    QLineSeries* _memorySeries;
    QValueAxis* _memoryAxisX;
    QValueAxis* _memoryAxisY;

    // System info
    QLabel* _osLabel;
    QLabel* _ramLabel;

    // Processes
    QLineEdit* _filterLineEdit;
    QTableView* _processTableView;
    
    // Модели
    ProcessTableModel* _processModel;
    QSortFilterProxyModel* _proxyModel;

    // Контекстное меню процесса
    qint32 _selectedProcessID;
    QMenu* _processContextMenu;
    QAction* _killProcessAction;
    QAction* _showDetailsAction;

    // Дерево процессов
    QWidget* _treeTab;
    QTreeView* _processTreeView;
    ProcessTreeModel* _processTreeModel;

    void createProcessInfoContextMenu();
    void createProcessTree();
    void showProcessDetailsDialog(quint32 pid);
    quint32 getPIDFromTreeIndex(const QModelIndex& index);
};