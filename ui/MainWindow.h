#pragma once

#include <QtWidgets/QWidget>
#include "ui_MainWindow.h"
#include "pages/AcquisitionPage.h"
#include "pages/AnalysisPage.h"
#include "pages/ConfigPage.h"
#include "pages/LogPage.h"
#include <QThread>
#include "network/ModbusManager.h"
#include "./pages/ConfigManager.h"
#include "database/DatabaseManager.h"
#include <QMessageBox>

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindowClass; };
QT_END_NAMESPACE

class MainWindow : public QWidget
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void onStationDataReceived(const StationData& data); // 处理实时数据到ui

    // 测试用：打印接收到的工位数据到控制台
    void debugPrintStationData(const StationData& data);

    //接收报警信号的槽函数
    void onAlarmTriggered(const AlarmInfo& info);

private:
    //Modbus TCP通信子线程
    QThread* m_modbusThread = nullptr;
    ModbusManager* m_modbusManager = nullptr;

    //Database 存储子线程
    QThread* m_dbThread = nullptr;
    DatabaseManager* m_dbManager = nullptr;

    void initUI();
    QString loadQss();

    Ui::MainWindowClass *ui;

	//指向各个页面的指针
    AcquisitionPage* m_acquisitionPage = nullptr;
	AnalysisPage* m_analysisPage = nullptr;
	LogPage* m_logPage = nullptr;
    ConfigPage* m_configPage = nullptr;

    //全局唯一的非模态报警弹窗指针
    QMessageBox* m_alarmBox = nullptr;

    //ConfigManager 测试函数，读取配置文件并打印到控制台
    //void testConfigManager();
};

