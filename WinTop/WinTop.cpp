#include "WinTop.h"
#include <WindowsSystemMonitor.h>

WinTop::WinTop(QWidget *parent)
    : QMainWindow(parent)
{
	setupUI();
    _monitor = std::make_unique<WindowsSystemMonitor>();
    connect(&_updateTimer, &QTimer::timeout, this, &WinTop::updateData);
    _updateTimer.start(1000);
}

WinTop::~WinTop()
{}

void WinTop::setupUI()
{
    
    _tabWidget = new QTabWidget(this);

    _overviewTab = new QWidget();
    auto* overviewLayout = new QVBoxLayout(_overviewTab);

    // CPU Chart
    _cpuChart = new QChart();
    _cpuSeries = new QLineSeries();
    _cpuChart->addSeries(_cpuSeries);
    _cpuChart->legend()->hide();
    _cpuAxisX = new QValueAxis;
    _cpuAxisY = new QValueAxis;
    _cpuAxisX->setRange(0, 100);
    _cpuAxisY->setRange(0, 100);
    _cpuChart->addAxis(_cpuAxisX, Qt::AlignBottom);
    _cpuChart->addAxis(_cpuAxisY, Qt::AlignLeft);
    _cpuSeries->attachAxis(_cpuAxisX);
    _cpuSeries->attachAxis(_cpuAxisY);

    _cpuChartView = new QChartView(_cpuChart);
    _cpuChartView->setRenderHint(QPainter::Antialiasing);

    // Memory Chart
    _memoryChart = new QChart();
    _memorySeries = new QLineSeries();
    _memoryChart->addSeries(_memorySeries);
    _memoryChart->legend()->hide();
    _memoryAxisX = new QValueAxis;
    _memoryAxisY = new QValueAxis;
    _memoryAxisX->setRange(0, 100);
    _memoryAxisY->setRange(0, 100);
    _memoryChart->addAxis(_memoryAxisX, Qt::AlignBottom);
    _memoryChart->addAxis(_memoryAxisY, Qt::AlignLeft);
    _memorySeries->attachAxis(_memoryAxisX);
    _memorySeries->attachAxis(_memoryAxisY);

    _memoryChartView = new QChartView(_memoryChart);
    _memoryChartView->setRenderHint(QPainter::Antialiasing);

    // System Info
    auto* infoGroup = new QGroupBox("System Information");
    auto* infoLayout = new QFormLayout(infoGroup);
    _osLabel = new QLabel("Windows 10");
    _ramLabel = new QLabel("4.0 / 16.0 GB");
    infoLayout->addRow("OS:", _osLabel);
    infoLayout->addRow("RAM:", _ramLabel);

    overviewLayout->addWidget(_cpuChartView);
    overviewLayout->addWidget(_memoryChartView);
    overviewLayout->addWidget(infoGroup);

    // === Processes Tab ===
    _processesTab = new QWidget();
    auto* processLayout = new QVBoxLayout(_processesTab);

    _filterLineEdit = new QLineEdit();
    _filterLineEdit->setPlaceholderText("Filter...");

    _processTableView = new QTableView();

    // Сортировка
    _processModel = new ProcessTableModel(this);
    _proxyModel = new QSortFilterProxyModel(this);
    _proxyModel->setSourceModel(_processModel);
    _proxyModel->setSortRole(Qt::UserRole);

    _processTableView->setModel(_proxyModel);
    _processTableView->setSortingEnabled(true);

    processLayout->addWidget(_filterLineEdit);
    processLayout->addWidget(_processTableView);

    // Контекстное меню процесса
    CreateProcessInfoContextMenu();

    // === Add Tabs ===
    _tabWidget->addTab(_overviewTab, "Overview");
    _tabWidget->addTab(_processesTab, "Processes");

    setCentralWidget(_tabWidget);
    setWindowTitle("WinTop");
}

void WinTop::updateData() 
{
    SystemInfo info = _monitor->getSystemInfo();
    // Обновляем таблицу
    auto processes = _monitor->getProcesses();
    _processModel->updateData(processes);

    static int x = 0;
    _cpuSeries->append(x, info.cpuUsage);
    _memorySeries->append(x, (double)info.usedMemory / (double)info.totalMemory * 100.0);
    x++;
    if (_cpuSeries->count() > 100) {
        _cpuSeries->removePoints(0, 1);
        _memorySeries->removePoints(0, 1);
    }
    _cpuAxisX->setRange(x - 100, x);
    _memoryAxisX->setRange(x - 100, x);

    // Обновляем метки
    _osLabel->setText("Windows 10");
    _ramLabel->setText(QString("%1 / %2 GB").arg(info.usedMemory / 1024 / 1024 / 1024.0, 0, 'f', 1)
        .arg(info.totalMemory / 1024 / 1024 / 1024.0, 0, 'f', 1));
}

void WinTop::onProcessContextMenu(const QPoint& pos)
{
    QModelIndex index = _processTableView->indexAt(pos);
    if (!index.isValid())
    {
        return; // Клик не на строке
    }

    QModelIndex sourceIndex = _proxyModel->mapToSource(index);
    int row = sourceIndex.row();

    if (row < 0 || row >= _processModel->rowCount())
    {
        return;
    }

    _selectedProcessID = _processModel->
}

void WinTop::CreateProcessInfoContextMenu()
{
    _processContextMenu = new QMenu(this);
    _killProcessAction = new QAction("Завершить процесс", this);
    _showDetailsAction = new QAction("Свойства процесса", this);

    _processContextMenu->addAction(_showDetailsAction);
    _processContextMenu->addAction(_killProcessAction);

    connect(_processTableView, &QTableView::customContextMenuRequested, this, &WinTop::onProcessContextMenu);
    connect(_killProcessAction, &QAction::triggered, this, &WinTop::_killSelectedProcesses);
    connect(_showDetailsAction, &QAction::triggered, this, &WinTop::showProcessDetails);
}