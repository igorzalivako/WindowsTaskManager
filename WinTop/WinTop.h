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
#include "ProcessDetailsDialog.h"
#include <QTreeWidget>
#include <QStackedWidget>
#include <IDiskMonitor.h>
#include <INetworkMonitor.h>
#include <IGPUMonitor.h>
#include "ServiceTableModel.h"
#include "IServiceControl.h"

class WinTop : public QMainWindow
{
    Q_OBJECT

public:
    WinTop(QWidget *parent = nullptr);
    ~WinTop();

private slots:
    QList<ProcessInfo> updateData();
    void onProcessContextMenu(const QPoint& pos);
    void onFilterLineEditTextChanged(const QString &text);
    void onTreeContextMenu(const QPoint& pos);
    void killSelectedProcesses();
    void showProcessDetails();
    void onServiceContextMenu(const QPoint& pos);

private:
    void setupUI();
    std::unique_ptr<IMonitor> _monitor;
    std::unique_ptr<IProcessControl> _processControl;
    std::unique_ptr<IProcessTreeBuilder> _treeBuilder;
    std::unique_ptr<IDiskMonitor> _diskMonitor;
    std::unique_ptr<INetworkMonitor> _networkMonitor;
    std::unique_ptr<IGPUMonitor> _gpuMonitor;
    std::unique_ptr<IServiceMonitor> m_serviceMonitor;
    std::unique_ptr<IServiceControl> m_serviceControl;
    //std::unique_ptr<IAutoStartMonitor> m_autoStartMonitor;
    //std::unique_ptr<IAutoStartControl> m_autoStartControl;
    QTimer _updateTimer;

    QTabWidget* _tabWidget;
    QWidget* _overviewTab;
    QWidget* _processesTab;

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

    // === Новая вкладка "Службы" ===
    QWidget* m_servicesTab;
    QTableView* m_servicesTableView;
    ServiceTableModel* m_servicesModel;
    QSortFilterProxyModel* _servicesProxyModel;

    // Меню управления службой
    QMenu* m_serviceContextMenu;
    QAction* m_startServiceAction;
    QAction* m_stopServiceAction;

    // Выбранная служба
    QString m_selectedServiceName;

    //// === Новая вкладка "Автозагрузка" ===
    //QWidget* m_autoStartTab;
    //QTableView* m_autoStartTableView;
    //AutoStartTableModel* m_autoStartModel;

    //// Меню управления автозагрузкой
    //QMenu* m_autoStartContextMenu;
    //QAction* m_enableAutoStartAction;
    //QAction* m_disableAutoStartAction;

    //// Выбранная запись автозагрузки
    //QString m_selectedAutoStartName;
    //QString m_selectedAutoStartCommand;
    //bool m_selectedAutoStartIsUser;



    void setUpProcessInfoContextMenu();
    void setUpProcessTree();
    void setUpPerformanceTab();
    void setUpServicesTab();
    void showProcessDetailsDialog(quint32 pid);
    void setUpNetworkPerformanceTab();
    void setUpGPUPerformanceTab();
    void setUpCPUPerformanceTab();
    void setUpMemoryPerformanceTab();
    //void setUpAutoStartTab();
    void updateNetworkAdapterList();
    void updatePerformanceTab(const SystemInfo& info);
    quint32 getPIDFromTreeIndex(const QModelIndex& index);

    // === Новые поля для информации под графиками ===
    // Диск
    QTreeWidget* _diskInfoTree; // вместо QLabel
    QScrollArea* _diskInfoScroll; // для прокрутки

    // Сеть
    QTreeWidget* _networkInfoTree; // вместо QLabel
    QScrollArea* _networkInfoScroll; // для прокрутки

    // GPU
    QTreeWidget* _gpuInfoTree; // вместо QLabel
    QScrollArea* _gpuInfoScroll; // для прокрутки

    // CPU
    QTreeWidget* _cpuInfoTree; // вместо QLabel
    QScrollArea* _cpuInfoScroll; // для прокрутки

    // Память
    QTreeWidget* _memoryInfoTree; // вместо QLabel
    QScrollArea* _memoryInfoScroll; // для прокрутки

    // Методы для сохранения/восстановления состояния
    QStringList getExpandedItems(QTreeWidget* tree);
    void setExpandedItems(QTreeWidget* tree, const QStringList& items);

    void setupStyles();
};