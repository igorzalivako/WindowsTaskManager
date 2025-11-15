#include "WinTop.h"
#include "WindowsSystemMonitor.h"
#include "WindowsProcessControl.h"
#include "WindowsProcessTreeBuilder.h"
#include "WindowsDiskMonitor.h"
#include "WindowsNetworkMonitor.h"
#include <WindowsGPUMonitor.h>

WinTop::WinTop(QWidget *parent)
    : QMainWindow(parent)
{
    _processControl = std::make_unique<WindowsProcessControl>();
    _treeBuilder = std::make_unique<WindowsProcessTreeBuilder>();
    _diskMonitor = std::make_unique<WindowsDiskMonitor>();
    _networkMonitor = std::make_unique<WindowsNetworkMonitor>();
    _gpuMonitor = std::make_unique<WindowsGPUMonitor>();
    _monitor = std::make_unique<WindowsSystemMonitor>(_diskMonitor.get(), _networkMonitor.get(), _gpuMonitor.get());
    connect(&_updateTimer, &QTimer::timeout, this, &WinTop::updateData);
    setupUI();
    _updateTimer.start(1000);
}

WinTop::~WinTop()
{}

void WinTop::setupUI()
{
    
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

    connect(_filterLineEdit, &QLineEdit::textChanged, this, &WinTop::onFilterLineEditTextChanged);

    processLayout->addWidget(_filterLineEdit);
    processLayout->addWidget(_processTableView);

    // Дерево процессов
    setUpProcessTree();

    // Контекстное меню процесса
    setUpProcessInfoContextMenu();

    setUpPerformanceTab();

    // === Add Tabs ===
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

    /*// Обновляем метки
    _osLabel->setText("Windows 10");
    _ramLabel->setText(QString("%1 / %2 GB").arg(info.usedMemory / 1024 / 1024 / 1024.0, 0, 'f', 1)
        .arg(info.totalMemory / 1024 / 1024 / 1024.0, 0, 'f', 1));*/

    updatePerformanceTab(info);

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

void WinTop::setUpPerformanceTab()
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

    // Информация о диске (внизу)
    _diskInfoWidget = new QWidget();
    auto* diskInfoLayout = new QVBoxLayout(_diskInfoWidget);

    // QLabel для отображения информации
    auto* diskInfoLabel = new QLabel("Информация о диске появится здесь...");
    diskInfoLabel->setObjectName("diskInfoLabel"); // для удобства поиска
    diskInfoLayout->addWidget(diskInfoLabel);

    diskPerfLayout->addWidget(_diskInfoWidget);

    _performanceStack->addWidget(_diskPerformancePage);

    setUpNetworkPreformanceTab();
    
    setUpGPUPerformanceTab();

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

void WinTop::setUpNetworkPreformanceTab()
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
    _networkInfoWidget = new QWidget();
    auto* networkInfoLayout = new QVBoxLayout(_networkInfoWidget);
    auto networkInfoLabel = new QLabel("Информация о сети появится здесь...");
    networkInfoLabel->setObjectName("networkInfoLabel");
    networkInfoLayout->addWidget(networkInfoLabel);
    networkPerfLayout->addWidget(_networkInfoWidget);

    _performanceStack->addWidget(_networkPerformancePage);

    // ... (добавляем остальные страницы)

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
                updateNetworkAdapterList();
            } 
            else if (resource == "gpu")
            {
                _performanceStack->setCurrentWidget(m_gpuPerformancePage);
            }
            else if (resource == "cpu")
            {
                _performanceStack->setCurrentWidget(m_cpuPerformancePage);
            }
            else if (resource == "memory")
            {
                _performanceStack->setCurrentWidget(m_memoryPerformancePage);
            }
            // ... другие ресурсы
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
    m_gpuPerformancePage = new QWidget();
    auto* gpuPerfLayout = new QVBoxLayout(m_gpuPerformancePage);

    // График
    m_gpuChart = new QChart();
    m_gpuChart->legend()->show();

    m_gpuAxisX = new QValueAxis;
    m_gpuAxisY = new QValueAxis;
    m_gpuAxisX->setRange(0, 100);
    m_gpuAxisY->setRange(0, 100);
    m_gpuChart->addAxis(m_gpuAxisX, Qt::AlignBottom);
    m_gpuChart->addAxis(m_gpuAxisY, Qt::AlignLeft);

    m_gpuChartView = new QChartView(m_gpuChart);
    m_gpuChartView->setRenderHint(QPainter::Antialiasing);

    gpuPerfLayout->addWidget(m_gpuChartView);

    // Информация о GPU (внизу)
    m_gpuInfoWidget = new QWidget();
    auto* gpuInfoLayout = new QVBoxLayout(m_gpuInfoWidget);
    auto gpuInfoLabel = new QLabel("Информация о сети появится здесь...");
    gpuInfoLabel->setObjectName("gpuInfoLabel");
    gpuInfoLayout->addWidget(gpuInfoLabel);
    gpuPerfLayout->addWidget(m_gpuInfoWidget);
    _performanceStack->addWidget(m_gpuPerformancePage);
}

void WinTop::setUpCPUPerformanceTab()
{
    m_cpuPerformancePage = new QWidget();
    auto* cpuPerfLayout = new QVBoxLayout(m_cpuPerformancePage);

    // График
    m_cpuChart = new QChart();
    m_cpuSeries = new QLineSeries();
    m_cpuSeries->setName("Загрузка ЦП %");
    m_cpuChart->addSeries(m_cpuSeries);
    m_cpuChart->legend()->show();

    m_cpuAxisX = new QValueAxis;
    m_cpuAxisY = new QValueAxis;
    m_cpuAxisX->setRange(0, 100);
    m_cpuAxisY->setRange(0, 100);
    m_cpuChart->addAxis(m_cpuAxisX, Qt::AlignBottom);
    m_cpuChart->addAxis(m_cpuAxisY, Qt::AlignLeft);
    m_cpuSeries->attachAxis(m_cpuAxisX);
    m_cpuSeries->attachAxis(m_cpuAxisY);

    m_cpuChartView = new QChartView(m_cpuChart);
    m_cpuChartView->setRenderHint(QPainter::Antialiasing);

    cpuPerfLayout->addWidget(m_cpuChartView);

    // Информация о CPU (внизу)
    m_cpuInfoWidget = new QWidget();
    auto* cpuInfoLayout = new QVBoxLayout(m_cpuInfoWidget);
    auto cpuInfoLabel = new QLabel("Информация о CPU появится здесь...");
    cpuInfoLabel->setObjectName("cpuInfoLabel");
    cpuInfoLayout->addWidget(cpuInfoLabel);
    cpuPerfLayout->addWidget(m_cpuInfoWidget);

    _performanceStack->addWidget(m_cpuPerformancePage);
}

void WinTop::setUpMemoryPerformanceTab()
{
    m_memoryPerformancePage = new QWidget();
    auto* memoryPerfLayout = new QVBoxLayout(m_memoryPerformancePage);

    // График
    m_memoryChart = new QChart();
    m_memorySeriesUsed = new QLineSeries();
    m_memorySeriesUsed->setName("Использовано");
    m_memoryChart->addSeries(m_memorySeriesUsed);
    m_memoryChart->legend()->show();

    m_memoryAxisX = new QValueAxis;
    m_memoryAxisY = new QValueAxis;
    m_memoryAxisX->setRange(0, 100);
    m_memoryAxisY->setRange(0, 100);
    m_memoryChart->addAxis(m_memoryAxisX, Qt::AlignBottom);
    m_memoryChart->addAxis(m_memoryAxisY, Qt::AlignLeft);
    m_memorySeriesUsed->attachAxis(m_memoryAxisX);
    m_memorySeriesUsed->attachAxis(m_memoryAxisY);

    m_memoryChartView = new QChartView(m_memoryChart);
    m_memoryChartView->setRenderHint(QPainter::Antialiasing);

    memoryPerfLayout->addWidget(m_memoryChartView);

    // Информация о Памяти (внизу)
    m_memoryInfoWidget = new QWidget();
    auto* memoryInfoLayout = new QVBoxLayout(m_memoryInfoWidget);
    auto memoryInfoLabel = new QLabel("Информация о RAM появится здесь...");
    memoryInfoLabel->setObjectName("memoryInfoLabel");
    memoryInfoLayout->addWidget(memoryInfoLabel);
    memoryPerfLayout->addWidget(m_memoryInfoWidget);

    _performanceStack->addWidget(m_memoryPerformancePage);
}

void WinTop::updateNetworkAdapterList() {
    auto networkInfo = _networkMonitor->getNetworkInfo();

    _networkAdapterCombo->clear();

    for (const auto& net : networkInfo) {
        // Добавляем только активные адаптеры
        _networkAdapterCombo->addItem(QString("%1 (%2)").arg(net.description), net.name);
    }
}

void WinTop::updatePerformanceTab(const SystemInfo& info)
{
    // === Обновляем данные диска ===
    auto diskInfo = _diskMonitor->getDiskInfo();
    auto processDiskInfo = _diskMonitor->getProcessDiskInfo();

    // Обновляем график диска
    static int diskX = 0;
    double totalRead = diskInfo.readBytesPerSec, totalWrite = diskInfo.writeBytesPerSec;

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
    for (const auto& disk : diskInfo.disks) {
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

    // === Обновляем данные сети ===
    auto networkInfo = _networkMonitor->getNetworkInfo();

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
    QString networkInfoText = "Сетевые интерфейсы:\n";
    for (const auto& net : networkInfo) {
        if (net.name == selectedAdapter) {
            networkInfoText += QString("(%1): Приём %2 МБит/с, Отправка %3 МБит/с\n")
                .arg(net.description)
                .arg(net.receiveBytesPerSec / 1024 / 128, 0, 'f', 2)
                .arg(net.sendBytesPerSec / 1024 / 128, 0, 'f', 2);
        }
    }

    // Найдём QLabel и обновим текст
    auto* networkInfoLabel = _networkInfoWidget->findChild<QLabel*>("networkInfoLabel");
    if (networkInfoLabel) {
        networkInfoLabel->setText(networkInfoText);
    }

    // === Обновляем данные GPU ===
    auto gpuInfo = _gpuMonitor->getGPUInfo();
    auto i = _gpuMonitor->getProcessGPUInfo();
    // Обновляем график GPU
    static int gpuX = 0;
    double maxLoad = 0;
    for (const auto& gpu : gpuInfo) {
        QString gpuName = gpu.name;

        if (!m_gpuSeriesMap.contains(gpuName)) {
            // Создаём новый график
            auto* series = new QLineSeries();
            series->setName(gpuName); // подпись в легенде
            m_gpuChart->addSeries(series);
            series->attachAxis(m_gpuAxisX);
            series->attachAxis(m_gpuAxisY);
            m_gpuSeriesMap[gpuName] = series;
        }
        auto* series = m_gpuSeriesMap[gpuName];
        series->append(gpuX, gpu.usage);
    }
    gpuX++;

    // Удаляем лишние графики, если видеокарты исчезли
    QStringList currentNames;
    for (const auto& gpu : gpuInfo) {
        currentNames.append(gpu.name);
    }
    for (auto it = m_gpuSeriesMap.begin(); it != m_gpuSeriesMap.end();) {
        if (!currentNames.contains(it.key())) {
            m_gpuChart->removeSeries(it.value());
            delete it.value();
            it = m_gpuSeriesMap.erase(it);
        }
        else {
            ++it;
        }
    }

    gpuX++;
    // Удаляем старые точки, если нужно
    for (auto* series : m_gpuSeriesMap) {
        if (series->count() > 100) {
            series->removePoints(0, 1);
        }
    }
    m_gpuAxisX->setRange(gpuX - 100, gpuX);

    // === Обновляем информацию о GPU ===
    QString gpuInfoText = "Видеокарты:\n";
    for (const auto& gpu : gpuInfo) {
        gpuInfoText += QString("%1: Загрузка %2%, Память %3 ГБ / %4 ГБ, Производитель %5, Температура: %7 C, Текущее энергопотребление: %6 Вт\n")
            .arg(gpu.name)
            .arg(gpu.usage)
            .arg(gpu.usedMemoryBytes / 1024.0 / 1024.0 / 1024.0, 0, 'f', 2)
            .arg(gpu.totalMemoryBytes / 1024.0 / 1024.0 / 1024.0, 0, 'f', 2)
            .arg(gpu.vendor)
            .arg(gpu.powerUsage)
            .arg(gpu.temperatureCelsius);
    }

    // Найдём QLabel и обновим текст
    auto* gpuInfoLabel = m_gpuInfoWidget->findChild<QLabel*>("gpuInfoLabel");
    if (gpuInfoLabel) {
        gpuInfoLabel->setText(gpuInfoText);
    }
    
    // Обновляем график CPU
    static int cpuX = 0;
    m_cpuSeries->append(cpuX, info.cpuUsage);
    cpuX++;
    if (m_cpuSeries->count() > 100) {
        m_cpuSeries->removePoints(0, 1);
    }
    m_cpuAxisX->setRange(cpuX - 100, cpuX);

    // === Обновляем информацию о CPU ===
    QString cpuInfoText = QString(
        "Загрузка ЦП: %1%\n"
        "Базовая частота: %2 ГГц\n"
        "Число процессов: %3\n"
        "Число потоков: %4\n"
        "Число ядер: %5\n"
        "Число логических процессоров: %6\n"
        "Кэш L1: %7 КБ\n"
        "Кэш L2: %8 КБ\n"
        "Кэш L3: %9 МБ"
        "\nЗагрузка ядер:"
    ).arg(info.cpuUsage, 0, 'f', 2)
        .arg(info.baseSpeedGHz, 0, 'f', 2)
        .arg(info.processCount)
        .arg(info.threadCount)
        .arg(info.coreCount)
        .arg(info.logicalProcessorCount)
        .arg(info.cacheL1KB)
        .arg(info.cacheL2KB)
        .arg(info.cacheL3KB / 1024);
    // Добавь сюда другую информацию о CPU, если нужно

    for (int i = 0; i < info.cpuCoreUsage.size(); i++) {
        cpuInfoText += QString("\n  Ядро %1: %2%").arg(i).arg(info.cpuCoreUsage[i], 0, 'f', 2);
    }

    // Найдём QLabel и обновим текст
    auto* cpuInfoLabel = m_cpuInfoWidget->findChild<QLabel*>("cpuInfoLabel");
    if (cpuInfoLabel) {
        cpuInfoLabel->setText(cpuInfoText);
    }

    // === Обновляем данные Памяти ===
    // Обновляем график Памяти
    static int memoryX = 0;
    m_memorySeriesUsed->append(memoryX, (double)info.usedMemory / (double)info.totalMemory * 100.0);
    memoryX++;
    if (m_memorySeriesUsed->count() > 100) {
        m_memorySeriesUsed->removePoints(0, 1);
    }
    m_memoryAxisX->setRange(memoryX - 100, memoryX);

    // === Обновляем информацию о Памяти ===
    QString memoryInfoText = QString(
        "Память: %1 ГБ / %2 ГБ (использовано %3%)\n"
    ).arg(info.usedMemory / 1024.0 / 1024.0 / 1024.0, 0, 'f', 2)
        .arg(info.totalMemory / 1024.0 / 1024.0 / 1024.0, 0, 'f', 2)
        .arg((double)info.usedMemory / (double)info.totalMemory * 100.0, 0, 'f', 2);

    // Найдём QLabel и обновим текст
    auto* memoryInfoLabel = m_memoryInfoWidget->findChild<QLabel*>("memoryInfoLabel");
    if (memoryInfoLabel) {
        memoryInfoLabel->setText(memoryInfoText);
    }

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