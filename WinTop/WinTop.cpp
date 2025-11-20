#include "WinTop.h"
#include "WindowsSystemMonitor.h"
#include "WindowsProcessControl.h"
#include "WindowsProcessTreeBuilder.h"
#include "WindowsDiskMonitor.h"
#include "WindowsNetworkMonitor.h"
#include <WindowsGPUMonitor.h>
#include <WindowsServiceMonitor.h>
#include "WindowsServiceControl.h"

WinTop::WinTop(QWidget *parent)
    : QMainWindow(parent)
{
    m_serviceControl = std::make_unique<WindowsServiceControl>();
    _processControl = std::make_unique<WindowsProcessControl>();
    _treeBuilder = std::make_unique<WindowsProcessTreeBuilder>();

    m_dataThread = new QThread();
    m_dataUpdater = new DataUpdater(1000);
    m_dataUpdater->moveToThread(m_dataThread);
    connect(m_dataThread, &QThread::started, m_dataUpdater, &DataUpdater::update);
    connect(m_dataUpdater, &DataUpdater::dataReady, this, &WinTop::onDataReady);

    setupUI();

    m_dataThread->start();
    m_dataUpdater->start();
}

WinTop::~WinTop()
{}

void WinTop::setupUI()
{
    setupStyles();
    _tabWidget = new QTabWidget(this);

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
    _processTableView->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);

    connect(_filterLineEdit, &QLineEdit::textChanged, this, &WinTop::onFilterLineEditTextChanged);

    processLayout->addWidget(_filterLineEdit);
    processLayout->addWidget(_processTableView);

    // Дерево процессов
    setUpProcessTree();

    // Контекстное меню процесса
    setUpProcessInfoContextMenu();

    setUpPerformanceTab();

    setUpServicesTab();

    // === Add Tabs ===
    _tabWidget->addTab(_processesTab, "Процессы");
    _tabWidget->addTab(_treeTab, "Дерево процессов");
    _tabWidget->addTab(_performanceTab, "Производительность");
    _tabWidget->addTab(m_servicesTab, "Службы");

    setCentralWidget(_tabWidget);
    setWindowTitle("WinTop");
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
    _selectedProcessName = proc.name;

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

    _selectedProcessID = pid;
    for (const auto& proc : _lastProcesses)
    {
        if (proc.pid == pid)
        {
            _selectedProcessName = proc.name;
        }
    }

    _processContextMenu->exec(_processTreeView->viewport()->mapToGlobal(pos));
}

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

void WinTop::setUpProcessInfoContextMenu()
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

void WinTop::setUpProcessTree() 
{
    _treeTab = new QWidget();
    auto* treeLayout = new QVBoxLayout(_treeTab);

    _processTreeView = new QTreeView();
    _processTreeModel = new ProcessTreeModel(this);
    _processTreeModel->setTreeBuilder(std::move(_treeBuilder));
    _processTreeModel->setProcessControl(_processControl.get());
    _treeProxyModel = new QSortFilterProxyModel(this);
    _treeProxyModel->setSourceModel(_processTreeModel);
    _treeProxyModel->setSortRole(Qt::UserRole);
    _processTreeView->setModel(_treeProxyModel);
    _processTreeView->setAlternatingRowColors(true);
    _processTreeView->setSortingEnabled(true);
    _processTreeView->setContextMenuPolicy(Qt::CustomContextMenu);
    _processTreeView->header()->setSectionResizeMode(QHeaderView::ResizeToContents);

    treeLayout->addWidget(_processTreeView);
}

void WinTop::setUpPerformanceTab()
{
    _performanceTab = new QWidget();
    auto* perfLayout = new QHBoxLayout(_performanceTab); // горизонтальный layout

    // Боковая панель (список ресурсов)
    _performanceTree = new QTreeWidget();
    _performanceTree->setHeaderHidden(true);
    _performanceTree->setMaximumWidth(150);
    _performanceTree->setMinimumWidth(150);

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

    auto* gpuItem = new QTreeWidgetItem(_performanceTree);
    gpuItem->setText(0, "GPU");
    gpuItem->setData(0, Qt::UserRole, "gpu");

    perfLayout->addWidget(_performanceTree);

    // Основная область (графики)
    _performanceStack = new QStackedWidget();

    setUpCPUPerformanceTab();

    setUpMemoryPerformanceTab();

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

    // === Информация о диске (внизу) ===
    _diskInfoTree = new QTreeWidget();
    _diskInfoTree->setHeaderHidden(true);
    _diskInfoTree->setRootIsDecorated(false); // убираем иконки

    _diskInfoScroll = new QScrollArea();
    _diskInfoScroll->setWidget(_diskInfoTree);
    _diskInfoScroll->setWidgetResizable(true);
    _diskInfoScroll->setMaximumHeight(150); // ограничиваем высоту

    diskPerfLayout->addWidget(_diskInfoScroll);

    _performanceStack->addWidget(_diskPerformancePage);

    setUpNetworkPerformanceTab();
    
    setUpGPUPerformanceTab();

    perfLayout->addWidget(_performanceStack);

    connect(_performanceTree, &QTreeWidget::currentItemChanged, this, [this](QTreeWidgetItem* current, QTreeWidgetItem* previous) {
        Q_UNUSED(previous);
        if (current) {
            QString resource = current->data(0, Qt::UserRole).toString();
            if (resource == "disk") {
                _performanceStack->setCurrentWidget(_diskPerformancePage);
            }
        }
    });

    _performanceTree->setCurrentItem(_performanceTree->topLevelItem(0));

}

void WinTop::setUpServicesTab()
{
    m_servicesTab = new QWidget();
    auto* servicesLayout = new QVBoxLayout(m_servicesTab);

    m_servicesTableView = new QTableView();
    m_servicesModel = new ServiceTableModel(this);
    
    _servicesProxyModel = new QSortFilterProxyModel(this);
    _servicesProxyModel->setSourceModel(m_servicesModel);
    _servicesProxyModel->setSortRole(Qt::UserRole);

    m_servicesTableView->setModel(_servicesProxyModel);

    // Настройка таблицы
    m_servicesTableView->setAlternatingRowColors(true);
    m_servicesTableView->setSortingEnabled(true);
    m_servicesTableView->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeMode::Interactive);

    // === Контекстное меню для служб ===
    m_serviceContextMenu = new QMenu(this);
    m_startServiceAction = new QAction("Запустить службу", this);
    m_stopServiceAction = new QAction("Остановить службу", this);

    m_serviceContextMenu->addAction(m_startServiceAction);
    m_serviceContextMenu->addAction(m_stopServiceAction);

    m_servicesTableView->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_servicesTableView, &QTableView::customContextMenuRequested,
        this, &WinTop::onServiceContextMenu);

    connect(m_startServiceAction, &QAction::triggered, this, [this]() {
        if (!m_selectedServiceName.isEmpty()) {
            if (m_serviceControl.get()->startService(m_selectedServiceName)) {
                QMessageBox::information(this, "Успешно", "Служба запущена.");
            }
            else {
                QMessageBox::critical(this, "Ошибка", "Не удалось запустить службу.");
            }
        }
        });

    connect(m_stopServiceAction, &QAction::triggered, this, [this]() {
        if (!m_selectedServiceName.isEmpty()) {
            if (m_serviceControl.get()->stopService(m_selectedServiceName)) {
                QMessageBox::information(this, "Успешно", "Служба остановлена.");
            }
            else {
                QMessageBox::critical(this, "Ошибка", "Не удалось остановить службу.");
            }
        }
        });

    servicesLayout->addWidget(m_servicesTableView);
}

void WinTop::setUpNetworkPerformanceTab()
{
    // === Страница сети ===
    _networkPerformancePage = new QWidget();
    auto* networkPerfLayout = new QVBoxLayout(_networkPerformancePage);

    // Выпадающий список
    _networkAdapterCombo = new QComboBox();
    networkPerfLayout->addWidget(_networkAdapterCombo);

    // График
    _networkChart = new QChart();
    _networkSeriesRecv = new QLineSeries();
    _networkSeriesRecv->setName("Приём");
    _networkSeriesSent = new QLineSeries();
    _networkSeriesSent->setName("Отправка");
    _networkChart->addSeries(_networkSeriesRecv);
    _networkChart->addSeries(_networkSeriesSent);
    _networkChart->legend()->show();

    _networkAxisX = new QValueAxis;
    _networkAxisY = new QValueAxis;
    _networkAxisX->setRange(0, 100);
    _networkAxisY->setRange(0, 100);
    _networkChart->addAxis(_networkAxisX, Qt::AlignBottom);
    _networkChart->addAxis(_networkAxisY, Qt::AlignLeft);
    _networkSeriesRecv->attachAxis(_networkAxisX);
    _networkSeriesRecv->attachAxis(_networkAxisY);
    _networkSeriesSent->attachAxis(_networkAxisX);
    _networkSeriesSent->attachAxis(_networkAxisY);

    _networkChartView = new QChartView(_networkChart);
    _networkChartView->setRenderHint(QPainter::Antialiasing);

    networkPerfLayout->addWidget(_networkChartView);

    // Информация о сети (внизу)
    _networkInfoTree = new QTreeWidget();
    _networkInfoTree->setHeaderHidden(true);
    _networkInfoTree->setRootIsDecorated(false);

    _networkInfoScroll = new QScrollArea();
    _networkInfoScroll->setWidget(_networkInfoTree);
    _networkInfoScroll->setWidgetResizable(true);
    _networkInfoScroll->setMaximumHeight(150);

    networkPerfLayout->addWidget(_networkInfoScroll);

    _performanceStack->addWidget(_networkPerformancePage);

    // Подключаем выбор элемента в дереве
    connect(_performanceTree, &QTreeWidget::currentItemChanged, this, [this](QTreeWidgetItem* current, QTreeWidgetItem* previous) {
        Q_UNUSED(previous);
        if (current) {
            QString resource = current->data(0, Qt::UserRole).toString();
            if (resource == "disk") {
                _performanceStack->setCurrentWidget(_diskPerformancePage);
            }
            else if (resource == "network") 
            {
                _performanceStack->setCurrentWidget(_networkPerformancePage);
                updateNetworkAdapterList(_lastNetworkInterfaces);
            } 
            else if (resource == "gpu")
            {
                _performanceStack->setCurrentWidget(_gpuPerformancePage);
            }
            else if (resource == "cpu")
            {
                _performanceStack->setCurrentWidget(_cpuPerformancePage);
            }
            else if (resource == "memory")
            {
                _performanceStack->setCurrentWidget(_memoryPerformancePage);
            }
        }
    });

    connect(_networkAdapterCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int index) {
        // Очищаем графики при смене адаптера
        _networkSeriesRecv->clear();
        _networkSeriesSent->clear();
    });
}

void WinTop::setUpGPUPerformanceTab()
{
    _gpuPerformancePage = new QWidget();
    auto* gpuPerfLayout = new QVBoxLayout(_gpuPerformancePage);

    // График
    _gpuChart = new QChart();
    _gpuChart->legend()->show();

    _gpuAxisX = new QValueAxis;
    _gpuAxisY = new QValueAxis;
    _gpuAxisX->setRange(0, 100);
    _gpuAxisY->setRange(0, 100);
    _gpuChart->addAxis(_gpuAxisX, Qt::AlignBottom);
    _gpuChart->addAxis(_gpuAxisY, Qt::AlignLeft);

    _gpuChartView = new QChartView(_gpuChart);
    _gpuChartView->setRenderHint(QPainter::Antialiasing);

    gpuPerfLayout->addWidget(_gpuChartView);

    // === Информация о GPU (внизу) ===
    _gpuInfoTree = new QTreeWidget();
    _gpuInfoTree->setHeaderHidden(true);
    _gpuInfoTree->setRootIsDecorated(false);

    _gpuInfoScroll = new QScrollArea();
    _gpuInfoScroll->setWidget(_gpuInfoTree);
    _gpuInfoScroll->setWidgetResizable(true);
    _gpuInfoScroll->setMaximumHeight(150);

    gpuPerfLayout->addWidget(_gpuInfoScroll);
    _performanceStack->addWidget(_gpuPerformancePage);
}

void WinTop::setUpCPUPerformanceTab()
{
    _cpuPerformancePage = new QWidget();
    auto* cpuPerfLayout = new QVBoxLayout(_cpuPerformancePage);

    // График
    _cpuChart = new QChart();
    _cpuSeries = new QLineSeries();
    _cpuSeries->setName("Загрузка ЦП %");
    _cpuChart->addSeries(_cpuSeries);
    _cpuChart->legend()->show();

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

    cpuPerfLayout->addWidget(_cpuChartView);

    // === Информация о CPU (внизу) ===
    _cpuInfoTree = new QTreeWidget();
    _cpuInfoTree->setHeaderHidden(true);
    _cpuInfoTree->setRootIsDecorated(false);

    _cpuInfoScroll = new QScrollArea();
    _cpuInfoScroll->setWidget(_cpuInfoTree);
    _cpuInfoScroll->setWidgetResizable(true);
    _cpuInfoScroll->setMaximumHeight(150);

    cpuPerfLayout->addWidget(_cpuInfoScroll);

    _performanceStack->addWidget(_cpuPerformancePage);
}

void WinTop::setUpMemoryPerformanceTab()
{
    _memoryPerformancePage = new QWidget();
    auto* memoryPerfLayout = new QVBoxLayout(_memoryPerformancePage);

    // График
    _memoryChart = new QChart();
    _memorySeriesUsed = new QLineSeries();
    _memorySeriesUsed->setName("Использовано");
    _memoryChart->addSeries(_memorySeriesUsed);
    _memoryChart->legend()->show();

    _memoryAxisX = new QValueAxis;
    _memoryAxisY = new QValueAxis;
    _memoryAxisX->setRange(0, 100);
    _memoryAxisY->setRange(0, 100);
    _memoryChart->addAxis(_memoryAxisX, Qt::AlignBottom);
    _memoryChart->addAxis(_memoryAxisY, Qt::AlignLeft);
    _memorySeriesUsed->attachAxis(_memoryAxisX);
    _memorySeriesUsed->attachAxis(_memoryAxisY);

    _memoryChartView = new QChartView(_memoryChart);
    _memoryChartView->setRenderHint(QPainter::Antialiasing);

    memoryPerfLayout->addWidget(_memoryChartView);

    // === Информация о Памяти (внизу) ===
    _memoryInfoTree = new QTreeWidget();
    _memoryInfoTree->setHeaderHidden(true);
    _memoryInfoTree->setRootIsDecorated(false);

    _memoryInfoScroll = new QScrollArea();
    _memoryInfoScroll->setWidget(_memoryInfoTree);
    _memoryInfoScroll->setWidgetResizable(true);
    _memoryInfoScroll->setMaximumHeight(150);

    memoryPerfLayout->addWidget(_memoryInfoScroll);

    _performanceStack->addWidget(_memoryPerformancePage);
}

void WinTop::updateNetworkAdapterList(const QList<NetworkInterfaceInfo>& networkInfo) {

    _networkAdapterCombo->clear();

    for (const auto& net : networkInfo) {
        // Добавляем только активные адаптеры
        _networkAdapterCombo->addItem(QString("%1 (%2)").arg(net.description), net.name);
    }
}

void WinTop::updatePerformanceTab(const SystemInfo& info, const DisksInfo& disksInfo, const QList<NetworkInterfaceInfo>& networkInfo, const QList<GPUInfo>& gpuInfo)
{
    // === Сохраняем состояния ===
    QStringList diskExpanded = getExpandedItems(_diskInfoTree);
    QStringList networkExpanded = getExpandedItems(_networkInfoTree);
    QStringList gpuExpanded = getExpandedItems(_gpuInfoTree);
    QStringList cpuExpanded = getExpandedItems(_cpuInfoTree);
    QStringList memoryExpanded = getExpandedItems(_memoryInfoTree);

    // === Обновляем данные диска ===
    
    // Обновляем график диска
    static int diskX = 0;
    double totalRead = disksInfo.readBytesPerSec, totalWrite = disksInfo.writeBytesPerSec;
    _diskSeriesRead->append(diskX, totalRead / 1024 / 1024); // в МБ/с
    _diskSeriesWrite->append(diskX, totalWrite / 1024 / 1024);
    diskX++;
    if (_diskSeriesRead->count() > 100) {
        _diskSeriesRead->removePoints(0, 1);
        _diskSeriesWrite->removePoints(0, 1);
    }
    _diskAxisX->setRange(diskX - 100, diskX);

    // === Обновляем информацию о диске ===
    _diskInfoTree->clear();
    auto* diskGroup = new QTreeWidgetItem(_diskInfoTree);
    diskGroup->setText(0, "Диски");
    for (const auto& disk : disksInfo.disks) {
        auto* item = new QTreeWidgetItem(diskGroup);
        diskGroup->setExpanded(true); 
        item->setText(0, QString("%1: %2 ГБ / %3 ГБ")
            .arg(disk.name)
            .arg((disk.totalBytes - disk.freeBytes) / 1024.0 / 1024.0 / 1024.0, 0, 'f', 2)
            .arg(disk.totalBytes / 1024.0 / 1024.0 / 1024.0, 0, 'f', 2));
    }

    // Добавляем общую статистику
    auto* totalReadItem = new QTreeWidgetItem(diskGroup);
    totalReadItem->setText(0, QString("Всего прочитано: %1 МБ/с")
        .arg(disksInfo.readBytesPerSec / 1024 / 1024, 0, 'f', 2));

    auto* totalWriteItem = new QTreeWidgetItem(diskGroup);
    totalWriteItem->setText(0, QString("Всего записано: %1 МБ/с")
        .arg(disksInfo.writeBytesPerSec / 1024 / 1024, 0, 'f', 2));

    // === Обновляем данные сети ===
   
    // Обновляем график сети
    static int networkX = 0;
    double selectedRecv = 0, selectedSent = 0;
    QString selectedAdapter = _networkAdapterCombo->currentData().toString();
    if (networkX != 0) {
        // Статистика для выбранного адаптера
        for (const auto& net : networkInfo) {
            if (net.name == selectedAdapter) {
                selectedRecv = net.receiveBytesPerSec;
                selectedSent = net.sendBytesPerSec;
                break;
            }
        }
    }

    _networkSeriesRecv->append(networkX, selectedRecv / 1024 / 128); // в МБит/с
    _networkSeriesSent->append(networkX, selectedSent / 1024 / 128);
    networkX++;
    if (_networkSeriesRecv->count() > 100) {
        _networkSeriesRecv->removePoints(0, 1);
        _networkSeriesSent->removePoints(0, 1);
    }
    _networkAxisX->setRange(networkX - 100, networkX);

    // === Обновляем информацию о сети ===
    _networkInfoTree->clear();
    auto* networkGroup = new QTreeWidgetItem(_networkInfoTree);
    networkGroup->setText(0, "Сетевые адаптеры");
    networkGroup->setExpanded(true);
    for (const auto& net : networkInfo) 
    {
        if (net.name == selectedAdapter)
        {
            auto* adapterGroup = new QTreeWidgetItem(networkGroup);
            adapterGroup->setText(0, net.description);

            auto* recvItem = new QTreeWidgetItem(adapterGroup);
            recvItem->setText(0, QString("Приём: %1 МБит/с")
                .arg(net.receiveBytesPerSec / 1024 / 128, 0, 'f', 2));

            auto* sentItem = new QTreeWidgetItem(adapterGroup);
            sentItem->setText(0, QString("Отправка: %1 МБит/с")
                .arg(net.sendBytesPerSec / 1024 / 128, 0, 'f', 2));
        }
    }

    // === Обновляем данные GPU ===
    // Обновляем график GPU
    static int gpuX = 0;
    double maxLoad = 0;
    for (const auto& gpu : gpuInfo) {
        QString gpuName = gpu.name;

        if (!_gpuSeriesMap.contains(gpuName)) {
            // Создаём новый график
            auto* series = new QLineSeries();
            series->setName(gpuName); // подпись в легенде
            _gpuChart->addSeries(series);
            series->attachAxis(_gpuAxisX);
            series->attachAxis(_gpuAxisY);
            _gpuSeriesMap[gpuName] = series;
        }
        auto* series = _gpuSeriesMap[gpuName];
        series->append(gpuX, gpu.usage);
    }
    gpuX++;

    // Удаляем лишние графики, если видеокарты исчезли
    QStringList currentNames;
    for (const auto& gpu : gpuInfo) {
        currentNames.append(gpu.name);
    }
    for (auto it = _gpuSeriesMap.begin(); it != _gpuSeriesMap.end();) {
        if (!currentNames.contains(it.key())) {
            _gpuChart->removeSeries(it.value());
            delete it.value();
            it = _gpuSeriesMap.erase(it);
        }
        else {
            ++it;
        }
    }

    gpuX++;
    // Удаляем старые точки, если нужно
    for (auto* series : _gpuSeriesMap) {
        if (series->count() > 100) {
            series->removePoints(0, 1);
        }
    }
    _gpuAxisX->setRange(gpuX - 100, gpuX);

    // === Обновляем информацию о GPU ===
    _gpuInfoTree->clear();
    auto* gpuGroup = new QTreeWidgetItem(_gpuInfoTree);
    gpuGroup->setText(0, "Видеокарты");
    gpuGroup->setExpanded(true);
    for (const auto& gpu : gpuInfo) {
        auto* gpuAdapterGroup = new QTreeWidgetItem(gpuGroup);
        gpuAdapterGroup->setText(0, gpu.name);

        auto* usageItem = new QTreeWidgetItem(gpuAdapterGroup);
        usageItem->setText(0, QString("Загрузка: %1%").arg(gpu.usage));

        auto* memoryItem = new QTreeWidgetItem(gpuAdapterGroup);
        memoryItem->setText(0, QString("Память: %1 ГБ / %2 ГБ")
            .arg(gpu.usedMemoryBytes / 1024.0 / 1024.0 / 1024.0, 0, 'f', 2)
            .arg(gpu.totalMemoryBytes / 1024.0 / 1024.0 / 1024.0, 0, 'f', 2));

        auto* powerItem = new QTreeWidgetItem(gpuAdapterGroup);
        powerItem->setText(0, QString("Потребление: %1 Вт").arg(gpu.powerUsage));

        auto* tempItem = new QTreeWidgetItem(gpuAdapterGroup);
        tempItem->setText(0, QString("Температура: %1 °C").arg(gpu.temperatureCelsius, 0, 'f', 1));

        auto* vendorItem = new QTreeWidgetItem(gpuAdapterGroup);
        vendorItem->setText(0, QString("Производитель: %1").arg(gpu.vendor));
    }

    // Обновляем график CPU
    static int cpuX = 0;
    _cpuSeries->append(cpuX, info.cpuUsage);
    cpuX++;
    if (_cpuSeries->count() > 100) {
        _cpuSeries->removePoints(0, 1);
    }
    _cpuAxisX->setRange(cpuX - 100, cpuX);

    _cpuInfoTree->clear();
    auto* cpuGroup = new QTreeWidgetItem(_cpuInfoTree);
    cpuGroup->setText(0, "Центральный процессор");
    cpuGroup->setExpanded(true);

    auto* usageItem = new QTreeWidgetItem(cpuGroup);
    usageItem->setText(0, QString("Загрузка ЦП: %1%").arg(info.cpuUsage, 0, 'f', 2));

    auto* baseSpeedItem = new QTreeWidgetItem(cpuGroup);
    baseSpeedItem->setText(0, QString("Базовая частота: %1 ГГц").arg(info.baseSpeedGHz, 0, 'f', 2));

    auto* procCountItem = new QTreeWidgetItem(cpuGroup);
    procCountItem->setText(0, QString("Число процессов: %1").arg(info.processCount));

    auto* threadCountItem = new QTreeWidgetItem(cpuGroup);
    threadCountItem->setText(0, QString("Число потоков: %1").arg(info.threadCount));

    auto* coreCountItem = new QTreeWidgetItem(cpuGroup);
    coreCountItem->setText(0, QString("Число ядер: %1").arg(info.coreCount));

    auto* logicalCountItem = new QTreeWidgetItem(cpuGroup);
    logicalCountItem->setText(0, QString("Логические процессоры: %1").arg(info.logicalProcessorCount));

    auto* cacheGroup = new QTreeWidgetItem(cpuGroup);
    cacheGroup->setText(0, "Кэш-память");

    auto* cacheL1Item = new QTreeWidgetItem(cacheGroup);
    cacheL1Item->setText(0, QString("L1: %1 КБ").arg(info.cacheL1KB));

    auto* cacheL2Item = new QTreeWidgetItem(cacheGroup);
    cacheL2Item->setText(0, QString("L2: %1 МБ").arg(info.cacheL2KB / 1024));

    auto* cacheL3Item = new QTreeWidgetItem(cacheGroup);
    cacheL3Item->setText(0, QString("L3: %1 МБ").arg(info.cacheL3KB / 1024));

    // Загрузка ядер
    auto* coresGroup = new QTreeWidgetItem(cpuGroup);
    coresGroup->setText(0, "Загрузка ядер");
    for (int i = 0; i < info.cpuCoreUsage.size(); i++) {
        auto* coreItem = new QTreeWidgetItem(coresGroup);
        coreItem->setText(0, QString("Ядро %1: %2%").arg(i + 1).arg(info.cpuCoreUsage[i], 0, 'f', 2));
    }

    // === Обновляем данные Памяти ===
    static int memoryX = 0;
    _memorySeriesUsed->append(memoryX, (double)info.usedMemory / (double)info.totalMemory * 100.0);
    memoryX++;
    if (_memorySeriesUsed->count() > 100) {
        _memorySeriesUsed->removePoints(0, 1);
    }
    _memoryAxisX->setRange(memoryX - 100, memoryX);

    // === Обновляем информацию о Памяти ===
    _memoryInfoTree->clear();
    auto* memoryGroup = new QTreeWidgetItem(_memoryInfoTree);
    memoryGroup->setText(0, "Оперативная память");
    memoryGroup->setExpanded(true);

    auto* memoryItem = new QTreeWidgetItem(memoryGroup);
    memoryItem->setText(0, QString("Использовано: %1 ГБ / %2 ГБ (%3%)")
        .arg(info.usedMemory / 1024.0 / 1024.0 / 1024.0, 0, 'f', 2)
        .arg(info.totalMemory / 1024.0 / 1024.0 / 1024.0, 0, 'f', 2)
        .arg((double)info.usedMemory / info.totalMemory * 100.0, 0, 'f', 2));
   
    setExpandedItems(_diskInfoTree, diskExpanded);
    setExpandedItems(_networkInfoTree, networkExpanded);
    setExpandedItems(_gpuInfoTree, gpuExpanded);
    setExpandedItems(_cpuInfoTree, cpuExpanded);
    setExpandedItems(_memoryInfoTree, memoryExpanded);
}

void WinTop::killSelectedProcesses()
{
    if (_selectedProcessID != 0) {
        QMessageBox::StandardButton reply;
        
        QString procName = _selectedProcessName;
        QModelIndexList selected = _processTableView->selectionModel()->selectedRows();
        if (!selected.isEmpty()) {
            QModelIndex proxyIndex = selected.first();
            QModelIndex sourceIndex = _proxyModel->mapToSource(proxyIndex);
            procName = _processModel->getProcessByRow(sourceIndex.row()).name;
        }

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
    QList<ProcessInfo> processes = _lastProcesses;
    
    ProcessDetails details = _processControl.get()->getProcessDetails(pid, processes);

    // Создаём и показываем диалог
    ProcessDetailsDialog dialog(this);
    dialog.setProcessControl(_processControl.get());
    dialog.setProcessDetails(details);
    dialog.exec();
}

void WinTop::onServiceContextMenu(const QPoint& pos) {
    QModelIndex proxyIndex = m_servicesTableView->indexAt(pos);
    if (!proxyIndex.isValid()) {
        return;
    }
    QModelIndex sourceIndex = _servicesProxyModel->mapToSource(proxyIndex);
    // Получаем имя службы из модели
    m_selectedServiceName = m_servicesModel->data(sourceIndex.siblingAtColumn(0), Qt::DisplayRole).toString(); // колонка "Имя"

    if (m_selectedServiceName.isEmpty()) {
        return;
    }

    // Показываем меню
    m_serviceContextMenu->exec(m_servicesTableView->viewport()->mapToGlobal(pos));
}

void WinTop::onDataReady(const UpdateData& data)
{
    QWidget* currentWidget = _tabWidget->currentWidget();
    _lastProcesses = data.processes;
    _lastServices = data.services;
    _lastNetworkInterfaces = data.networkInterfaces;
    // Обновляем таблицу
    if (_processModel && (currentWidget == _processesTab)) {
        _processModel->updateDataPartial(data.processes);
    }

    if (_processTreeModel && (currentWidget == _treeTab)) {
        _processTreeModel->updateData(data.processes);
    }

    if (m_servicesModel && (currentWidget == m_servicesTab)) {
        m_servicesModel->updateData(data.services);
    }

    // Обновляем вкладку производительности
    updatePerformanceTab(data.systemInfo, data.disks, data.networkInterfaces, data.gpus);
    static int i = 0;
    qDebug() << "Данные ВЫВЕДЕНЫ " << i++;
}

QStringList WinTop::getExpandedItems(QTreeWidget* tree) {
    QStringList expanded;
    std::function<void(QTreeWidgetItem*)> traverse;
    traverse = [&](QTreeWidgetItem* item) {
        if (item->isExpanded()) {
            // Сохраняем путь к элементу (например, "Группа/Подгруппа/Элемент")
            QStringList path;
            QTreeWidgetItem* current = item;
            while (current) {
                path.prepend(current->text(0));
                current = current->parent();
            }
            expanded.append(path.join("/"));
        }
        for (int i = 0; i < item->childCount(); ++i) {
            traverse(item->child(i));
        }
    };

    for (int i = 0; i < tree->topLevelItemCount(); ++i) {
        traverse(tree->topLevelItem(i));
    }
    return expanded;
}

void WinTop::setExpandedItems(QTreeWidget* tree, const QStringList& items) {
    std::function<void(QTreeWidgetItem*, const QStringList&)> traverse;
    traverse = [&](QTreeWidgetItem* item, const QStringList& paths) {
        QStringList path;
        QTreeWidgetItem* current = item;
        while (current) {
            path.prepend(current->text(0));
            current = current->parent();
        }
        QString currentPath = path.join("/");

        if (paths.contains(currentPath)) {
            item->setExpanded(true);
        }

        for (int i = 0; i < item->childCount(); ++i) {
            traverse(item->child(i), paths);
        }
        };

    for (int i = 0; i < tree->topLevelItemCount(); ++i) {
        traverse(tree->topLevelItem(i), items);
    }
}

void WinTop::setupStyles() {
    QString style = R"(
        QMainWindow {
            background-color: #f5f5f5;
        }

        QTabWidget::pane {
            border: 1px solid #dcdcdc;
            background-color: #ffffff;
        }

        QTabBar::tab {
            background-color: #e0e0e0;
            color: #333333;
            padding: 8px 16px;
            border: 1px solid #dcdcdc;
            border-bottom-color: #dcdcdc;
            border-top-left-radius: 4px;
            border-top-right-radius: 4px;
            margin-right: 2px;
        }

        QTabBar::tab:hover {
            background-color: #f0f0f0;
        }

        QTreeWidget {
            background-color: #ffffff;
            alternate-background-color: #f9f9f9;
            border: 1px solid #dcdcdc;
            font-size: 12px;
        }

        QTreeWidget::item {
            height: 20px;
        }


        QTableView {
            background-color: #ffffff;
            alternate-background-color: #f9f9f9;
            gridline-color: #e0e0e0;
            border: 1px solid #dcdcdc;
        }

        QTableView::item {
            padding: 4px;
            border: none;
        }

        QHeaderView::section {
            background-color: #f0f0f0;
            color: #333333;
            padding: 6px;
            border: 1px solid #dcdcdc;
            font-weight: bold;
        }

        QTreeView {
            background-color: #ffffff;
            alternate-background-color: #f9f9f9;
            border: 1px solid #dcdcdc;
        }

        QChartView {
            border: 1px solid #dcdcdc;
            background-color: #ffffff;
        }

        QScrollArea {
            border: 1px solid #dcdcdc;
            background-color: #ffffff;
        }

        QScrollBar:vertical {
            background-color: #f0f0f0;
            width: 12px;
            border-radius: 4px;
        }

        QScrollBar::handle:vertical {
            background-color: #c0c0c0;
            border-radius: 4px;
            min-height: 20px;
        }

        QScrollBar::handle:vertical:hover {
            background-color: #a0a0a0;
        }

        QComboBox {
            background-color: #ffffff;
            border: 1px solid #dcdcdc;
            padding: 4px;
            border-radius: 4px;
        }

        QComboBox::drop-down {
            border: none;
        }

        QComboBox::down-arrow {
            image: url(down_arrow.png); /* Путь к иконке */
        }

        QLineEdit {
            background-color: #ffffff;
            border: 1px solid #dcdcdc;
            padding: 4px;
            border-radius: 4px;
        }

        QMenuBar {
            background-color: #f5f5f5;
            spacing: 3px;
        }

        QMenuBar::item {
            background-color: transparent;
            padding: 4px 8px;
        }

        QMenuBar::item:pressed {
            background-color: #cce6ff;
        }

        QMenu {
            background-color: #ffffff;
            border: 1px solid #dcdcdc;
        }

        QMenu::item {
            padding: 4px 20px;
        }

        QMenu::separator {
            height: 1px;
            background-color: #dcdcdc;
        }
    )";

    qApp->setStyleSheet(style);
}