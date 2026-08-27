#pragma once

#include <QtWidgets/QWidget>
#include "ui_MainWindow.h"
#include "pages/AcquisitionPage.h"
#include "pages/AnalysisPage.h"
#include "pages/ConfigPage.h"
#include "pages/LogPage.h"

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindowClass; };
QT_END_NAMESPACE

class MainWindow : public QWidget
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private:
    Ui::MainWindowClass *ui;
    AcquisitionPage* m_acquisitionPage = nullptr;
	AnalysisPage* m_analysisPage = nullptr;
	LogPage* m_logPage = nullptr;
    ConfigPage* m_configPage = nullptr;
};

