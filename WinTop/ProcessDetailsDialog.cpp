#include "ProcessDetailsDialog.h"
#include <QDateTime>
#include <QApplication>
#include <QStyle>

ProcessDetailsDialog::ProcessDetailsDialog(QWidget* parent)
    : QDialog(parent)
{
    setupUI();
    setupStyles();
}

void ProcessDetailsDialog::setupUI()
{
    setWindowTitle("Свойства процесса");
    resize(600, 500);

    auto* mainLayout = new QVBoxLayout(this);

    // Верхняя часть: иконка и имя
    auto* topLayout = new QHBoxLayout();
    _processIconLabel = new QLabel();
    _processIconLabel->setFixedSize(32, 32);
    _processIconLabel->setScaledContents(true);

    _processNameLabel = new QLabel();
    _processNameLabel->setStyleSheet("font-size: 14px; font-weight: bold;");

    topLayout->addWidget(_processIconLabel);
    topLayout->addWidget(_processNameLabel);
    topLayout->addStretch();

    mainLayout->addLayout(topLayout);

    // Дерево с информацией
    _detailsTree = new QTreeWidget();
    _detailsTree->setHeaderHidden(true);
    _detailsTree->setRootIsDecorated(true); // показываем иконки группы
    _detailsTree->setAlternatingRowColors(true);

    mainLayout->addWidget(_detailsTree);

    // Кнопки
    _buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok, Qt::Horizontal, this);
    mainLayout->addWidget(_buttonBox);

    connect(_buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
}

void ProcessDetailsDialog::setupStyles()
{
    QString style = R"(
        QTreeWidget {
            background-color: #ffffff;
            alternate-background-color: #f9f9f9;
            border: 1px solid #dcdcdc;
            font-size: 12px;
        }

        QTreeWidget::item {
            height: 24px;
        }

        QTreeWidget::item:selected {
            background-color: #e6f3ff;
            color: #000000;
        }

        QDialogButtonBox {
            border-top: 1px solid #dcdcdc;
            padding: 8px;
        }
    )";

    setStyleSheet(style);
}

void ProcessDetailsDialog::setProcessDetails(const ProcessDetails& details) {
    _details = details;

    // Устанавливаем иконку и имя
    if (_processControl) {
        QIcon icon = _processControl->getProcessIcon(details.pid);
        _processIconLabel->setPixmap(icon.pixmap(20, 20));
    }
    _processNameLabel->setText(QString("%1 (PID: %2)").arg(details.name).arg(details.pid));

    // Заполняем дерево
    _detailsTree->clear();

    // === Основная информация ===
    auto* mainGroup = new QTreeWidgetItem(_detailsTree);
    mainGroup->setText(0, "Основная информация");
    mainGroup->setExpanded(true);

    auto* pidItem = new QTreeWidgetItem(mainGroup);
    pidItem->setText(0, QString("PID: %1").arg(details.pid));

    auto* nameItem = new QTreeWidgetItem(mainGroup);
    nameItem->setText(0, QString("Имя: %1").arg(details.name));

    auto* pathItem = new QTreeWidgetItem(mainGroup);
    pathItem->setText(0, QString("Путь: %1").arg(details.path));

    auto* parentItem = new QTreeWidgetItem(mainGroup);
    parentItem->setText(0, QString("Родительский PID: %1").arg(details.parentPID));

    // === Ресурсы ===
    auto* resourcesGroup = new QTreeWidgetItem(_detailsTree);
    resourcesGroup->setText(0, "Ресурсы");
    resourcesGroup->setExpanded(true);

    auto* cpuItem = new QTreeWidgetItem(resourcesGroup);
    cpuItem->setText(0, QString("Загрузка ЦП: %1%").arg(details.cpuUsage, 0, 'f', 2));

    auto* memoryItem = new QTreeWidgetItem(resourcesGroup);
    memoryItem->setText(0, QString("Память (Private): %1 МБ").arg(details.memoryUsage / 1024 / 1024));

    auto* wsItem = new QTreeWidgetItem(resourcesGroup);
    wsItem->setText(0, QString("Рабочий набор: %1 МБ").arg(details.workingSetSize / 1024 / 1024));

    // === Системная информация ===
    auto* systemGroup = new QTreeWidgetItem(_detailsTree);
    systemGroup->setText(0, "Системная информация");
    systemGroup->setExpanded(true);

    auto* threadsItem = new QTreeWidgetItem(systemGroup);
    threadsItem->setText(0, QString("Количество потоков: %1").arg(details.threadCount));

    auto* startTimeItem = new QTreeWidgetItem(systemGroup);
    startTimeItem->setText(0, QString("Время запуска: %1").arg(details.startTime.toString("dd.MM.yyyy hh:mm:ss")));

    auto* childrenItem = new QTreeWidgetItem(systemGroup);
    childrenItem->setText(0, QString("Дочерние процессы: %1").arg(details.childProcessesCount));

    auto* handlesItem = new QTreeWidgetItem(systemGroup);
    handlesItem->setText(0, QString("Количество дескрипторов: %1").arg(details.handleCount));

    // === Дополнительная информация ===
    auto* additionalGroup = new QTreeWidgetItem(_detailsTree);
    additionalGroup->setText(0, "Дополнительная информация");
    additionalGroup->setExpanded(true);

    auto* userItem = new QTreeWidgetItem(additionalGroup);
    userItem->setText(0, QString("Пользователь: %1").arg(details.userName));

    auto* priorityItem = new QTreeWidgetItem(additionalGroup);
    priorityItem->setText(0, QString("Приоритет: %1").arg(details.priorityClass));
}

void ProcessDetailsDialog::setProcessControl(IProcessControl* processControl)
{
    _processControl = processControl;
}