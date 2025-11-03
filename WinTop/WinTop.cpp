#include "WinTop.h"
#include <WindowsSystemMonitor.h>
#include <WindowsProcessControl.h>
#include <WindowsProcessTreeBuilder.h>
#include <WindowsDiskMonitor.h>

WinTop::WinTop(QWidget *parent)
    : QMainWindow(parent)
{
    _monitor = std::make_unique<WindowsSystemMonitor>();
    _processControl = std::make_unique<WindowsProcessControl>();
    _treeBuilder = std::make_unique<WindowsProcessTreeBuilder>();
    _diskMonitor = std::make_unique<WindowsDiskMonitor>();
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

    // Дерево процессов
    createProcessTree();

    // Контекстное меню процесса
    createProcessInfoContextMenu();

    createPerformanceTab();

    // === Add Tabs ===
    _tabWidget->addTab(_overviewTab, "Обзор");
    _tabWidget->addTab(_processesTab, "Процессы");
    _tabWidget->addTab(_treeTab, "Дерево процессов");
    _tabWidget->addTab(_performanceTab, "Производительность");

    setCentralWidget(_tabWidget);
    setWindowTitle("WinTop");
}

QList<ProcessInfo> WinTop::updateData() 
{
    SystemInfo info = _monitor->getSystemInfo();
    // Обновляем таблицу
    auto processes = _monitor->getProcesses();
    _processModel->updateData(processes);

    // Обновляем дерево
    _processTreeModel->updateData(processes);

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

    // === Обновляем данные диска ===
    auto diskInfo = _diskMonitor->getDiskInfo();
    auto processDiskInfo = _diskMonitor->getProcessDiskInfo();

    // Обновляем график диска
    static int diskX = 0;
    double totalRead = 0, totalWrite = 0;
    for (const auto& disk : diskInfo) {
        totalRead += disk.readBytesPerSec;
        totalWrite += disk.writeBytesPerSec;
    }
    _diskSeriesRead->append(diskX, totalRead / 1024 / 1024); // в МБ/с
    _diskSeriesWrite->append(diskX, totalWrite / 1024 / 1024);
    diskX++;
    if (_diskSeriesRead->count() > 100) {
        _diskSeriesRead->removePoints(0, 1);
        _diskSeriesWrite->removePoints(0, 1);
    }
    _diskAxisX->setRange(diskX - 100, diskX);

    // === Обновляем информацию о диске ===
    QString diskInfoText = "Диски:\n";
    for (const auto& disk : diskInfo) {
        double totalGB = disk.totalBytes / 1024.0 / 1024.0 / 1024.0;
        double freeGB = disk.freeBytes / 1024.0 / 1024.0 / 1024.0;
        double usedGB = totalGB - freeGB;
        diskInfoText += QString("%1: %2 ГБ / %3 ГБ (использовано %4 ГБ)\n")
            .arg(disk.name)
            .arg(usedGB, 0, 'f', 2)
            .arg(totalGB, 0, 'f', 2)
            .arg(usedGB, 0, 'f', 2);
    }
    diskInfoText += QString("\nОбщий I/O: Чтение %1 МБ/с, Запись %2 МБ/с")
        .arg(totalRead / 1024 / 1024, 0, 'f', 2)
        .arg(totalWrite / 1024 / 1024, 0, 'f', 2);

    // Найдём QLabel и обновим текст
    auto* diskInfoLabel = _diskInfoWidget->findChild<QLabel*>("diskInfoLabel");
    if (diskInfoLabel) {
        diskInfoLabel->setText(diskInfoText);
    }

    return processes;
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

// Новый слот для дерева
void WinTop::onTreeContextMenu(const QPoint& pos) {
    QModelIndex index = _processTreeView->indexAt(pos);
    if (!index.isValid()) {
        return; // клик не на строке
    }

    quint32 pid = getPIDFromTreeIndex(index);
    if (pid == 0) {
        return;
    }

    _selectedProcessID = pid; // используем общий PID

    _processContextMenu->exec(_processTreeView->viewport()->mapToGlobal(pos));
}

// Вспомогательный метод
quint32 WinTop::getPIDFromTreeIndex(const QModelIndex& index) {
    if (!index.isValid()) {
        return 0;
    }

    QModelIndex pidIndex = index.siblingAtColumn(1);
    if (pidIndex.isValid()) {
        return pidIndex.data().toUInt();
    }

    QModelIndex nameIndex = index.siblingAtColumn(0);
    if (nameIndex.isValid()) {
        return nameIndex.data(Qt::UserRole + 1).toUInt();
    }

    return 0;
}

void WinTop::onFilterLineEditTextChanged(const QString &text)
{
    _proxyModel->setFilterKeyColumn(1);
    _proxyModel->setFilterFixedString(text);
}

void WinTop::createProcessInfoContextMenu()
{
    _processContextMenu = new QMenu(this);
    _killProcessAction = new QAction("Завершить процесс", this);
    _showDetailsAction = new QAction("Свойства процесса", this);

    _processContextMenu->addAction(_showDetailsAction);
    _processContextMenu->addAction(_killProcessAction);

    connect(_processTableView, &QTableView::customContextMenuRequested, this, &WinTop::onProcessContextMenu);
    connect(_processTreeView, &QTreeView::customContextMenuRequested, this, &WinTop::onTreeContextMenu);
    connect(_killProcessAction, &QAction::triggered, this, &WinTop::killSelectedProcesses);
    connect(_showDetailsAction, &QAction::triggered, this, &WinTop::showProcessDetails);
}

void WinTop::createProcessTree() 
{
    _treeTab = new QWidget();
    auto* treeLayout = new QVBoxLayout(_treeTab);

    _processTreeView = new QTreeView();
    _processTreeModel = new ProcessTreeModel(this);
    _processTreeModel->setTreeBuilder(std::move(_treeBuilder)); // передаём билдер
    _processTreeModel->setProcessControl(_processControl.get());
    _treeProxyModel = new QSortFilterProxyModel(this);
    _treeProxyModel->setSourceModel(_processTreeModel);
    _treeProxyModel->setSortRole(Qt::UserRole);
    _processTreeView->setModel(_treeProxyModel);
    _processTreeView->setAlternatingRowColors(true);
    _processTreeView->setSortingEnabled(true);
    _processTreeView->setContextMenuPolicy(Qt::CustomContextMenu);

    treeLayout->addWidget(_processTreeView);
}

void WinTop::createPerformanceTab()
{
    _performanceTab = new QWidget();
    auto* perfLayout = new QHBoxLayout(_performanceTab); // горизонтальный layout

    // Боковая панель (список ресурсов)
    _performanceTree = new QTreeWidget();
    _performanceTree->setHeaderHidden(true);
    _performanceTree->setMaximumWidth(150);

    auto* cpuItem = new QTreeWidgetItem(_performanceTree);
    cpuItem->setText(0, "ЦП");
    cpuItem->setData(0, Qt::UserRole, "cpu");

    auto* memoryItem = new QTreeWidgetItem(_performanceTree);
    memoryItem->setText(0, "Память");
    memoryItem->setData(0, Qt::UserRole, "memory");

    auto* diskItem = new QTreeWidgetItem(_performanceTree);
    diskItem->setText(0, "Диск");
    diskItem->setData(0, Qt::UserRole, "disk");

    auto* networkItem = new QTreeWidgetItem(_performanceTree);
    networkItem->setText(0, "Сеть");
    networkItem->setData(0, Qt::UserRole, "network");

    // ... можно добавить GPU, если реализуешь

    perfLayout->addWidget(_performanceTree);

    // Основная область (графики)
    _performanceStack = new QStackedWidget();

    // === Страница диска ===
    _diskPerformancePage = new QWidget();
    auto* diskPerfLayout = new QVBoxLayout(_diskPerformancePage);

    // График
    _diskChart = new QChart();
    _diskSeriesRead = new QLineSeries();
    _diskSeriesRead->setName("Чтение");
    _diskSeriesWrite = new QLineSeries();
    _diskSeriesWrite->setName("Запись");
    _diskChart->addSeries(_diskSeriesRead);
    _diskChart->addSeries(_diskSeriesWrite);
    _diskChart->legend()->show();

    _diskAxisX = new QValueAxis;
    _diskAxisY = new QValueAxis;
    _diskAxisX->setRange(0, 100);
    _diskAxisY->setRange(0, 100);
    _diskChart->addAxis(_diskAxisX, Qt::AlignBottom);
    _diskChart->addAxis(_diskAxisY, Qt::AlignLeft);
    _diskSeriesRead->attachAxis(_diskAxisX);
    _diskSeriesRead->attachAxis(_diskAxisY);
    _diskSeriesWrite->attachAxis(_diskAxisX);
    _diskSeriesWrite->attachAxis(_diskAxisY);

    _diskChartView = new QChartView(_diskChart);
    _diskChartView->setRenderHint(QPainter::Antialiasing);

    diskPerfLayout->addWidget(_diskChartView);

    // Информация о диске (внизу)
    _diskInfoWidget = new QWidget();
    auto* diskInfoLayout = new QVBoxLayout(_diskInfoWidget);

    // QLabel для отображения информации
    auto* diskInfoLabel = new QLabel("Информация о диске появится здесь...");
    diskInfoLabel->setObjectName("diskInfoLabel"); // для удобства поиска
    diskInfoLayout->addWidget(diskInfoLabel);

    diskPerfLayout->addWidget(_diskInfoWidget);

    _performanceStack->addWidget(_diskPerformancePage);

    // ... можно добавить другие страницы: CPU, Memory, Network

    perfLayout->addWidget(_performanceStack);

    // Подключаем выбор элемента в дереве
    connect(_performanceTree, &QTreeWidget::currentItemChanged, this, [this](QTreeWidgetItem* current, QTreeWidgetItem* previous) {
        Q_UNUSED(previous);
        if (current) {
            QString resource = current->data(0, Qt::UserRole).toString();
            if (resource == "disk") {
                _performanceStack->setCurrentWidget(_diskPerformancePage);
            }
            // ... другие ресурсы
        }
        });

    // Устанавливаем первый элемент как текущий
    _performanceTree->setCurrentItem(_performanceTree->topLevelItem(0));

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
    QList<ProcessInfo> processes = updateData();
    
    ProcessDetails details = _processControl.get()->getProcessDetails(pid, processes);

    // Создаём и показываем диалог
    ProcessDetailsDialog dialog(this);
    dialog.setProcessDetails(details);
    dialog.exec();
}