#pragma once

#include <QtWidgets/QWidget>
#include "ui_AnalysisPage.h"

QT_BEGIN_NAMESPACE
namespace Ui { class AnalysisPageClass; };
QT_END_NAMESPACE

class AnalysisPage : public QWidget
{
    Q_OBJECT

public:
    AnalysisPage(QWidget* parent = nullptr);
    ~AnalysisPage();

private:
    Ui::AnalysisPageClass* ui;
};

