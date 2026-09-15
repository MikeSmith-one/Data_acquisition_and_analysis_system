#include "AcquisitionPage.h"

AcquisitionPage::AcquisitionPage(QWidget* parent)
    : QWidget(parent)
    , ui(new Ui::AcquisitionPageClass())
{
    ui->setupUi(this);

    initUI();

    //默认初始化为 1 个工位，防止 UI 出现空载状态
    updateStationCount(1);

    //连接单例中的信号更新界面参数
    connect(ConfigManager::instance(), &ConfigManager::configUpdated, this, &AcquisitionPage::onConfigReloaded);

	//连接“提取模式”下拉框的信号槽实现动态更新右侧单位 Label 和输入框占位符
    connect(ui->comboBoxMode, QOverload<int>::of(&QComboBox::currentIndexChanged),
        this, &AcquisitionPage::onAcquisitionExtractionModeChanged);
}

AcquisitionPage::~AcquisitionPage()
{
    delete ui;
}

void AcquisitionPage::initUI()
{
    //加载数据到ListView
    m_metricList = loadMetricInfoFromJson();

	QStringList displayNames;
    for (const auto& metric : m_metricList) {
        displayNames.append(metric.name);
    }
	initListViewModel(displayNames);

	//加载数据到“提取模式”下拉框
    loadAcquisitionExtractionModesFromJson();

    //设置qss样式表
    QString qss = loadQss();
    this->setStyleSheet(qss);
}

//从json加载qss样式表
QString AcquisitionPage::loadQss()
{
    QFile file(":/Resource/qss/AcquisitionPage.css");

    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "打开文件失败!";
        return "";
    }

    return file.readAll();
}

//加载数据到表格列
//从json加载数据到ListView
QList<MetricInfo1> AcquisitionPage::loadMetricInfoFromJson()
{
    QList<MetricInfo1> metricList;

    QFile file(":/Resource/uiData/listView1Data.json");
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "无法打开 JSON 文件!";
        return metricList;
    }

    QByteArray fileData = file.readAll();
    file.close();

    QJsonParseError parseError;
    QJsonDocument jdom = QJsonDocument::fromJson(fileData, &parseError);

	//开始解析JSON数据
    if (parseError.error == QJsonParseError::NoError && jdom.isObject()) {
		//获取根对象
        QJsonObject rootObj = jdom.object();

		//检查是否包含"metrics"键，并且该键对应的值是一个数组
        if (rootObj.contains("metrics") && rootObj["metrics"].isArray()) {

			//获取"metrics"数组
            QJsonArray array = rootObj["metrics"].toArray();

			//遍历数组中的每个对象，并将其转换为MetricInfo1结构体
            for (const QJsonValue& val : array) {
                QJsonObject itemObj = val.toObject();
                MetricInfo1 info;
                info.id = itemObj["id"].toString();
                info.name = itemObj["name"].toString();
                info.unit = itemObj["unit"].toString();
                metricList.append(info);
            }
        }
    }

    return metricList;
}

//初始化ListView的模型，并加载数据
void AcquisitionPage::initListViewModel(const QStringList& data)
{
    m_listViewModel = new QStringListModel(data, this);

    m_listViewModel->setStringList(data);

    //加载数据到ListView
    ui->listView->setModel(m_listViewModel);

    //设置编辑模式为不可编辑
    ui->listView->setEditTriggers(QAbstractItemView::NoEditTriggers);

    //设置选择模式为多选
    ui->listView->setSelectionMode(QAbstractItemView::MultiSelection);

    //连接信号槽，处理选择变化事件
    connect(ui->listView->selectionModel(), &QItemSelectionModel::selectionChanged, this, &AcquisitionPage::onListViewSelectionChanged);
}

//停止采集按钮响应
void AcquisitionPage::on_stopCollectBtn_clicked()
{
    //恢复按钮状态
    ui->startCollectBtn->setEnabled(true);
    ui->stopCollectBtn->setEnabled(false);
	ui->readingIndicator->setStyleSheet("color: red; font-weight: bold;");
    ui->readingIndicator->setText("读取已停止");

    //通知网络层断开 TCP 连接、清空未处理的请求队列并安全退出子线程
    emit stopAcquisitionRequested();
}

//开始采集按钮响应
void AcquisitionPage::on_startCollectBtn_clicked()
{
    //切换按钮状态（UI 防抖与状态互斥）
    ui->startCollectBtn->setEnabled(false);
    ui->stopCollectBtn->setEnabled(true);
	ui->readingIndicator->setStyleSheet("color: green; font-weight: bold;");
    ui->readingIndicator->setText("实时读取中...");

	//通知网络层建立 TCP 连接、启动子线程并开始数据采集
    emit startAcquisitionRequested();
}

//处理ListView的选择变化事件，实现选中数量上限为4个
void AcquisitionPage::onListViewSelectionChanged(const QItemSelection& selected, const QItemSelection& deselected)
{
    QItemSelectionModel* selModel = ui->listView->selectionModel();

    //检查当前item总选中数量是否超过4个
    if (selModel->selectedIndexes().count() > 4) {
        //暂停阻塞信号
        selModel->blockSignals(true);

        //将超出限额的选中项取消选中
        selModel->select(selected, QItemSelectionModel::Deselect);

        //恢复阻塞信号
        selModel->blockSignals(false);
    }
}

//处理电机工位下拉框的索引变化事件，更新对应工位的流量数据
void AcquisitionPage::on_comboMotorStation_currentIndexChanged(int index)
{
    if (index < 0) return;

    //提取下拉框内绑定的真实工位索引
    int stationIndex = ui->comboMotorStation->itemData(index).toInt();

    //根据 stationIndex 从底层多线程 Socket 缓存池中取出对应流量数据
    double currentFlow = 0.00;

    ui->motorTraffic->setText(QString::number(currentFlow, 'f', 2));
}


//加载数据到“提取模式”下拉框，并绑定元数据
//从json加载数据到“提取模式”下拉框，并绑定元数据
void AcquisitionPage::loadAcquisitionExtractionModesFromJson()
{
    QFile file(":/Resource/uiData/acquisitionExtractionModes.json");
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "无法打开 acquisitionExtractionModes.json";
        return;
    }

    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    file.close();

    if (!doc.isObject()) {
        qWarning() << "acquisitionExtractionModes.json 格式不正确";
        return;
    }

    ui->comboBoxMode->clear();
    QJsonArray modes = doc.object()["extraction_modes"].toArray();

    for (const QJsonValue& val : modes) {
        QJsonObject obj = val.toObject();
        QString text = obj["text"].toString();

        //将整对象作为 QVariant 存入 itemData，便于后续信号槽直接读取
        QVariantMap metaData;
        metaData["id"] = obj["id"].toString();
        metaData["unit"] = obj["unit"].toString();
        metaData["placeholder"] = obj["placeholder"].toString();

        ui->comboBoxMode->addItem(text, metaData);
    }
}

//处理“提取模式”下拉框的索引变化事件，实现动态更新右侧单位 Label 和输入框占位符
void AcquisitionPage::onAcquisitionExtractionModeChanged(int index)
{
    if (index < 0) return;

    //获取当前选中项的 JSON 元数据
    QVariantMap metaData = ui->comboBoxMode->itemData(index).toMap();

    //动态更新右侧的单位 Label 和输入框占位符
    ui->labelUnit->setText(metaData["unit"].toString());
    ui->lineEditTime->setPlaceholderText(metaData["placeholder"].toString());
}


//加载TableWidget的指标数据
//从json加载数据到TableWidget
QList<MetricInfo1> AcquisitionPage::loadTableMetricInfoFromJson()
{
    QList<MetricInfo1> metricList;

    QFile file(":/Resource/uiData/tableWidget1Data.json");
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "无法打开 tableWidget1Data.json 文件!";
        return metricList;
    }

    QByteArray fileData = file.readAll();
    file.close();

    QJsonParseError parseError;
    QJsonDocument jdom = QJsonDocument::fromJson(fileData, &parseError);

    if (parseError.error == QJsonParseError::NoError && jdom.isObject()) {
        QJsonObject rootObj = jdom.object();
        if (rootObj.contains("metrics") && rootObj["metrics"].isArray()) {
            QJsonArray array = rootObj["metrics"].toArray();
            for (const QJsonValue& val : array) {
                QJsonObject itemObj = val.toObject();
                MetricInfo1 info;
                info.id = itemObj["id"].toString();
                info.name = itemObj["name"].toString();
                info.unit = itemObj["unit"].toString();
                metricList.append(info);
            }
        }
    }
    return metricList;
}

//由MainWindow或ConfigPage发出的信号，动态更新工位数量
void AcquisitionPage::updateStationCount(int count)
{
    if (count <= 0) return;

    //刷新左下角电机控制下拉框
    ui->comboMotorStation->blockSignals(true); //隔离信号，防止清理重建时越界
    ui->comboMotorStation->clear();
    for (int i = 1; i <= count; ++i) {
        ui->comboMotorStation->addItem(QString("工位 %1").arg(i), i - 1);
    }
    ui->comboMotorStation->blockSignals(false);

    //刷新右侧 QTableWidget
    updateTableColumns(count);
}

//重载界面配置
void AcquisitionPage::onConfigReloaded(int stationCount)
{
    // 刷新界面 UI（更新站号下拉框选项、重绘监控表格的列数）
    updateStationCount(stationCount);
}

//动态更新TableWidget表格列数与行数，并调用loadTableMetricInfoFromJson*()填充内容
void AcquisitionPage::updateTableColumns(int stationCount)
{
    //确保 Table 专属的 JSON 数据已被加载
    if (m_tableMetricList.isEmpty()) {
        m_tableMetricList = loadTableMetricInfoFromJson();
    }

    //动态设置表格列数与行数 (1列指标名 + N列工位)
    ui->tableWidget->setColumnCount(stationCount + 1);
    ui->tableWidget->setRowCount(m_tableMetricList.size());

    //构建表头文字
    QStringList headers;
    headers << "性能指标";
    for (int i = 1; i <= stationCount; ++i) {
        headers << QString("工位 %1").arg(i);
    }
    ui->tableWidget->setHorizontalHeaderLabels(headers);

    //填充表格内容
    for (int row = 0; row < m_tableMetricList.size(); ++row) {
        //第0列：指标名（不可编辑，居中）
        QTableWidgetItem* nameItem = new QTableWidgetItem(m_tableMetricList[row].name);
        nameItem->setFlags(nameItem->flags() & ~Qt::ItemIsEditable);
        nameItem->setTextAlignment(Qt::AlignCenter);
        ui->tableWidget->setItem(row, 0, nameItem);

        //第1列到第N列：工位数据（初始为 "--"，居中）
        for (int col = 1; col <= stationCount; ++col) {
            QTableWidgetItem* dataItem = new QTableWidgetItem("--");
            dataItem->setTextAlignment(Qt::AlignCenter);
            ui->tableWidget->setItem(row, col, dataItem);
        }
    }

    //隐藏原生最左侧的行号
    ui->tableWidget->verticalHeader()->setVisible(false);

    //允许根据需要自动显示水平滚动条
    ui->tableWidget->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);

    //行高保持垂直方向自适应填满
    ui->tableWidget->verticalHeader()->setSectionResizeMode(QHeaderView::Stretch);

    //第0列（性能指标）根据文字文本内容自动拉伸
    ui->tableWidget->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);

    //第1到第N列（工位列）：允许手动缩放，并赋予最小与默认宽度
    ui->tableWidget->horizontalHeader()->setMinimumSectionSize(75);

    for (int col = 1; col <= stationCount; ++col) {
        ui->tableWidget->horizontalHeader()->setSectionResizeMode(col, QHeaderView::Interactive);
        ui->tableWidget->setColumnWidth(col, 85); //给每个工位分配 85px 的宽度
    }
}

//测试 ConfigManager 读取配置文件的函数，打印到控制台
/*
void AcquisitionPage::testConfigManager(QVector<StationConfig>* stationConfigs) {
    qDebug() << "========== 开始测试 ConfigManager 读取 ==========";

    QVector<StationConfig> stations = *stationConfigs;
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