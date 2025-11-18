#include "ProcessDetailsDialog.h"

#include <QDateTime>
#include <QString>

ProcessDetailsDialog::ProcessDetailsDialog(QWidget* parent)
    : QDialog(parent)
{
    setupUI();
}

void ProcessDetailsDialog::setupUI() 
{
    setWindowTitle("Свойства процесса");
    resize(500, 400);

    auto* mainLayout = new QVBoxLayout(this);

    // Создаём Scroll Area для прокрутки
    _scrollArea = new QScrollArea(this);
    _scrollArea->setWidgetResizable(true);

    _scrollWidget = new QWidget(_scrollArea);
    _formLayout = new QFormLayout(_scrollWidget);

    // === Основная информация ===
    _formLayout->addRow("PID:", _pidLabel = new QLabel());
    _formLayout->addRow("Имя:", _nameLabel = new QLabel());
    _formLayout->addRow("Путь:", _pathLabel = new QLabel());
    _formLayout->addRow("Родительский PID:", _parentPIDLabel = new QLabel());

    // === Ресурсы ===
    _formLayout->addRow("Загрузка ЦП:", _cpuLabel = new QLabel());
    _formLayout->addRow("Память (Private):", _memoryLabel = new QLabel());
    _formLayout->addRow("Рабочий набор:", _workingSetLabel = new QLabel());

    // === Системная информация ===
    _formLayout->addRow("Количество потоков:", _threadCountLabel = new QLabel());
    _formLayout->addRow("Время запуска:", _startTimeLabel = new QLabel());
    _formLayout->addRow("Дочерние процессы:", _childProcessCountLabel = new QLabel());
    _formLayout->addRow("Количество дескрипторов:", _handleCountLabel = new QLabel());

    _scrollArea->setWidget(_scrollWidget);
    mainLayout->addWidget(_scrollArea);

    // === Кнопки ===
    _buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok, Qt::Horizontal, this);
    mainLayout->addWidget(_buttonBox);

    connect(_buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
}

void ProcessDetailsDialog::setProcessDetails(const ProcessDetails& details) {
    _details = details;

    _pidLabel->setText(QString::number(details.pid));
    _nameLabel->setText(details.name);
    _pathLabel->setText(details.path);
    _parentPIDLabel->setText(QString::number(details.parentPID));

    _cpuLabel->setText(QString::number(details.cpuUsage, 'f', 2) + "%");
    _memoryLabel->setText(QString::number(details.memoryUsage / 1024 / 1024) + " MB");
    _workingSetLabel->setText(QString::number(details.workingSetSize / 1024 / 1024) + " MB");

    _threadCountLabel->setText(QString::number(details.threadCount));
    _startTimeLabel->setText(details.startTime.toString("dd.MM.yyyy hh:mm:ss.zzz"));
    _childProcessCountLabel->setText(QString::number(details.childProcessesCount));
    _handleCountLabel->setText(QString::number(details.handleCount));
}
