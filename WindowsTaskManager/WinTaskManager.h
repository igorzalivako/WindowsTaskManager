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
#include <QComboBox>
#include <QHash>
#include <QHeaderView>
#include <QThread>

#include <QtCharts/QChartView>
#include <QtCharts/QLineSeries>
#include <QtCharts/QValueAxis>

#include "ui_WinTop.h"
#include <QTimer.h>
#include <memory>
#include <ISystemMonitor.h>
#include "ProcessTableModel.h"
#include <IProcessControl.h>
#include "IProcessTreeBuilder.h"
#include "ProcessTreeModel.h"
#include "ProcessDetailsDialog.h"
#include <QTreeWidget>
#include <QStackedWidget>
#include <IDiskMonitor.h>
#include <INetworkMonitor.h>
#include <IGPUMonitor.h>
#include "ServiceTableModel.h"
#include "IServiceControl.h"
#include <DataUpdater.h>

class WinTaskManager : public QMainWindow
{
    Q_OBJECT

public:
    WinTaskManager(QWidget *parent = nullptr);
    ~WinTaskManager();

private slots:
    void onProcessContextMenu(const QPoint& pos);
    void onFilterLineEditTextChanged(const QString &text);
    void onTreeContextMenu(const QPoint& pos);
    void killSelectedProcesses();
    void showProcessDetails();
    void onServiceContextMenu(const QPoint& pos);
    void onDataReady(const UpdateData& data);

private:
    void setupUI();

    std::unique_ptr<IServiceControl> _serviceControl;
    std::unique_ptr<IProcessControl> _processControl;
    std::unique_ptr<IProcessTreeBuilder> _treeBuilder;
    QList<ProcessInfo> _lastProcesses;
    QList<NetworkInterfaceInfo> _lastNetworkInterfaces;
    QList<ServiceInfo> _lastServices;

    QTabWidget* _tabWidget;
    QWidget* _overviewTab;
    QWidget* _processesTab;

    // Процессы
    QLineEdit* _filterLineEdit;
    QTableView* _processTableView;
    
    // Модели
    ProcessTableModel* _processModel;
    QSortFilterProxyModel* _proxyModel;

    // Контекстное меню процесса
    qint32 _selectedProcessID;
    QString _selectedProcessName;
    QMenu* _processContextMenu;
    QAction* _killProcessAction;
    QAction* _showDetailsAction;

    // Дерево процессов
    QWidget* _treeTab;
    QTreeView* _processTreeView;
    ProcessTreeModel* _processTreeModel;
    QSortFilterProxyModel* _treeProxyModel;

    // Производительность

    // Диск
    QWidget* _performanceTab;
    QTreeWidget* _performanceTree; // боковая панель
    QStackedWidget* _performanceStack; // основная область
    QWidget* _diskPerformancePage; // страница диска
    QChartView* _diskChartView; // график диска
    QChart* _diskChart;
    QLineSeries* _diskSeriesRead;
    QLineSeries* _diskSeriesWrite;
    QValueAxis* _diskAxisX;
    QValueAxis* _diskAxisY;
    QWidget* _diskInfoWidget; // информация о диске внизу

    // Сеть
    QWidget* _networkPerformancePage; // страница сети
    QChartView* _networkChartView; // график сети
    QChart* _networkChart;
    QLineSeries* _networkSeriesRecv;
    QLineSeries* _networkSeriesSent;
    QValueAxis* _networkAxisX;
    QValueAxis* _networkAxisY;
    QWidget* _networkInfoWidget; // информация о сети внизу
    // Выпадающий список для выбора адаптера
    QComboBox* _networkAdapterCombo;

    // Производительность GPU
    QWidget* _gpuPerformancePage; // страница GPU
    QChartView* _gpuChartView; // график GPU
    QChart* _gpuChart;
    QHash<QString, QLineSeries*> _gpuSeriesMap;
    QValueAxis* _gpuAxisX;
    QValueAxis* _gpuAxisY;
    QWidget* _gpuInfoWidget; // информация о GPU внизу

    // CPU
    QWidget* _cpuPerformancePage; // страница CPU
    QChartView* _cpuChartView; // график CPU
    QChart* _cpuChart;
    QLineSeries* _cpuSeries;
    QValueAxis* _cpuAxisX;
    QValueAxis* _cpuAxisY;
    QWidget* _cpuInfoWidget; // информация о CPU внизу

    // ОЗУ
    QWidget* _memoryPerformancePage; // страница Памяти
    QChartView* _memoryChartView; // график Памяти
    QChart* _memoryChart;
    QLineSeries* _memorySeriesUsed;
    QValueAxis* _memoryAxisX;
    QValueAxis* _memoryAxisY;
    QWidget* _memoryInfoWidget;

    // вкладка "Службы"
    QWidget* _servicesTab;
    QTableView* _servicesTableView;
    ServiceTableModel* _servicesModel;
    QSortFilterProxyModel* _servicesProxyModel;

    // Меню управления службой
    QMenu* _serviceContextMenu;
    QAction* _startServiceAction;
    QAction* _stopServiceAction;

    // Выбранная служба
    QString _selectedServiceName;

    void setUpProcessInfoContextMenu();
    void setUpProcessTree();
    void setUpPerformanceTab();
    void setUpServicesTab();
    void showProcessDetailsDialog(quint32 pid);
    void setUpNetworkPerformanceTab();
    void setUpGPUPerformanceTab();
    void setUpCPUPerformanceTab();
    void setUpMemoryPerformanceTab();
    void updateNetworkAdapterList(const QList<NetworkInterfaceInfo> & networkInfo);
    void updatePerformanceTab(const SystemInfo& systemInfo, const DisksInfo& diskInfo, const QList<NetworkInterfaceInfo> & networkInfo, const QList<GPUInfo> & gpuInfo);
    quint32 getPIDFromTreeIndex(const QModelIndex& index);

    // поля для информации под графиками
    // Диск
    QTreeWidget* _diskInfoTree; 
    QScrollArea* _diskInfoScroll; 

    // Сеть
    QTreeWidget* _networkInfoTree; 
    QScrollArea* _networkInfoScroll; 

    // GPU
    QTreeWidget* _gpuInfoTree; 
    QScrollArea* _gpuInfoScroll; 

    // CPU
    QTreeWidget* _cpuInfoTree;
    QScrollArea* _cpuInfoScroll; 

    // Память
    QTreeWidget* _memoryInfoTree; 
    QScrollArea* _memoryInfoScroll;

    // Методы для сохранения/восстановления состояния дерева
    QStringList getExpandedItems(QTreeWidget* tree);
    void setExpandedItems(QTreeWidget* tree, const QStringList& items);

    void setupStyles();

    QThread* _dataThread;
    DataUpdater* _dataUpdater;
};