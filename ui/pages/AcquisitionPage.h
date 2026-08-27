#pragma once

#include <QtWidgets/QWidget>
#include "ui_AcquisitionPage.h"

QT_BEGIN_NAMESPACE
namespace Ui { class AcquisitionPageClass; };
QT_END_NAMESPACE

class AcquisitionPage : public QWidget
{
    Q_OBJECT

public:
    AcquisitionPage(QWidget* parent = nullptr);
    ~AcquisitionPage();

private:
    Ui::AcquisitionPageClass* ui;
};

