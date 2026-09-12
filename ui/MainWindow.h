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
    void initUI();
    QString loadQss();

    Ui::MainWindowClass *ui;

	//指向各个页面的指针
    AcquisitionPage* m_acquisitionPage = nullptr;
	AnalysisPage* m_analysisPage = nullptr;
	LogPage* m_logPage = nullptr;
    ConfigPage* m_configPage = nullptr;

    //ConfigManager 测试函数，读取配置文件并打印到控制台
    //void testConfigManager();
};

