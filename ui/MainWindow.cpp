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
    //testConfigManager();
//#endif

    // ====================================
    // Modbus TCP通信子线程初始化
	// ====================================
    
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
    
    //接收后台报警信号 (跨线程自动排队)
    connect(m_modbusManager, &ModbusManager::alarmTriggered,
        this, &MainWindow::onAlarmTriggered);



    //测试用：打印接收到的工位数据到控制台
#ifdef QT_DEBUG
    connect(m_modbusManager, &ModbusManager::stationDataReceived,
        this, &MainWindow::debugPrintStationData);
#endif
    //启动工作线程
    m_modbusThread->start();

    //触发首次配置加载
    QMetaObject::invokeMethod(m_modbusManager, "applyConfigurations",
        Qt::QueuedConnection,
        Q_ARG(QVector<StationConfig>, ConfigManager::instance()->getStationsConfig()));

    // ====================================
    // DatabaseManager 数据库子线程初始化
    // ====================================
    m_dbThread = new QThread(this);
    m_dbManager = new DatabaseManager();
    m_dbManager->moveToThread(m_dbThread);

    //线程安全销毁连接
    connect(m_dbThread, &QThread::finished, m_dbManager, &QObject::deleteLater);

    //接收底层数据：ModbusManager -> DatabaseManager
    connect(m_modbusManager, &ModbusManager::stationDataReceived,
        m_dbManager, &DatabaseManager::receiveData, Qt::QueuedConnection);

    //启停控制：AcquisitionPage -> DatabaseManager
    //直接在 MainWindow 里拦截 startAcquisitionRequested 信号，获取当前时间发给 DB
    connect(m_acquisitionPage, &AcquisitionPage::startAcquisitionRequested, this, [=]() {
        qint64 startTime = QDateTime::currentMSecsSinceEpoch();
        QMetaObject::invokeMethod(m_dbManager, "startRecording", Qt::QueuedConnection, Q_ARG(qint64, startTime));
        });

    connect(m_acquisitionPage, &AcquisitionPage::stopAcquisitionRequested,
        m_dbManager, &DatabaseManager::stopRecording, Qt::QueuedConnection);

    // ============================================================================
    // 日志系统
    // ============================================================================
    
    //路由 AcquisitionPage 的日志
    connect(m_acquisitionPage, &AcquisitionPage::logMessage, this, [=](const QString& msg) {
        if (m_logPage) {
            //智能推断级别：包含"失败","异常","超时","报警"等字眼定为 ERROR/WARN
            QString level = "INFO";
            if (msg.contains("失败") || msg.contains("异常") || msg.contains("超时") || msg.contains("错误")) level = "ERROR";
            else if (msg.contains("报警") || msg.contains("重试")) level = "WARN";

            m_logPage->receiveLog(level, "数据采集", msg);
        }
        });

    //路由 AnalysisPage 的日志
    connect(m_analysisPage, &AnalysisPage::logMessage, this, [=](const QString& msg) {
        if (m_logPage) {
            //智能推断级别：包含"失败","异常","超时","报警"等字眼定为 ERROR/WARN
            QString level = "INFO";
            if (msg.contains("失败") || msg.contains("异常") || msg.contains("超时") || msg.contains("错误")) level = "ERROR";
            else if (msg.contains("报警") || msg.contains("重试")) level = "WARN";

            m_logPage->receiveLog(level, "数据分析", msg);
        }
        });

    //路由 ConfigPage 的日志
    connect(m_configPage, &ConfigPage::logMessage, this, [=](const QString& msg) {
        if (m_logPage) {
            //智能推断级别：包含"失败","异常","超时","报警"等字眼定为 ERROR/WARN
            QString level = "INFO";
            if (msg.contains("失败") || msg.contains("异常") || msg.contains("超时") || msg.contains("错误")) level = "ERROR";
            else if (msg.contains("报警") || msg.contains("重试")) level = "WARN";

            m_logPage->receiveLog(level, "参数配置", msg);
        }
        });

    //路由 LogPage 的日志
    connect(m_logPage, &LogPage::logMessage, this, [=](const QString& msg) {
        if (m_logPage) {
            //智能推断级别：包含"失败","异常","超时","报警"等字眼定为 ERROR/WARN
            QString level = "INFO";
            if (msg.contains("失败") || msg.contains("异常") || msg.contains("超时") || msg.contains("错误")) level = "ERROR";
            else if (msg.contains("报警") || msg.contains("重试")) level = "WARN";

            m_logPage->receiveLog(level, "系统日志", msg);
        }
        });

    //路由 ModbusManager 的日志
    connect(m_modbusManager, &ModbusManager::logMessage, this, [=](const QString& msg) {
        if (m_logPage) {
            //智能推断级别：包含"失败","异常","超时","报警"等字眼定为 ERROR/WARN
            QString level = "INFO";
            if (msg.contains("失败") || msg.contains("异常") || msg.contains("超时")) level = "ERROR";
            else if (msg.contains("报警") || msg.contains("重试")) level = "WARN";

            m_logPage->receiveLog(level, "Modbus通信", msg);
        }
        });

    //路由 DatabaseManager 的日志
    connect(m_dbManager, &DatabaseManager::logMessage, this, [=](const QString& msg) {
        if (m_logPage) {
            QString level = "INFO";
            if (msg.contains("失败") || msg.contains("错误")) level = "ERROR";

            m_logPage->receiveLog(level, "数据库存储", msg);
        }
        });

    //启动数据库线程
    m_dbThread->start();
}

MainWindow::~MainWindow()
{
    delete ui;
}

//槽函数
void MainWindow::onStationDataReceived(const StationData& data)
{
    //将接收到的工位实时数据分发给 AcquisitionPage 或其他 UI 界面
    if (m_acquisitionPage) {

        //调用 AcquisitionPage 的接收函数
        m_acquisitionPage->updateStationData(data);
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
    //使用 noquote() 去除 QString 打印时自带的双引号
    qDebug().noquote() << QString("========== [工位 %1 实时数据] ==========").arg(data.stationId + 1);

    if (!data.isOnline) {
        qDebug().noquote() << "  状态: [离线 OFFLINE]";
        qDebug().noquote() << "========================================";
        return;
    }

    qDebug().noquote() << "  状态: [在线 ONLINE]";

    //C++17 结构化绑定 + Qt6 asKeyValueRange() 遍历 QMap
    for (auto [paramName, value] : data.values.asKeyValueRange()) {
        //%1对应参数名，-15 表示左对齐占 15 个字符
        //%2对应数值，保留 2 位小数
        QString line = QString("  %1 %2")
            .arg(paramName + " :", -15)
            .arg(value, 0, 'f', 2);

        qDebug().noquote() << line;
    }

    qDebug().noquote() << "========================================";
}

// =====================================================================
// 全局报警响应逻辑 (非模态合并弹窗)
// =====================================================================
void MainWindow::onAlarmTriggered(const AlarmInfo& info)
{
    //组装单条报警信息文本
    QString alarmType = info.isUpperLimit ? "超上限" : "超下限";
    QString newMsg = QString("🔴 工位 %1 | 参数: %2 | 当前值: %3 | %4 (阈值: %5)")
        .arg(info.stationId + 1)
        .arg(info.paramName)
        .arg(QString::number(info.currentValue, 'f', 2))
        .arg(alarmType)
        .arg(QString::number(info.limitValue, 'f', 2));

    //如果弹窗不存在，或者已经被用户关闭了，就重新创建一个
    if (!m_alarmBox) {
        m_alarmBox = new QMessageBox(this);
        m_alarmBox->setIcon(QMessageBox::Critical);
        m_alarmBox->setWindowTitle("系统全局报警");
        m_alarmBox->setStandardButtons(QMessageBox::Ok);

        //当用户点击确定关闭弹窗时，销毁对象并置空指针
        connect(m_alarmBox, &QMessageBox::finished, this, [=]() {
            m_alarmBox->deleteLater();
            m_alarmBox = nullptr;
            });

        m_alarmBox->setText("检测到以下数据异常，请立即检查：\n\n" + newMsg);

        //使用 show() 而不是 exec()，保证是非模态，绝对不阻塞后台采集和图表刷新
        m_alarmBox->show();
    }
    else {
        //如果弹窗已经存在 (说明之前有别的报警还没关)，就把新报警追加到文本后面
        QString currentText = m_alarmBox->text();
        currentText += "\n" + newMsg;
        m_alarmBox->setText(currentText);

        //确保证弹窗在最上层闪烁一下，提醒用户有新报警加入
        m_alarmBox->activateWindow();
    }
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