#include "MainWindow.h"

MainWindow::MainWindow(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::MainWindowClass())
{
    ui->setupUi(this);

    //4、参数配置界面
    m_configPage = new ConfigPage(this);
    ui->stackedWidget->addWidget(m_configPage);
    ui->stackedWidget->setCurrentWidget(m_configPage);

    connect(ui->configBtn, &QPushButton::clicked, this, [=]() {
        ui->stackedWidget->setCurrentWidget(m_configPage);
        });

    //3、日志系统界面
    m_logPage = new LogPage(this);
    ui->stackedWidget->addWidget(m_logPage);
    ui->stackedWidget->setCurrentWidget(m_logPage);

    connect(ui->logBtn, &QPushButton::clicked, this, [=]() {
        ui->stackedWidget->setCurrentWidget(m_logPage);
        });

    //2、数据分析界面
    m_analysisPage = new AnalysisPage(this);
    ui->stackedWidget->addWidget(m_analysisPage);
    ui->stackedWidget->setCurrentWidget(m_analysisPage);

    connect(ui->analysisBtn, &QPushButton::clicked, this, [=]() {
        ui->stackedWidget->setCurrentWidget(m_analysisPage);
        });

	//1、数据采集界面
    m_acquisitionPage = new AcquisitionPage(this);
    ui->stackedWidget->addWidget(m_acquisitionPage);
    ui->stackedWidget->setCurrentWidget(m_acquisitionPage);

    connect(ui->acquisitionBtn, &QPushButton::clicked, this, [=](){
        ui->stackedWidget->setCurrentWidget(m_acquisitionPage);
    });
}

MainWindow::~MainWindow()
{
    delete ui;
}

