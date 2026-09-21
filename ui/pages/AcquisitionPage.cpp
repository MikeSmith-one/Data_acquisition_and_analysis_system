#include "AcquisitionPage.h"

AcquisitionPage::AcquisitionPage(QWidget* parent)
    : QWidget(parent)
    , ui(new Ui::AcquisitionPageClass())
{
    ui->setupUi(this);

    //初始化渲染定时器 (100ms 刷新一次图表，解耦通信频率，防止 UI 卡顿)
    m_renderTimer = new QTimer(this);
    connect(m_renderTimer, &QTimer::timeout, this, &AcquisitionPage::onRenderTimerTimeout);
    m_renderTimer->start(100);

    initUI();


    //初始化图表插槽池
    initChartPool();


    //默认初始化为 1 个工位，防止 UI 出现空载状态
    updateStationCount(1);

    //连接单例中的信号更新界面参数
    connect(ConfigManager::instance(), &ConfigManager::configUpdated, this, &AcquisitionPage::onConfigReloaded);

	//连接“提取模式”下拉框的信号槽实现动态更新右侧单位 Label 和输入框占位符
    connect(ui->comboBoxMode, QOverload<int>::of(&QComboBox::currentIndexChanged),
        this, &AcquisitionPage::onAcquisitionExtractionModeChanged);
    //ConfigManager 测试函数：读取配置文件并打印到控制台
	//testConfigManager();
}

AcquisitionPage::~AcquisitionPage()
{
    delete ui;
}


// =====================================================================
// 界面整体的初始化操作
// =====================================================================
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

    //停止秒表，图表将冻结在最后一刻，不会清空
    m_isAcquiring = false;

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

    //重置并启动秒表
    m_isAcquiring = true;
    m_acquisitionStartTime = QDateTime::currentMSecsSinceEpoch();
    m_currentRelativeSecs = 0.0;

    //清空上一批次的图表环形缓冲区，让图表重新从 0 开始画
    m_chartDataBuffer.clear();
    for (auto& slot : m_chartSlots) {
        for (auto series : slot.seriesMap) {
            series->clear();
        }
    }

	//通知网络层建立 TCP 连接、启动子线程并开始数据采集
    emit startAcquisitionRequested();
}

//隐藏和显示左侧面板的槽函数
void AcquisitionPage::on_leftBtn_clicked()
{
    if(leftDockisShow)
	{
		ui->leftDock->hide();
		ui->leftBtn->setText(">");
		leftDockisShow = false;
	}
    else
    {
		ui->leftDock->show();
		ui->leftBtn->setText("<");
		leftDockisShow = true;
    }
}

//隐藏和显示右侧面板的槽函数
void AcquisitionPage::on_rightBtn_clicked()
{
	if(rightDockisShow)
	{
		ui->rightDock->hide();
		ui->leftBtn->setText(">");
		rightDockisShow = false;
	}
    else
    {
		ui->rightDock->show();
		ui->leftBtn->setText("<");
		rightDockisShow = true;
    }
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
        return; //超过4个直接返回，不处理本次分配
    }

    //处理取消勾选的项 -> 释放插槽
    for (const QModelIndex& index : deselected.indexes()) {
        QString metricName = m_listViewModel->stringList().at(index.row());
        QString metricId;
        for (const auto& m : m_metricList) { if (m.name == metricName) { metricId = m.id; break; } }
        freeChartSlot(metricId);
    }

    //处理新勾选的项 -> 分配插槽
    for (const QModelIndex& index : selected.indexes()) {
        QString metricName = m_listViewModel->stringList().at(index.row());
        QString metricId;
        for (const auto& m : m_metricList) { if (m.name == metricName) { metricId = m.id; break; } }
        assignChartSlot(metricId, metricName);
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

    //记录当前系统工位总数
    m_currentStationCount = count;

    //刷新左下角电机控制下拉框
    ui->comboMotorStation->blockSignals(true); //隔离信号，防止清理重建时越界
    ui->comboMotorStation->clear();
    for (int i = 1; i <= count; ++i) {
        ui->comboMotorStation->addItem(QString("工位 %1").arg(i), i - 1);
    }
    ui->comboMotorStation->blockSignals(false);

    //刷新右侧 QTableWidget
    updateTableColumns(count);

    //动态更新已被占用的图表插槽中的曲线数量
    for (int i = 0; i < m_chartSlots.size(); ++i) {
        if (m_chartSlots[i].isOccupied) {
            updateSeriesForStationCount(m_chartSlots[i]);
        }
    }
}

//重载界面配置
void AcquisitionPage::onConfigReloaded(int stationCount)
{
    //刷新界面 UI（更新站号下拉框选项、重绘监控表格的列数）
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

    //禁止编辑
	ui->tableWidget->setEditTriggers(QAbstractItemView::NoEditTriggers);

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

// =====================================================================
// 图表插槽池 (Slot Pool) 与 环形缓冲区 (Ring Buffer) 核心逻辑
// =====================================================================

//初始化 4 个图表插槽
void AcquisitionPage::initChartPool()
{
	//将 UI 中的 4 个 QChartView 控件存入列表，便于统一管理
    QList<QChartView*> views = { ui->chart1, ui->chart2, ui->chart3, ui->chart4 };

    for (int i = 0; i < views.size(); ++i) {
        ChartSlot slot;
        slot.view = views[i];
        slot.chart = new QChart();
        slot.chart->setTheme(QChart::ChartThemeLight);
        slot.chart->setAnimationOptions(QChart::NoAnimation); //实时图表必须关闭动画

        //给图表四周留出像素的呼吸空间，确保坐标轴文字能显示
        slot.chart->setMargins(QMargins(5, 5, 5, 5));

        //初始化 X 轴 (时间轴)
        slot.axisX = new QValueAxis();
        slot.axisX->setLabelFormat("%.0f"); // 显示整数秒
        slot.axisX->setTitleText("时间（s）");
        slot.axisX->setTickCount(6);//分成5段：0，20，40，60，80，100

        //强制要求显示标签，防止 Qt 自作主张隐藏
        slot.axisX->setLabelsVisible(true);

        //初始化 Y 轴 (数值轴)
        slot.axisY = new QValueAxis();
        slot.axisY->setLabelFormat("%.2f");

        slot.chart->addAxis(slot.axisX, Qt::AlignBottom);
        slot.chart->addAxis(slot.axisY, Qt::AlignLeft);

        slot.view->setChart(slot.chart);
        slot.view->setRenderHint(QPainter::Antialiasing); //开启抗锯齿
		slot.view->show();//始终显示图表，未被分配时为白板

        m_chartSlots.append(slot);
    }
}

//分配插槽：当用户在 ListView 勾选指标时调用
void AcquisitionPage::assignChartSlot(const QString& metricId, const QString& metricName)
{
    for (int i = 0; i < m_chartSlots.size(); ++i) {
        if (!m_chartSlots[i].isOccupied) {
            m_chartSlots[i].isOccupied = true;
            m_chartSlots[i].currentMetricId = metricId;
            m_chartSlots[i].currentMetricName = metricName;
            m_chartSlots[i].chart->setTitle(metricName);

            //从 m_metricList 中查找该指标的单位，并设置为 Y 轴标题
            QString unit = "";
            for (const auto& m : m_metricList) {
                if (m.id == metricId) {
                    unit = m.unit;
                    break;
                }
            }
            m_chartSlots[i].axisY->setTitleText(unit);//显示类似 "℃" 或 "kPa"

            //根据当前工位数量动态生成曲线
            updateSeriesForStationCount(m_chartSlots[i]);
            return;
        }
    }
}

//释放插槽：当用户在 ListView 取消勾选时调用
void AcquisitionPage::freeChartSlot(const QString& metricId)
{
    for (int i = 0; i < m_chartSlots.size(); ++i) {
        if (m_chartSlots[i].isOccupied && m_chartSlots[i].currentMetricId == metricId) {
            m_chartSlots[i].isOccupied = false;
            m_chartSlots[i].currentMetricId = "";
            m_chartSlots[i].chart->setTitle("");

            //清理所有曲线
            for (auto series : m_chartSlots[i].seriesMap) {
                m_chartSlots[i].chart->removeSeries(series);
                delete series;
            }
            m_chartSlots[i].seriesMap.clear();

            //清理对应的环形缓冲区数据，释放内存
            m_chartDataBuffer.remove(metricId);
            return;
        }
    }
}

//根据工位数量动态增删曲线
void AcquisitionPage::updateSeriesForStationCount(ChartSlot& slot)
{
    //预设颜色池供不同工位使用
    QList<QColor> colors = { Qt::red, Qt::blue, Qt::darkGreen, Qt::darkMagenta, Qt::darkCyan, Qt::darkYellow };

    //移除多余的曲线 (如果工位数量减少)
    QList<int> existingIds = slot.seriesMap.keys();
    for (int id : existingIds) {
        if (id >= m_currentStationCount) {
            slot.chart->removeSeries(slot.seriesMap[id]);
            delete slot.seriesMap[id];
            slot.seriesMap.remove(id);
        }
    }

    //添加缺失的曲线 (如果工位数量增加)
    for (int i = 0; i < m_currentStationCount; ++i) {
        if (!slot.seriesMap.contains(i)) {
            QLineSeries* series = new QLineSeries();
            series->setName(QString("工位 %1").arg(i + 1));

            QPen pen;
            pen.setColor(colors[i % colors.size()]);
            pen.setWidth(2);
            pen.setStyle(Qt::SolidLine);//统一用实线
            series->setPen(pen);

            slot.chart->addSeries(series);
            series->attachAxis(slot.axisX);
            series->attachAxis(slot.axisY);

            slot.seriesMap.insert(i, series);
        }
    }
}


//接收底层数据：写入环形缓冲区(供图表)，并实时更新表格和 Label
void AcquisitionPage::updateStationData(const StationData& data)
{
    //获取当前左下角下拉框选中的工位 ID
    int currentSelectedStation = ui->comboMotorStation->currentData().toInt();

    if (!data.isOnline) {
        if (data.stationId == currentSelectedStation) ui->motorTraffic->setText("--");
        return; //离线不记录图表数据
    }

    if (!m_isAcquiring || m_acquisitionStartTime <= 0) return;  //如果没在采集，或者起点时间还未初始化，则丢弃数据

    //使用数据包自带的绝对时间戳，计算出极其精准的相对秒数
    //哪怕 UI 线程卡顿了，这个时间点依然是数据真实产生的那一刻
    double exactRelativeSecs = (data.timestamp - m_acquisitionStartTime) / 1000.0;

    //写入图表专用的环形缓冲区
    for (auto it = data.values.begin(); it != data.values.end(); ++it) {
        QString metricId = it.key();
        double value = it.value();

        QList<QPointF>& points = m_chartDataBuffer[metricId][data.stationId];

        //X 坐标使用刚才算出的 exactRelativeSecs
        points.append(QPointF(exactRelativeSecs, value));

        //维持缓冲区最大长度
        if (points.size() > MAX_BUFFER_SIZE) {
            points.removeFirst();
        }
    }

    //实时更新左下角电机流量 Label (仅当下拉框选中该工位时更新)
    if (data.stationId == currentSelectedStation && data.values.contains("flow")) {
        ui->motorTraffic->setText(QString::number(data.values.value("flow"), 'f', 2));
    }


    //写入 Session Buffer (专供右侧提取面板使用)
    m_sessionBuffer[data.stationId].append(data);

    //维持 Session Buffer 最大 24 小时 (防止长期运行内存溢出)
    qint64 cutoffTime = data.timestamp - MAX_SESSION_DURATION_MS;
    QList<StationData>& sessionList = m_sessionBuffer[data.stationId];
    while (!sessionList.isEmpty() && sessionList.first().timestamp < cutoffTime) {
        sessionList.removeFirst();
    }
}

//定时器触发：将缓冲区数据刷入图表
void AcquisitionPage::onRenderTimerTimeout()
{
    //如果正在采集，更新当前流逝的秒数
    if (m_isAcquiring && m_acquisitionStartTime > 0) {
        m_currentRelativeSecs = (QDateTime::currentMSecsSinceEpoch() - m_acquisitionStartTime) / 1000.0;
    }

    for (int i = 0; i < m_chartSlots.size(); ++i) {
        ChartSlot& slot = m_chartSlots[i];
        if (!slot.isOccupied) continue;

        QString metricId = slot.currentMetricId;

        double minY = std::numeric_limits<double>::max();
        double maxY = std::numeric_limits<double>::lowest();
        bool hasData = false;

        //遍历该插槽下的所有工位曲线
        for (auto it = slot.seriesMap.begin(); it != slot.seriesMap.end(); ++it) {
            int stationId = it.key();
            QLineSeries* series = it.value();

            //如果缓冲区有该指标该工位的数据
            if (m_chartDataBuffer.contains(metricId) && m_chartDataBuffer[metricId].contains(stationId)) {
                const QList<QPointF>& points = m_chartDataBuffer[metricId][stationId];

                //批量替换曲线数据 (比append 效率高极多，防止卡顿)
                series->replace(points);

                //寻找 Y 轴极值用于自适应缩放
                for (const QPointF& pt : points) {
                    if (pt.y() < minY) minY = pt.y();
                    if (pt.y() > maxY) maxY = pt.y();
                }
                if (!points.isEmpty()) hasData = true;
            }
        }

        //X 轴滚动逻辑：前 100 秒固定显示 0~100，超过 100 秒后整体向左滚动
        if (m_currentRelativeSecs <= 100.0) {
            slot.axisX->setRange(0, 100.0);
        }
        else {
            slot.axisX->setRange(m_currentRelativeSecs - 100.0, m_currentRelativeSecs);
        }

        //动态调整 Y 轴 (留出 10% 的上下边距)
        if (hasData) {
            double margin = (maxY - minY) * 0.1;
            if (margin == 0) margin = 1.0; //防止直线时极差为 0 导致崩溃
            slot.axisY->setRange(minY - margin, maxY + margin);
        }
    }
}

// =====================================================================
// 右侧面板：数据提取逻辑 (基于 Session Buffer)
// =====================================================================
void AcquisitionPage::on_getDataBtn_clicked()
{
    //获取当前选择的提取模式 ID (如 "time_point", "steady_state", "max_value")
    QString modeId = ui->comboBoxMode->currentData().toMap()["id"].toString();
    QString timeInput = ui->lineEditTime->text().trimmed();

    //遍历所有工位进行数据提取
    for (int statId = 0; statId < m_currentStationCount; ++statId) {
        int colIndex = statId + 1; //表格对应的列号
        const QList<StationData>& historyData = m_sessionBuffer[statId];

        //如果该工位没有任何历史数据，直接填 "--"
        if (historyData.isEmpty()) {
            for (int row = 0; row < m_tableMetricList.size(); ++row) {
                if (ui->tableWidget->item(row, colIndex)) ui->tableWidget->item(row, colIndex)->setText("--");
            }
            continue;
        }

        //存放最终计算结果的字典
        QMap<QString, double> extractedValues;

        //模式 A：时间点提取 (最近邻搜索)
        if (modeId == "time_point") {
            bool ok;
            double targetSecs = timeInput.toDouble(&ok);
            if (!ok) { QMessageBox::warning(this, "输入错误", "请输入有效的时间数值！"); return; }

            StationData bestData;
            double minDiff = std::numeric_limits<double>::max();

            //遍历寻找最接近的时间点
            for (const StationData& d : historyData) {
                double relSecs = (d.timestamp - m_acquisitionStartTime) / 1000.0;//计算相对时间，筛选数据用
                double diff = std::abs(relSecs - targetSecs);
                if (diff < minDiff) {
                    minDiff = diff;
                    bestData = d;
                }
            }

            //如果找到的数据时间差在 2 秒以内，认为是有效数据
            if (minDiff <= 2.0) {
                extractedValues = bestData.values;
            }
        }
        //模式 B：稳态平均提取 (区间求均值)
        else if (modeId == "steady_state") {
            QStringList parts = timeInput.split("-");
            if (parts.size() != 2) { QMessageBox::warning(this, "输入错误", "格式错误！请输入如: 100-150"); return; }

            double startSecs = parts[0].toDouble();
            double endSecs = parts[1].toDouble();
            if (startSecs >= endSecs) { QMessageBox::warning(this, "输入错误", "起始时间必须小于结束时间！"); return; }

            QMap<QString, double> sumMap;
            int count = 0;

            for (const StationData& d : historyData) {
                double relSecs = (d.timestamp - m_acquisitionStartTime) / 1000.0;//计算相对时间，筛选数据用
                if (relSecs >= startSecs && relSecs <= endSecs) {
                    count++;
                    for (auto it = d.values.begin(); it != d.values.end(); ++it) {
                        sumMap[it.key()] += it.value();
                    }
                }
            }

            if (count > 0) {
                for (auto it = sumMap.begin(); it != sumMap.end(); ++it) {
                    extractedValues[it.key()] = it.value() / count; //计算平均值
                }
            }
        }
        //模式 C：极值提取 (全局找最大值)
        else if (modeId == "max_value") {
            //初始化极值字典为极小值
            for (int row = 0; row < m_tableMetricList.size(); ++row) {
                extractedValues[m_tableMetricList[row].id] = std::numeric_limits<double>::lowest();
            }

            for (const StationData& d : historyData) {
                for (auto it = d.values.begin(); it != d.values.end(); ++it) {
                    if (it.value() > extractedValues[it.key()]) {
                        extractedValues[it.key()] = it.value();
                    }
                }
            }
        }

        //将计算好的结果填入 QTableWidget
        for (int row = 0; row < m_tableMetricList.size(); ++row) {
            QString metricId = m_tableMetricList[row].id;
            if (ui->tableWidget->item(row, colIndex)) {
                if (extractedValues.contains(metricId) && extractedValues[metricId] > std::numeric_limits<double>::lowest()) {
                    ui->tableWidget->item(row, colIndex)->setText(QString::number(extractedValues[metricId], 'f', 2));
                }
                else {
                    ui->tableWidget->item(row, colIndex)->setText("--"); //无数据
                }
            }
        }
    }
}

// =====================================================================
// 顶部工具栏：清理缓存逻辑
// =====================================================================
void AcquisitionPage::on_cleanBtn_clicked()
{
    //防误触确认
    if (QMessageBox::question(this, "确认清理", "确定要清空当前所有的采集缓存数据吗？\n清空后图表和提取面板将重置。") != QMessageBox::Yes) {
        return;
    }

    //清空图表环形缓冲区 & Session Buffer
    m_chartDataBuffer.clear();
    m_sessionBuffer.clear();

    //清空图表上的曲线
    for (auto& slot : m_chartSlots) {
        for (auto series : slot.seriesMap) {
            series->clear();
        }
    }

    //重置右侧表格为 "--"
    for (int col = 1; col <= m_currentStationCount; ++col) {
        for (int row = 0; row < ui->tableWidget->rowCount(); ++row) {
            if (ui->tableWidget->item(row, col)) {
                ui->tableWidget->item(row, col)->setText("--");
            }
        }
    }

    QMessageBox::information(this, "清理完成", "内存缓存已成功清理！");
}

// =====================================================================
// 右侧面板：将当前表格数据导出为 CSV 文件
// =====================================================================
void AcquisitionPage::on_outputBtn_clicked()
{
    //检查表格是否有实质性数据 (简单判断第一行第一列是否为 "--")
    if (ui->tableWidget->rowCount() == 0 ||
        (ui->tableWidget->item(0, 1) && ui->tableWidget->item(0, 1)->text() == "--")) {
        QMessageBox::warning(this, "导出失败", "当前表格中没有有效数据，请先点击【提取数据】！");
        return;
    }

    //创建写死的默认目录 (exe同级目录下的 temporary_data)
    QString dirPath = QCoreApplication::applicationDirPath() + "/temporary_data";
    QDir dir;
    if (!dir.exists(dirPath)) {
        dir.mkpath(dirPath); //如果目录不存在，自动创建
    }

    //动态生成文件名 (格式: 提取模式_年月日_时分秒.csv)
    QString modeText = ui->comboBoxMode->currentText();
    QString timestamp = QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss");
    QString fileName = QString("%1/提取快照_%2_%3.csv").arg(dirPath).arg(modeText).arg(timestamp);

    //打开文件准备写入
    QFile file(fileName);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::critical(this, "导出失败", "无法创建文件，请检查目录权限：\n" + fileName);
        return;
    }

    QTextStream out(&file);

    //设置 UTF-8 编码并写入 BOM 头
    //工业软件导出的 CSV 经常用 Excel 打开，如果不加 BOM 头，Excel 打开 UTF-8 文件时中文会乱码
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    out.setEncoding(QStringConverter::Utf8);
#else
    out.setCodec("UTF-8");
#endif
    out.setGenerateByteOrderMark(true);

    //写入表头 (如: 性能指标, 工位 1, 工位 2...)
    QStringList headers;
    for (int col = 0; col < ui->tableWidget->columnCount(); ++col) {
        if (ui->tableWidget->horizontalHeaderItem(col)) {
            headers << ui->tableWidget->horizontalHeaderItem(col)->text();
        }
        else {
            headers << "";
        }
    }
    out << headers.join(",") << "\n";

    //遍历写入数据行
    for (int row = 0; row < ui->tableWidget->rowCount(); ++row) {
        QStringList rowData;
        for (int col = 0; col < ui->tableWidget->columnCount(); ++col) {
            QTableWidgetItem* item = ui->tableWidget->item(row, col);
            QString text = item ? item->text() : "";

            //CSV 标准防呆：如果文本本身包含逗号，必须用双引号括起来
            if (text.contains(",")) {
                text = "\"" + text + "\"";
            }
            rowData << text;
        }
        out << rowData.join(",") << "\n";
    }

    file.close();

    //弹窗提示成功，并告知具体路径
    QMessageBox::information(this, "导出成功", "数据已成功导出至：\n" + fileName);
}

//测试 ConfigManager 读取配置文件的函数，打印到控制台
/*
void AcquisitionPage::testConfigManager() {
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