#include "WinTop.h"
#include <WindowsSystemMonitor.h>
#include <WindowsProcessController.h>

WinTop::WinTop(QWidget *parent)
    : QMainWindow(parent)
{
    _monitor = std::make_unique<WindowsSystemMonitor>();
    _processControl = std::make_unique<WindowsProcessControl>();
    connect(&_updateTimer, &QTimer::timeout, this, &WinTop::updateData);
    setupUI();
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
    _processTableView->setContextMenuPolicy(Qt::CustomContextMenu);
    // Сортировка
    _processModel = new ProcessTableModel(this);
    _processModel->setProcessControl(_processControl.get());

    _proxyModel = new QSortFilterProxyModel(this);
    _proxyModel->setSourceModel(_processModel);
    _proxyModel->setSortRole(Qt::UserRole);

    _processTableView->setModel(_proxyModel);
    _processTableView->setSortingEnabled(true);

    connect(_filterLineEdit, &QLineEdit::textChanged, this, &WinTop::onFilterLineEditTextChanged);

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
    if (!index.isValid()) {
        return;
    }

    QModelIndex sourceIndex = _proxyModel->mapToSource(index);
    ProcessInfo proc = _processModel->getProcessByRow(sourceIndex.row());

    if (proc.pid == 0) {
        return;
    }

    _selectedProcessID = proc.pid;

    _processContextMenu->exec(_processTableView->viewport()->mapToGlobal(pos));
}

void WinTop::onFilterLineEditTextChanged(const QString &text)
{
    _proxyModel->setFilterKeyColumn(1);
    _proxyModel->setFilterFixedString(text);
}

void WinTop::CreateProcessInfoContextMenu()
{
    _processContextMenu = new QMenu(this);
    _killProcessAction = new QAction("Завершить процесс", this);
    _showDetailsAction = new QAction("Свойства процесса", this);

    _processContextMenu->addAction(_showDetailsAction);
    _processContextMenu->addAction(_killProcessAction);

    connect(_processTableView, &QTableView::customContextMenuRequested, this, &WinTop::onProcessContextMenu);
    connect(_killProcessAction, &QAction::triggered, this, &WinTop::killSelectedProcesses);
    connect(_showDetailsAction, &QAction::triggered, this, &WinTop::showProcessDetails);
}

void WinTop::killSelectedProcesses()
{
    if (_selectedProcessID != 0) {
        QMessageBox::StandardButton reply;
        
        QString procName;
        QModelIndexList selected = _processTableView->selectionModel()->selectedRows();
        if (!selected.isEmpty()) {
            QModelIndex proxyIndex = selected.first();
            QModelIndex sourceIndex = _proxyModel->mapToSource(proxyIndex);
            procName = _processModel->getProcessByRow(sourceIndex.row()).name;
        }
        procName = "N/A";

        reply = QMessageBox::question(this, "Подтверждение",
            QString("Завершить процесс %1 (PID: %2)?").arg(procName).arg(_selectedProcessID),
            QMessageBox::Yes | QMessageBox::No);

        if (reply == QMessageBox::Yes) {
            if (_processControl->killProcess(_selectedProcessID)) {
                QMessageBox::information(this, "Успешно", "Процесс завершён.");
            }
            else {
                QMessageBox::critical(this, "Ошибка", "Не удалось завершить процесс.");
            }
        }
    }
}

void WinTop::showProcessDetails()
{
    if (_selectedProcessID != 0) {
        showProcessDetailsDialog(_selectedProcessID);
    }
}

void WinTop::showProcessDetailsDialog(quint32 pid) {
    ProcessDetails details = _processControl.get()->getProcessDetails(pid);

    QString info = QString(
        "PID: %1\n"
        "Имя: %2\n"
        "Путь: %3\n"
        "Родительский PID: %4\n"
        "Загрузка ЦП: %5%\n"
        "Память (Private): %6 MB\n"
        "Рабочий набор: %7 MB"
    ).arg(details.pid)
        .arg(details.name)
        .arg(details.path)
        .arg(details.parentPID)
        .arg(details.cpuUsage, 0, 'f', 2)
        .arg(details.memoryUsage / 1024 / 1024)
        .arg(details.workingSetSize / 1024 / 1024);

    QMessageBox::information(this, "Свойства процесса", info);
}