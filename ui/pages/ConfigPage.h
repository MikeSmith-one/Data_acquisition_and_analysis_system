#pragma once

#include <QtWidgets/QWidget>
#include "ui_ConfigPage.h"

QT_BEGIN_NAMESPACE
namespace Ui { class ConfigPageClass; };
QT_END_NAMESPACE

class ConfigPage : public QWidget
{
    Q_OBJECT

public:
    ConfigPage(QWidget* parent = nullptr);
    ~ConfigPage();

private:
    Ui::ConfigPageClass* ui;
};

