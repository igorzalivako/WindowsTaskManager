#pragma once
#include <QDialog>
#include <QVBoxLayout>
#include <QFormLayout>
#include <QLabel>
#include <QTextEdit>
#include <QDialogButtonBox>
#include <QScrollArea>
#include "DataStructs.h"

class ProcessDetailsDialog : public QDialog {
    Q_OBJECT

public:
    explicit ProcessDetailsDialog(QWidget* parent = nullptr);
    void setProcessDetails(const ProcessDetails& details);

private:
    void setupUI();

    ProcessDetails _details;

    // Основная информация
    QLabel* _pidLabel;
    QLabel* _nameLabel;
    QLabel* _pathLabel;
    QLabel* _parentPIDLabel;

    // Ресурсы
    QLabel* _cpuLabel;
    QLabel* _memoryLabel;
    QLabel* _workingSetLabel;

    // Системная информация
    QLabel* _threadCountLabel;
    QLabel* _startTimeLabel;
    QLabel* _childProcessCountLabel;
    QLabel* _handleCountLabel;

    // Виджеты
    QScrollArea* _scrollArea;
    QWidget* _scrollWidget;
    QFormLayout* _formLayout;
    QDialogButtonBox* _buttonBox;
};
