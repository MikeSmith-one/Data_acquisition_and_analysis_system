#include "MainWindow.h"
#include <QFile>
#include "./pages/ConfigManager.h"
#include <QDebug>

MainWindow::MainWindow(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::MainWindowClass())
{
    //系统初始化阶段：优先加载本地 INI 配置文件
    //确保后端与页面（如 AcquisitionPage）在初始状态下能拿到最新的工位配置
    ConfigManager::instance()->loadConfigFromIni();

    ui->setupUi(this);

    initUI();

    resize(1360, 740);

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

	//ConfigManager 测试函数：读取配置文件并打印到控制台
//#ifdef QT_DEBUG
//    testConfigManager();
//#endif
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::initUI()
{
    QString qss = loadQss();
    this->setStyleSheet(qss);
}

QString MainWindow::loadQss()
{
    QFile file(":/Resource/qss/MainWindow.css");
 
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "打开文件失败!";
        return "";
    }

    return file.readAll();
}

//ConfigManager 测试函数：读取配置文件并打印到控制台
/*
void MainWindow::testConfigManager() {
    qDebug() << "========== 开始测试 ConfigManager 读取 ==========";

    ConfigManager* manager = ConfigManager::instance();
    // 确保此时已经执行过 loadConfigFromIni()

    int count = manager->getStationCount();
    qDebug() << "[全局配置] 工位总数:" << count;

    QVector<StationConfig> stations = manager->getStationsConfig();
    if (stations.isEmpty()) {
        qDebug() << "警告: 未读取到任何工位配置，请检查 INI 路径或数据！";
        return;
    }

    for (const StationConfig& station : stations) {
        qDebug() << "----------------------------------------";
        qDebug() << "工位 ID:" << station.stationId;
        qDebug() << "[通信设置] IP:" << station.ip
            << " | 端口:" << station.port
            << " | 从站ID:" << station.slaveId
            << " | 轮询周期:" << station.pollPeriod << "ms";

        qDebug() << "[数据点映射] 共" << station.mappings.size() << "个点位:";
        for (const PointMapping& mapping : station.mappings) {
            qDebug() << "  ->" << mapping.paramName.leftJustified(5, ' ')
                << "| 地址:" << QString::number(mapping.regAddr).leftJustified(4, ' ')
                << "| 类型:" << mapping.regType.leftJustified(8, ' ')
                << "| 格式:" << mapping.dataFormat.leftJustified(6, ' ')
                << "| 缩放:" << mapping.scaleFactor;
        }

        qDebug() << "[报警阈值] 共" << station.alarmLimits.size() << "个参数:";
        QMapIterator<QString, AlarmLimit> i(station.alarmLimits);
        while (i.hasNext()) {
            i.next();
            qDebug() << "  ->" << i.key().leftJustified(5, ' ')
                << "| 下限:" << QString::number(i.value().lowerLimit, 'f', 2).leftJustified(6, ' ')
                << "| 上限:" << QString::number(i.value().upperLimit, 'f', 2);
        }
    }
    qDebug() << "========== 测试 ConfigManager 读取结束 ==========";
}
*/