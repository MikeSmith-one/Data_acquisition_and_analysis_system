#pragma once

#include <QtWidgets/QWidget>
#include "ui_LogPage.h"

QT_BEGIN_NAMESPACE
namespace Ui { class LogPageClass; };
QT_END_NAMESPACE

class LogPage : public QWidget
{
    Q_OBJECT

public:
    LogPage(QWidget* parent = nullptr);
    ~LogPage();

private:
    Ui::LogPageClass* ui;
};

