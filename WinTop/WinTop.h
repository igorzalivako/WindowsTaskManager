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

#include <QtCharts/QChartView>
#include <QtCharts/QLineSeries>
#include <QtCharts/QValueAxis>

#include "ui_WinTop.h"
#include <QTimer.h>
#include <memory>
#include <IMonitor.h>
#include "ProcessTableModel.h"

class WinTop : public QMainWindow
{
    Q_OBJECT

public:
    WinTop(QWidget *parent = nullptr);
    ~WinTop();

private slots:
    void updateData();
    void onProcessContextMenu(const QPoint& pos);

private:
    void setupUI();
    std::unique_ptr<IMonitor> _monitor;
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


    void CreateProcessInfoContextMenu();
};