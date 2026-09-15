#include "MainWindow.h"
#include <QFile>
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

    //#####################################
    //Modbus TCP通信子线程初始化
    // ####################################
    
    //实例化核心组件
    m_modbusThread = new QThread(this);
    m_modbusManager = new ModbusManager(); // 注意：这里不能传 parent，否则无法 moveToThread

    //移入子线程
    m_modbusManager->moveToThread(m_modbusThread);

    // 跨线程连接：AcquisitionPage -> ModbusManager
    //
    //当 AcquisitionPage 发出信号时，Qt 会通过 QueuedConnection 自动将请求投递到子线程事件循环中执行
    connect(m_acquisitionPage, &AcquisitionPage::startAcquisitionRequested,
        m_modbusManager, &ModbusManager::startAcquisition);

    connect(m_acquisitionPage, &AcquisitionPage::stopAcquisitionRequested,
        m_modbusManager, &ModbusManager::stopAcquisition);

    //线程安全销毁连接
    connect(m_modbusThread, &QThread::finished, m_modbusManager, &QObject::deleteLater);

    //绑定 ConfigManager 的配置更新信号到 ModbusManager (跨线程自动排队)
    // 
    //假设 ConfigManager 增加了一个获取最新配置并发射的机制，或者直接在 MainWindow 桥接：
    connect(ConfigManager::instance(), &ConfigManager::configUpdated, this, [=]() {
        //当配置更新时，拉取最新配置并推给后台通信线程
        QVector<StationConfig> configs = ConfigManager::instance()->getStationsConfig();
        QMetaObject::invokeMethod(m_modbusManager, "applyConfigurations",//invokeMethod()函数跨线程调用，类似信号和槽
            Qt::QueuedConnection,
            Q_ARG(QVector<StationConfig>, configs));//传入参数(参数类型，参数)
        });

    //接收后台线程解析好的实时数据 (跨线程自动排队)
    connect(m_modbusManager, &ModbusManager::stationDataReceived,
        this, &MainWindow::onStationDataReceived);
    
    //测试用：打印接收到的工位数据到控制台
    connect(m_modbusManager, &ModbusManager::stationDataReceived,
        this, &MainWindow::debugPrintStationData);

    //启动工作线程
    m_modbusThread->start();

    //触发首次配置加载
    QMetaObject::invokeMethod(m_modbusManager, "applyConfigurations",
        Qt::QueuedConnection,
        Q_ARG(QVector<StationConfig>, ConfigManager::instance()->getStationsConfig()));
}

MainWindow::~MainWindow()
{
    delete ui;
}

//槽函数
void MainWindow::onStationDataReceived(const StationData& data)
{
    // TODO: 将接收到的工位实时数据分发给 AcquisitionPage 或其他 UI 界面
    if (m_acquisitionPage) {
        // m_acquisitionPage->updateStationData(data);
    }
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

//测试函数：打印接收到的工位数据到控制台
void MainWindow::debugPrintStationData(const StationData& data) {
    // 使用 noquote() 去除 QString 打印时自带的双引号
    qDebug().noquote() << QString("========== [工位 %1 实时数据] ==========").arg(data.stationId + 1);

    if (!data.isOnline) {
        qDebug().noquote() << "  状态: [离线 OFFLINE]";
        qDebug().noquote() << "========================================";
        return;
    }

    qDebug().noquote() << "  状态: [在线 ONLINE]";

    // C++17 结构化绑定 + Qt6 asKeyValueRange() 遍历 QMap
    for (auto [paramName, value] : data.values.asKeyValueRange()) {
        // 使用 arg() 进行格式化对齐：
        // %1, -15 表示左对齐，占 15 个字符宽度
        // %2, 0, 'f', 2 表示浮点数，保留 2 位小数
        qDebug().noquote() << QString("  %-15").arg(paramName + " :")
            << QString::number(value, 'f', 2);
    }

    qDebug().noquote() << "========================================";
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