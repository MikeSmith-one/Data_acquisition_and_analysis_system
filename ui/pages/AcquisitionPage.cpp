#include "AcquisitionPage.h"
#include "ConfigManager.h"

AcquisitionPage::AcquisitionPage(QWidget* parent)
    : QWidget(parent)
    , ui(new Ui::AcquisitionPageClass())
{
    ui->setupUi(this);

    initUI();

    //默认初始化为 1 个工位，防止 UI 出现空载状态
    updateStationCount(1);

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

    //TODO: 根据 stationIndex 从底层多线程 Socket 缓存池中取出对应流量数据
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

void AcquisitionPage::onConfigReloaded(int stationCount)
{
    // 1. 刷新界面 UI（更新站号下拉框选项、重绘监控表格的列数）
    updateStationCount(stationCount);

    // 2. TODO: 后续从 ConfigManager 获取解析好的纯 C++ 结构体参数，交给 Modbus 轮询线程
    // 示例思路：
    // GlobalConfig currentConfig = ConfigManager::instance()->getGlobalConfig();
    // emit restartModbusThreads(currentConfig);
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

