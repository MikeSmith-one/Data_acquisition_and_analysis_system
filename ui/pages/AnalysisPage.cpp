#include "AnalysisPage.h"
#include "./pages/ConfigManager.h"

AnalysisPage::AnalysisPage(QWidget* parent)
    : QWidget(parent)
    , ui(new Ui::AnalysisPageClass())
{
    ui->setupUi(this);

    initUI();

    //初始化日期过滤器
    initDateFilters();

    //默认初始化为 1 个工位，防止 UI 出现空载状态
    updateStationCount(1);

    //连接“提取模式”下拉框的信号槽实现动态更新右侧单位 Label 和输入框占位符
    connect(ui->comboBoxMode, QOverload<int>::of(&QComboBox::currentIndexChanged),
        this, &AnalysisPage::onAnalysisExtractionModeChanged);

    //连接单例中的信号更新界面参数
    connect(ConfigManager::instance(), &ConfigManager::configUpdated, this, &AnalysisPage::onConfigReloaded);
}

AnalysisPage::~AnalysisPage()
{
    delete ui;
}

void AnalysisPage::initUI()
{
    //加载数据到“提取模式”下拉框
    loadAnalysisExtractionModesFromJson();

	//设置qss样式表
    QString qss = loadQss();
    this->setStyleSheet(qss);

    //读取数据库存储路径，作为默认的“文件源”路径
    QString iniPath = ConfigManager::instance()->getIniPath();
    QSettings settings(iniPath, QSettings::IniFormat);
    QString dbLocation = settings.value("Global/DatabaseLocation", "").toString();

    if (dbLocation.isEmpty()) {
        dbLocation = QCoreApplication::applicationDirPath() + "/database";
    }
    ui->sourcePathEdit->setText(dbLocation);

    //初始化带CheckBox的ListView模型
    m_dbListModel = new QStandardItemModel(this);
    ui->listView->setModel(m_dbListModel);
    ui->listView->setEditTriggers(QAbstractItemView::NoEditTriggers);//禁止双击编辑文字

    //绑定 ListView 的 Item 变化信号 (用于监听单选和多选)
    connect(m_dbListModel, &QStandardItemModel::itemChanged, this, &AnalysisPage::onListViewItemChanged);

    //绑定 ListView 的单击/焦点改变 (用于触发画图)
    connect(ui->listView->selectionModel(), &QItemSelectionModel::currentChanged,
        this, [=](const QModelIndex& current, const QModelIndex& previous) {
            Q_UNUSED(previous);
            if (current.isValid()) {
                updateCharts(); //焦点改变，立刻重绘图表
            }
        });

    //初始化图表池（共四种类型）
    initChartPool();

    //初始化时触发一次下拉联动，确保默认状态正确
    on_timeModelBox_currentIndexChanged(ui->timeModelBox->currentIndex());
}

void AnalysisPage::on_timeModelBox_currentIndexChanged(int index)
{
    if (index < 0) return;

    QString currentText = ui->timeModelBox->currentText();

    //只有选中"自定义区间"时，输入框才可用
    bool isCustom = (currentText == "自定义区间");
    ui->diyTimeEdit1->setEnabled(isCustom);
    ui->diyTimeEdit2->setEnabled(isCustom);

    if (ui->applyDiyTimeBtn) {
        ui->applyDiyTimeBtn->setEnabled(isCustom);
    }

    //如果不是自定义，清空输入框内容，让界面更干净
    if (!isCustom) {
        ui->diyTimeEdit1->clear();
        ui->diyTimeEdit2->clear();
    }
}


//空槽函数
void AnalysisPage::onListViewItemChanged(QStandardItem* item)
{
    Q_UNUSED(item);
    //当用户打勾/取消打勾时，可以在这里记录选中的文件列表
}

//从json加载qss样式表
QString AnalysisPage::loadQss()
{
    QFile file(":/Resource/qss/AnalysisPage.css");
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        qWarning() << "文件打开失败";

        emit logMessage("文件打开失败");
        return"";
    }

    return file.readAll();
}

//由MainWindow或ConfigPage发出的信号，动态更新工位数量
void AnalysisPage::updateStationCount(int count)
{
    if (count <= 0) return;

    //记录当前系统工位总数
    m_currentStationCount = count;

    //刷新右侧 QTableWidget
    updateTableColumns(count);
}

//重载界面配置
void AnalysisPage::onConfigReloaded(int stationCount)
{
    //刷新界面UI，重绘监控表格的列数
    updateStationCount(stationCount);
}

// ================================================
// 顶部加载数据控件初始化
// ================================================
//在文件最末尾，追加以下两个槽函数：
void AnalysisPage::on_toolBtn_clicked()
{
    QString currentPath = ui->sourcePathEdit->text();
    QString dir = QFileDialog::getExistingDirectory(this, "选择数据源目录", currentPath,
        QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);

    if (!dir.isEmpty()) {
        ui->sourcePathEdit->setText(dir);
    }
}

//打开资源文件所在位置
void AnalysisPage::on_openSourceBtn_clicked()
{
    QString path = ui->sourcePathEdit->text();
    QDir dir(path);

    if (!dir.exists()) {
        QMessageBox::warning(this, "提示", "该目录不存在！\n" + path);

        emit logMessage("该目录不存在,打开资源文件失败");
        return;
    }

    //调用系统资源管理器打开该目录
    QDesktopServices::openUrl(QUrl::fromLocalFile(path));
}

void AnalysisPage::on_cleanBtn_clicked()
{
    if (QMessageBox::question(this, "确认清理", "确定要清空当前界面上的所有分析数据和图表吗？") != QMessageBox::Yes) {
        return;
    }

    //清空左侧文件列表
    m_dbListModel->clear();

    //清空中心区域的 4 个图表曲线
    for (auto& slot : m_chartSlots) {
        for (auto series : slot.seriesMap) {
            slot.chart->removeSeries(series);
            delete series;
        }
        slot.seriesMap.clear();
        if (slot.axisXBar) slot.axisXBar->clear();
    }

    //清空右侧统计摘要表格 (重置为 "--")
    for (int col = 1; col < ui->tableWidget->columnCount(); ++col) {
        for (int row = 0; row < ui->tableWidget->rowCount(); ++row) {
            if (ui->tableWidget->item(row, col)) {
                ui->tableWidget->item(row, col)->setText("--");
            }
        }
    }

    QMessageBox::information(this, "清理完成", "界面数据已重置！");

    emit logMessage("清理完成,界面数据已重置！");
}

void AnalysisPage::on_outputReportBtn_clicked()
{
    //防呆：检查是否有数据可导
    if (ui->tableWidget->rowCount() == 0 ||
        (ui->tableWidget->item(0, 1) && ui->tableWidget->item(0, 1)->text() == "--")) {
        QMessageBox::warning(this, "导出失败", "当前没有有效数据，请先点击【生成摘要】！");

        emit logMessage("导出数据失败");
        return;
    }

    //选择保存路径
    QString defaultName = QCoreApplication::applicationDirPath() + "/分析报告_" + QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss") + ".txt";
    QString fileName = QFileDialog::getSaveFileName(this, "保存分析报告", defaultName, "Text Files (*.txt)");
    if (fileName.isEmpty()) return;

    //打开文件
    QFile file(fileName);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::critical(this, "导出失败", "无法创建文件！");

        emit logMessage("导出失败,无法创建文件！");
        return;
    }

    QTextStream out(&file);

    //强制使用 UTF-8 编码，防止 Windows 记事本打开时中文乱码
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    out.setEncoding(QStringConverter::Utf8);
#else
    out.setCodec("UTF-8");
#endif

    //写入报告头部信息 (简单的 ASCII 排版)
    out << "==================================================\n";
    out << "             液冷散热测试分析报告                 \n";
    out << "==================================================\n\n";

    out << "[基本信息]\n";
    out << "生成时间: " << QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss") << "\n";
    out << "数据源文件: " << ui->sourcePathEdit->text() << "\n";
    out << "提取模式: " << ui->comboBoxMode->currentText() << "\n";
    out << "时间参数: " << ui->lineEditTime->text() << " s\n\n";

    out << "--------------------------------------------------\n";
    out << "[核心性能指标]\n\n";

    //遍历表格，按工位分块写入数据
    int stationCount = ui->tableWidget->columnCount() - 1; //减去第0列(指标名)

    for (int col = 1; col <= stationCount; ++col) {
        QString stationName = ui->tableWidget->horizontalHeaderItem(col)->text();
        out << ">> " << stationName << " <<\n";

        for (int row = 0; row < ui->tableWidget->rowCount(); ++row) {
            QString metricName = ui->tableWidget->item(row, 0) ? ui->tableWidget->item(row, 0)->text() : "";
            QString metricValue = ui->tableWidget->item(row, col) ? ui->tableWidget->item(row, col)->text() : "";

            //简单的左对齐排版，让 txt 看起来整齐
            //arg(metricName, -20) 表示占 20 个字符宽度并左对齐
            out << QString("  %1 : %2\n").arg(metricName, -20).arg(metricValue);
        }
        out << "\n";
    }

    out << "==================================================\n";
    out << "                   报告结束                       \n";
    out << "==================================================\n";

    file.close();
    QMessageBox::information(this, "导出成功", "分析报告已成功导出为 TXT 文件！");

    emit logMessage("导出成功,分析报告已成功导出为 TXT 文件！");
}

// =========================================
// 左侧日期筛选初始化
// =========================================
void AnalysisPage::initDateFilters()
{
    //开启日历弹窗
    ui->dateEditStart->setCalendarPopup(true);
    ui->dateEditEnd->setCalendarPopup(true);

    //设置标准的日期显示格式
    ui->dateEditStart->setDisplayFormat("yyyy-MM-dd");
    ui->dateEditEnd->setDisplayFormat("yyyy-MM-dd");

    //设置默认显示范围（默认展示最近一星期的数据）
    ui->dateEditStart->setDate(QDate::currentDate().addDays(-7));
    ui->dateEditEnd->setDate(QDate::currentDate());

    //绑定信号：当日期改变时，自动重新查询数据库更新 ListView 列表
    connect(ui->dateEditStart, &QDateEdit::dateChanged, this, &AnalysisPage::onDateRangeChanged);
    connect(ui->dateEditEnd, &QDateEdit::dateChanged, this, &AnalysisPage::onDateRangeChanged);
}

//日期切换时的数据库筛选槽函数
void AnalysisPage::onDateRangeChanged()
{
    QString startDate = ui->dateEditStart->date().toString("yyyy-MM-dd");
    QString endDate = ui->dateEditEnd->date().toString("yyyy-MM-dd");

    //SQL 示例: 查询该时间段内的实验记录并刷新左侧 ListView
    //SELECT exp_name FROM experiment_records WHERE test_date BETWEEN :start AND :end
}


//隐藏和显示左侧面板的槽函数
void AnalysisPage::on_leftBtn_clicked()
{
    if (leftDockisShow)
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
void AnalysisPage::on_rightBtn_clicked()
{
    if (rightDockisShow)
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


// ================================================
// 右侧提取数据面板初始化
// ================================================
//加载数据到“提取模式”下拉框，并绑定元数据
//从json加载数据到“提取模式”下拉框，并绑定元数据
void AnalysisPage::loadAnalysisExtractionModesFromJson()
{
    QFile file(":/Resource/uiData/analysisExtractionModes.json");
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "无法打开 analysisExtractionModes.json";

        emit logMessage("打开 analysisExtractionModes.json 失败");
        return;
    }
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    file.close();

    if (!doc.isObject()) {
        qWarning() << "analysisExtractionModes.json 格式不正确";

        emit logMessage("analysisExtractionModes.json 错误");
        return;
    }

    ui->comboBoxMode->clear();
    QJsonArray modes = doc.object()["analysis_modes"].toArray();

    for (const QJsonValue& val : modes) {
        QJsonObject obj = val.toObject();

        //绑定元数据到ItemData
        QVariantMap meta;
        meta["id"] = obj["id"].toString();
        meta["unit"] = obj["unit"].toString();
        meta["default_val"] = obj["default_val"].toString();

        ui->comboBoxMode->addItem(obj["text"].toString(), meta);
    }
}

//处理“提取模式”下拉框的索引变化事件，实现动态更新右侧单位 Label 和输入框占位符
void AnalysisPage::onAnalysisExtractionModeChanged(int index)
{
    if (index < 0) return;

    //获取当前选中项的 JSON 元数据
    QVariantMap metaData = ui->comboBoxMode->itemData(index).toMap();

    //动态更新右侧的单位 Label 和输入框占位符
    ui->labelUnit->setText(metaData["unit"].toString());
    ui->lineEditTime->setPlaceholderText(metaData["placeholder"].toString());
}

//动态更新TableWidget表格列数与行数，并调用loadTableMetricInfoFromJson*()填充内容
void AnalysisPage::updateTableColumns(int stationCount)
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
    ui->tableWidget->horizontalHeader()->setMinimumSectionSize(95);

    for (int col = 1; col <= stationCount; ++col) {
        ui->tableWidget->horizontalHeader()->setSectionResizeMode(col, QHeaderView::Interactive);
        ui->tableWidget->setColumnWidth(col, 85); //给每个工位分配 85px 的宽度
    }
}

//加载TableWidget的指标数据
//从json加载数据到TableWidget
QList<MetricInfo2> AnalysisPage::loadTableMetricInfoFromJson()
{
    QList<MetricInfo2> metricList;

    QFile file(":/Resource/uiData/tableWidget2Data.json");
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "无法打开 tableWidget2Data.json 文件!";

        emit logMessage("警告，无法打开 tableWidget2Data.json 文件!");
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
                MetricInfo2 info;
                info.id = itemObj["id"].toString();
                info.name = itemObj["name"].toString();
                info.unit = itemObj["unit"].toString();
                metricList.append(info);
            }
        }
    }
    return metricList;
}

// =====================================================================
// 左侧面板：加载与筛选数据库文件
// =====================================================================

void AnalysisPage::on_loadingBtn_clicked()
{
    //不传日期参数，代表加载该目录下所有文件
    loadDatabaseFiles();
}

void AnalysisPage::on_filterData_btn_clicked()
{
    QDate startDate = ui->dateEditStart->date();
    QDate endDate = ui->dateEditEnd->date();

    if (startDate > endDate) {
        QMessageBox::warning(this, "日期错误", "开始日期不能晚于结束日期！");

        emit logMessage("日期错误,开始日期不能晚于结束日期！");
        return;
    }

    //传入日期范围进行筛选加载
    loadDatabaseFiles(startDate, endDate);
}

void AnalysisPage::loadDatabaseFiles(const QDate& startDate, const QDate& endDate)
{
    m_dbListModel->clear(); //清空旧列表

    QString dirPath = ui->sourcePathEdit->text();
    QDir dir(dirPath);

    if (!dir.exists()) {
        QMessageBox::warning(this, "错误", "文件源目录不存在！\n" + dirPath);

        emit logMessage("错误,文件源目录不存在！");
        return;
    }

    //只查找 Exp_ 开头的 .db 文件
    QStringList filters;
    filters << "Exp_*.db";
    QFileInfoList fileList = dir.entryInfoList(filters, QDir::Files, QDir::Time); //按时间排序，最新的在前面

    int loadedCount = 0;
    for (const QFileInfo& fileInfo : fileList) {
        QString fileName = fileInfo.fileName(); //例如: Exp_20231027_153000.db

        //尝试从文件名中解析日期 (第 5 到 12 位是 yyyyMMdd)
        //防呆：如果文件名格式不对，就跳过日期检查
        if (fileName.length() >= 17 && startDate.isValid() && endDate.isValid()) {
            QString dateStr = fileName.mid(4, 8);
            QDate fileDate = QDate::fromString(dateStr, "yyyyMMdd");

            if (fileDate.isValid()) {
                //如果文件日期不在筛选范围内，跳过该文件
                if (fileDate < startDate || fileDate > endDate) {
                    continue;
                }
            }
        }

        //创建一个带 CheckBox 的列表项
        QStandardItem* item = new QStandardItem(fileName);
        item->setCheckable(true);
        item->setCheckState(Qt::Unchecked);

        //把文件的绝对路径存入 UserRole，方便以后读取
        item->setData(fileInfo.absoluteFilePath(), Qt::UserRole);

        m_dbListModel->appendRow(item);
        loadedCount++;
    }

    if (loadedCount == 0) {
        QMessageBox::information(this, "提示", "未找到符合条件的数据库文件。");

        emit logMessage("提示,未找到符合条件的数据库文件。");
    }
}

// =====================================================================
// 左侧面板：批量导出选中的数据库文件 (短连接模式)
// =====================================================================

void AnalysisPage::on_outputData_btn_clicked()
{
    // 收集所有打勾的文件路径
    QStringList selectedFiles;
    for (int i = 0; i < m_dbListModel->rowCount(); ++i) {
        QStandardItem* item = m_dbListModel->item(i);
        if (item->checkState() == Qt::Checked) {
            selectedFiles.append(item->data(Qt::UserRole).toString());
        }
    }

    if (selectedFiles.isEmpty()) {
        QMessageBox::warning(this, "提示", "请先在列表中勾选需要导出的数据库文件！");

        emit logMessage("提示,请先在列表中勾选需要导出的数据库文件！");
        return;
    }

    //准备导出目录
    QString exportDir = QCoreApplication::applicationDirPath() + "/database_extractive";
    QDir dir;
    if (!dir.exists(exportDir)) dir.mkpath(exportDir);

    int successCount = 0;

    //遍历选中的文件，逐个导出
    for (const QString& dbFilePath : selectedFiles) {
        QFileInfo fi(dbFilePath);
        QString csvFileName = exportDir + "/" + fi.baseName() + "_全量导出.csv";

        //生成独一无二的临时连接名，用完即毁
        QString connName = QString("ExportConn_%1").arg(QDateTime::currentMSecsSinceEpoch());

        {
            QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", connName);
            db.setDatabaseName(dbFilePath);

            if (!db.open()) {
                qWarning() << "无法打开数据库进行导出:" << dbFilePath;

                emit logMessage("无法打开数据库进行导出:");
                continue; //跳过这个文件，继续下一个
            }

            QFile csvFile(csvFileName);
            if (csvFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
                QTextStream out(&csvFile);

                //写入 UTF-8 BOM 头，防止 Excel 打开乱码
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
                out.setEncoding(QStringConverter::Utf8);
#else
                out.setCodec("UTF-8");
#endif
                out.setGenerateByteOrderMark(true);

                //写入表头
                out << "ID,绝对时间,相对时间(s),工位号,流量(L/min),压降(kPa),进口温度(C),出口温度(C),平均温度(C),最高温度(C),温度均匀性(C),热阻(C/W)\n";

                //查询全量数据
                QSqlQuery query(db);
                query.exec("SELECT * FROM history_data ORDER BY abs_time ASC");

                while (query.next()) {
                    QStringList row;
                    row << query.value("id").toString();

                    //将底层的毫秒时间戳翻译为人类可读的时间
                    qint64 ms = query.value("abs_time").toLongLong();
                    row << QDateTime::fromMSecsSinceEpoch(ms).toString("yyyy-MM-dd HH:mm:ss.zzz") + "\t";

                    row << query.value("rel_time").toString();
                    row << query.value("station_id").toString();
                    row << query.value("flow").toString();
                    row << query.value("dp").toString();
                    row << query.value("tin").toString();
                    row << query.value("tout").toString();
                    row << query.value("tavg").toString();
                    row << query.value("tmax").toString();
                    row << query.value("dt").toString();
                    row << query.value("rth").toString();

                    out << row.join(",") << "\n";
                }
                csvFile.close();
                successCount++;
            }
            db.close();
        }
        //彻底销毁这个临时连接，释放文件句柄
        QSqlDatabase::removeDatabase(connName);
    }

    QMessageBox::information(this, "导出完成",
        QString("成功导出 %1 个文件！\n\n保存在目录：\n%2").arg(successCount).arg(exportDir));

    emit logMessage("导出数据库数据成功！");
}

// =====================================================================
// 图表池初始化
// =====================================================================
void AnalysisPage::initChartPool()
{
    QList<QChartView*> views = { ui->chart1, ui->chart2, ui->chart3, ui->chart4 };

    //定义 4 个图表的属性
    struct ChartDef { ChartType type; QString title; QString metricY; QString metricX; QString unitY; QString unitX; };
    QList<ChartDef> defs = {
        { ChartType::Line,    "关键指标对比 - Tmax",     "tmax", "",   "温度 (℃)", "时间 (s)" },
        { ChartType::Line,    "性能趋势对比 - Rth",      "rth",  "",   "热阻 (℃/W)", "时间 (s)" },
        { ChartType::Scatter, "稳态性能散点图",          "rth",  "dp", "热阻 (℃/W)", "压降 ΔP (kPa)" },
        { ChartType::BoxPlot, "温度均匀性分布统计 (ΔT)", "dt",   "",   "ΔT (℃)",   "实验工位" }
    };

    for (int i = 0; i < views.size(); ++i) {
        AnalysisChartSlot slot;
        slot.view = views[i];
        slot.type = defs[i].type;
        slot.title = defs[i].title;
        slot.metricY = defs[i].metricY;
        slot.metricX = defs[i].metricX;

        slot.chart = new QChart();
        slot.chart->setTheme(QChart::ChartThemeLight);
        slot.chart->setAnimationOptions(QChart::SeriesAnimations);
        slot.chart->setMargins(QMargins(10, 10, 10, 10));
        slot.chart->setTitle(slot.title);

        //初始化 Y 轴 (所有图表都有数值 Y 轴)
        slot.axisY = new QValueAxis();
        slot.axisY->setLabelFormat("%.2f");
        slot.axisY->setTitleText(defs[i].unitY);
        slot.chart->addAxis(slot.axisY, Qt::AlignLeft);

        //初始化 X 轴 (根据图表类型不同)
        if (slot.type == ChartType::BoxPlot) {
            slot.axisXBar = new QBarCategoryAxis();
            slot.axisXBar->setTitleText(defs[i].unitX);
            slot.chart->addAxis(slot.axisXBar, Qt::AlignBottom);
        }
        else {
            slot.axisX = new QValueAxis();
            slot.axisX->setLabelFormat("%.0f");
            slot.axisX->setTitleText(defs[i].unitX);
            slot.axisX->setTickCount(6);
            slot.chart->addAxis(slot.axisX, Qt::AlignBottom);
        }

        slot.view->setChart(slot.chart);
        slot.view->setRenderHint(QPainter::Antialiasing);
        m_chartSlots.append(slot);
    }
}

// =====================================================================
// 左侧面板：自定义时间区间应用逻辑
// =====================================================================
void AnalysisPage::on_applyDiyTimeBtn_clicked()
{
    //模式校验（双重保险）
    if (ui->timeModelBox->currentText() != "自定义区间") {
        return;
    }

    //空值与数字格式校验
    bool ok1, ok2;
    double startSecs = ui->diyTimeEdit1->text().toDouble(&ok1);
    double endSecs = ui->diyTimeEdit2->text().toDouble(&ok2);

    if (!ok1 || !ok2) {
        QMessageBox::warning(this, "输入错误", "请输入有效的数字时间！\n例如：10.5");

        emit logMessage("输入错误,请输入有效的数字时间！");
        return;
    }

    //逻辑校验：起点必须小于终点
    if (startSecs >= endSecs) {
        QMessageBox::warning(this, "逻辑错误", "起始时间必须严格小于结束时间！");

        emit logMessage("逻辑错误,起始时间必须严格小于结束时间！");
        return;
    }

    //负数校验（时间不能为负）
    if (startSecs < 0 || endSecs < 0) {
        QMessageBox::warning(this, "输入错误", "时间不能为负数！");

        emit logMessage("输入错误,时间不能为负数！");
        return;
    }

    //校验全部通过，触发图表重绘！
    updateCharts();
}

// =====================================================================
// 核心逻辑：读取数据库并渲染图表
// =====================================================================
void AnalysisPage::updateCharts()
{
    //获取当前高亮选中的数据库文件路径
    QModelIndex currentIndex = ui->listView->selectionModel()->currentIndex();
    if (!currentIndex.isValid()) return;

    QStandardItem* item = m_dbListModel->itemFromIndex(currentIndex);
    QString dbFilePath = item->data(Qt::UserRole).toString();

    //遍历 4 个图表插槽，分别执行查询和渲染
    for (int i = 0; i < m_chartSlots.size(); ++i) {
        renderChart(m_chartSlots[i], dbFilePath);
    }
}

void AnalysisPage::renderChart(AnalysisChartSlot& slot, const QString& dbFilePath)
{
    //清空旧曲线
    for (auto series : slot.seriesMap) {
        slot.chart->removeSeries(series);
        delete series;
    }
    slot.seriesMap.clear();
    if (slot.axisXBar) slot.axisXBar->clear();

    //解析分析周期条件
    QString timeMode = ui->timeModelBox->currentText();
    double startSecs = 0.0;
    double endSecs = 9999999.0;
    bool isSteadyState = (timeMode == "末端稳态");

    if (timeMode == "自定义区间") {
        startSecs = ui->diyTimeEdit1->text().toDouble();
        endSecs = ui->diyTimeEdit2->text().toDouble();
        if (startSecs >= endSecs) return;
    }

    QString connName = QString("ChartConn_%1_%2").arg(slot.metricY).arg(QDateTime::currentMSecsSinceEpoch());

    double minX = std::numeric_limits<double>::max();
    double maxX = std::numeric_limits<double>::lowest();
    double minY = std::numeric_limits<double>::max();
    double maxY = std::numeric_limits<double>::lowest();
    bool hasData = false;

    //用于箱线图的数据暂存 (Key: 工位ID, Value: 该工位的所有 ΔT 数据)
    QMap<int, QList<double>> boxPlotData;


    //局部作用域（匿名作用域）用于实现短连接，即用即毁坏
    {
        QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", connName);
        db.setDatabaseName(dbFilePath);

        if (db.open()) {
            QSqlQuery query(db);

            //先查出最大时间，再倒推 60 秒
            if (isSteadyState) {
                query.exec("SELECT MAX(rel_time) FROM history_data");
                if (query.next()) {
                    endSecs = query.value(0).toDouble();
                    startSecs = qMax(0.0, endSecs - 60.0); //末端稳态：取最后 60 秒
                }
            }

            //构建查询 SQL
            QString sql;
            if (slot.type == ChartType::Scatter) {
                //0 ?、1 station_id、 2 ？
                sql = QString("SELECT %1, station_id, %2 FROM history_data WHERE rel_time >= ? AND rel_time <= ?").arg(slot.metricX, slot.metricY);
            }
            else {
                //0 rel_time、1 station_id、2 ？
                sql = QString("SELECT rel_time, station_id, %1 FROM history_data WHERE rel_time >= ? AND rel_time <= ?").arg(slot.metricY);
            }

            query.prepare(sql);
            query.addBindValue(startSecs);
            query.addBindValue(endSecs);
            query.exec();

            QList<QColor> colors = { Qt::red, Qt::blue, Qt::darkGreen, Qt::darkMagenta, Qt::darkCyan };

            while (query.next()) {//query.next()，逐行遍历查询语句返回的子表
                //用value来取sql语句查询得到的子表的对应列的值
                double x = query.value(0).toDouble();//获取X轴坐标数据（相对时间rel_time或者自定义X轴的单位）
                int stId = query.value(1).toInt();//获取工位id
                QVariant yVar = query.value(2);//获取Y轴坐标数据
                if (yVar.isNull()) continue;//过滤空数据
                double y = yVar.toDouble();

                //根据图表类型分配数据
                if (slot.type == ChartType::Line) {
                    if (!slot.seriesMap.contains(stId)) {
                        QLineSeries* series = new QLineSeries();
                        series->setName(QString("工位 %1").arg(stId + 1));
                        QPen pen; pen.setColor(colors[stId % colors.size()]); pen.setWidth(2);
                        series->setPen(pen);
                        slot.chart->addSeries(series);
                        slot.seriesMap.insert(stId, series);
                    }
                    static_cast<QLineSeries*>(slot.seriesMap[stId])->append(x, y);
                }
                else if (slot.type == ChartType::Scatter) {
                    if (!slot.seriesMap.contains(stId)) {
                        QScatterSeries* series = new QScatterSeries();
                        series->setName(QString("工位 %1").arg(stId + 1));
                        series->setMarkerShape(QScatterSeries::MarkerShapeCircle);
                        series->setMarkerSize(10.0);
                        series->setColor(colors[stId % colors.size()]);
                        slot.chart->addSeries(series);
                        slot.seriesMap.insert(stId, series);
                    }
                    static_cast<QScatterSeries*>(slot.seriesMap[stId])->append(x, y);
                }
                else if (slot.type == ChartType::BoxPlot) {
                    boxPlotData[stId].append(y); //暂存数据，稍后计算四分位数
                }

                if (x < minX) minX = x; if (x > maxX) maxX = x;
                if (y < minY) minY = y; if (y > maxY) maxY = y;
                hasData = true;
            }

            //处理箱线图的特殊渲染逻辑
            if (slot.type == ChartType::BoxPlot && hasData) {
                QBoxPlotSeries* boxSeries = new QBoxPlotSeries();
                boxSeries->setName("温度均匀性分布");
                QStringList categories;

                for (auto it = boxPlotData.begin(); it != boxPlotData.end(); ++it) {
                    int stId = it.key();
                    QList<double>& vals = it.value();
                    if (vals.isEmpty()) continue;

                    std::sort(vals.begin(), vals.end());//排序以计算四分位数
                    double minVal = vals.first();
                    double maxVal = vals.last();
                    double median = vals[vals.size() / 2];
                    double q1 = vals[vals.size() / 4];
                    double q3 = vals[vals.size() * 3 / 4];

                    QBoxSet* box = new QBoxSet(QString("工位 %1").arg(stId + 1));
                    box->setValue(QBoxSet::LowerExtreme, minVal);
                    box->setValue(QBoxSet::LowerQuartile, q1);
                    box->setValue(QBoxSet::Median, median);
                    box->setValue(QBoxSet::UpperQuartile, q3);
                    box->setValue(QBoxSet::UpperExtreme, maxVal);
                    box->setBrush(colors[stId % colors.size()]);

                    boxSeries->append(box);
                    categories << QString("工位 %1").arg(stId + 1);
                }
                slot.chart->addSeries(boxSeries);
                slot.seriesMap.insert(0, boxSeries);//存入map方便后续清理
                slot.axisXBar->append(categories);
                boxSeries->attachAxis(slot.axisXBar);
                boxSeries->attachAxis(slot.axisY);
            }
            else {
                //折线图和散点图绑定坐标轴
                for (auto series : slot.seriesMap) {
                    series->attachAxis(slot.axisX);
                    series->attachAxis(slot.axisY);
                }
            }
        }
        db.close();
    }
    QSqlDatabase::removeDatabase(connName);

    //自适应调整坐标轴 (使用优雅刻度算法)
    if (hasData) {
        double niceMinX, niceMaxX, niceMinY, niceMaxY;

        if (slot.axisX) {
            if (maxX - minX < 1.0) { minX -= 0.5; maxX += 0.5; }
            calculateNiceAxis(minX, maxX, 6, niceMinX, niceMaxX);
            slot.axisX->setRange(niceMinX, niceMaxX);
        }

        double yMargin = (maxY - minY) * 0.1;
        if (yMargin == 0) yMargin = 1.0;
        calculateNiceAxis(minY - yMargin, maxY + yMargin, 6, niceMinY, niceMaxY);
        slot.axisY->setRange(niceMinY, niceMaxY);
    }
}

// =====================================================================
// 辅助函数：计算优雅的坐标轴刻度 (Nice Numbers Algorithm)
// =====================================================================
void AnalysisPage::calculateNiceAxis(double min, double max, int tickCount, double& outMin, double& outMax)
{
    double range = niceMax(max - min, false);
    double tickSpacing = niceMax(range / (tickCount - 1), true);
    outMin = std::floor(min / tickSpacing) * tickSpacing;
    outMax = std::ceil(max / tickSpacing) * tickSpacing;
}

double AnalysisPage::niceMax(double range, bool round)
{
    double exponent = std::floor(std::log10(range));
    double fraction = range / std::pow(10, exponent);
    double niceFraction;

    if (round) {
        if (fraction < 1.5) niceFraction = 1;
        else if (fraction < 3) niceFraction = 2;
        else if (fraction < 7) niceFraction = 5;
        else niceFraction = 10;
    }
    else {
        if (fraction <= 1) niceFraction = 1;
        else if (fraction <= 2) niceFraction = 2;
        else if (fraction <= 5) niceFraction = 5;
        else niceFraction = 10;
    }
    return niceFraction * std::pow(10, exponent);
}


// =====================================================================
// 右侧面板：生成统计摘要 (基于 SQLite 数据库短连接)
// =====================================================================
void AnalysisPage::on_getDataBtn_clicked()
{
    //获取当前高亮选中的数据库文件路径
    QModelIndex currentIndex = ui->listView->selectionModel()->currentIndex();
    if (!currentIndex.isValid()) {
        QMessageBox::warning(this, "提示", "请先在左侧列表中点击选中一个数据文件！");
        return;
    }
    QString dbFilePath = m_dbListModel->itemFromIndex(currentIndex)->data(Qt::UserRole).toString();

    //获取提取模式和输入参数
    QString modeId = ui->comboBoxMode->currentData().toMap()["id"].toString();
    QString timeInput = ui->lineEditTime->text().trimmed();

    //动态修改表头文字 (瞬时 vs 平均)
    bool isInstantMode = (modeId == "time_point" || modeId == "peak_value");
    QString prefix = isInstantMode ? "瞬时" : "平均";

    //动态修改第0列指标名称
    for (int row = 0; row < m_tableMetricList.size(); ++row) {
        QString metricName = m_tableMetricList[row].name;
        //简单替换：如果原名包含"平均"，且当前是瞬时模式，则替换为"瞬时"
        if (isInstantMode && metricName.contains("平均")) {
            metricName.replace("平均", "瞬时");
        }
        else if (!isInstantMode && metricName.contains("瞬时")) {
            metricName.replace("瞬时", "平均");
        }
        if (ui->tableWidget->item(row, 0)) {
            ui->tableWidget->item(row, 0)->setText(metricName);
        }
    }

    //遍历所有工位进行数据提取
    for (int statId = 0; statId < m_currentStationCount; ++statId) {
        int colIndex = statId + 1; //表格对应的列号
        QMap<QString, double> extractedValues;

        //建短连接查询数据库
        QString connName = QString("SummaryConn_%1_%2").arg(statId).arg(QDateTime::currentMSecsSinceEpoch());
        {
            QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", connName);
            db.setDatabaseName(dbFilePath);

            if (db.open()) {
                QSqlQuery query(db);
                bool querySuccess = false;

                // ---------------------------------------------------
                // 模式 A：时间点提取 (最近邻搜索)
                // ---------------------------------------------------
                if (modeId == "time_point") {
                    bool ok;
                    double targetSecs = timeInput.toDouble(&ok);
                    if (!ok) { QMessageBox::warning(this, "输入错误", "请输入有效的时间数值！"); return; }

                    // SQL: 按照与目标时间的绝对差值排序，取第一条 (极度高效的最近邻搜索)
                    query.prepare("SELECT flow, dp, tin, tout, tavg, tmax, dt, rth FROM history_data "
                        "WHERE station_id = ? ORDER BY ABS(rel_time - ?) ASC LIMIT 1");
                    query.addBindValue(statId);
                    query.addBindValue(targetSecs);
                    querySuccess = query.exec();
                }
                // ---------------------------------------------------
                // 模式 B & E：稳态平均 / 自定义区间 (区间求均值)
                // ---------------------------------------------------
                else if (modeId == "steady_state" || modeId == "custom_range") {
                    double startSecs = 0.0, endSecs = 0.0;

                    if (modeId == "steady_state") {
                        //先查出最大时间
                        query.exec("SELECT MAX(rel_time) FROM history_data");
                        if (query.next()) {
                            endSecs = query.value(0).toDouble();
                            startSecs = qMax(0.0, endSecs - 60.0); //默认取最后 60 秒
                        }
                    }
                    else {
                        //自定义区间：解析输入框 (格式如: 100-150)
                        QStringList parts = timeInput.split("-");
                        if (parts.size() != 2) { QMessageBox::warning(this, "输入错误", "格式错误！请输入如: 100-150"); return; }
                        startSecs = parts[0].toDouble();
                        endSecs = parts[1].toDouble();
                        if (startSecs >= endSecs) { QMessageBox::warning(this, "输入错误", "起始时间必须小于结束时间！"); return; }
                    }

                    //SQL: 使用 AVG 函数直接让数据库计算平均值
                    query.prepare("SELECT AVG(flow), AVG(dp), AVG(tin), AVG(tout), AVG(tavg), AVG(tmax), AVG(dt), AVG(rth) "
                        "FROM history_data WHERE station_id = ? AND rel_time >= ? AND rel_time <= ?");
                    query.addBindValue(statId);
                    query.addBindValue(startSecs);
                    query.addBindValue(endSecs);
                    querySuccess = query.exec();
                }
                // ---------------------------------------------------
                // 模式 C：全周期统计
                // ---------------------------------------------------
                else if (modeId == "full_cycle") {
                    // SQL: 全局求平均
                    query.prepare("SELECT AVG(flow), AVG(dp), AVG(tin), AVG(tout), AVG(tavg), AVG(tmax), AVG(dt), AVG(rth) "
                        "FROM history_data WHERE station_id = ?");
                    query.addBindValue(statId);
                    querySuccess = query.exec();
                }
                // ---------------------------------------------------
                // 模式 D：峰值工况 (最恶劣工况快照)
                // ---------------------------------------------------
                else if (modeId == "peak_value") {
                    // SQL: 找到 Tmax 最大的那一刻的所有数据
                    query.prepare("SELECT flow, dp, tin, tout, tavg, tmax, dt, rth FROM history_data "
                        "WHERE station_id = ? ORDER BY tmax DESC LIMIT 1");
                    query.addBindValue(statId);
                    querySuccess = query.exec();
                }

                // --- 统一处理查询结果 ---
                if (querySuccess && query.next()) {
                    //注意：这里的顺序必须和 SELECT 语句里的字段顺序严格一致！
                    if (!query.value(0).isNull()) extractedValues["flow"] = query.value(0).toDouble();
                    if (!query.value(1).isNull()) extractedValues["ΔP"] = query.value(1).toDouble();
                    if (!query.value(2).isNull()) extractedValues["Tin"] = query.value(2).toDouble();
                    if (!query.value(3).isNull()) extractedValues["Tout"] = query.value(3).toDouble();
                    if (!query.value(4).isNull()) extractedValues["Tavg"] = query.value(4).toDouble();
                    if (!query.value(5).isNull()) extractedValues["Tmax"] = query.value(5).toDouble();
                    if (!query.value(6).isNull()) extractedValues["ΔT"] = query.value(6).toDouble();
                    if (!query.value(7).isNull()) extractedValues["Rth"] = query.value(7).toDouble();
                }
            }
            db.close();
        }
        QSqlDatabase::removeDatabase(connName); //彻底销毁短连接

        //将计算好的结果填入 QTableWidget
        for (int row = 0; row < m_tableMetricList.size(); ++row) {
            QString metricId = m_tableMetricList[row].id;
            if (ui->tableWidget->item(row, colIndex)) {
                if (extractedValues.contains(metricId)) {
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
// 右侧面板：将当前统计摘要表格导出为 CSV 文件
// =====================================================================
void AnalysisPage::on_outputBtn_clicked()
{
    //检查表格是否有实质性数据
    if (ui->tableWidget->rowCount() == 0 ||
        (ui->tableWidget->item(0, 1) && ui->tableWidget->item(0, 1)->text() == "--")) {
        QMessageBox::warning(this, "导出失败", "当前表格中没有有效数据，请先点击【生成摘要】！");

        emit logMessage("导出失败,当前表格中没有有效数据");
        return;
    }

    //获取导出目录 (默认放在 database_extractive 下)
    QString dirPath = QCoreApplication::applicationDirPath() + "/database_extractive";
    QDir dir;
    if (!dir.exists(dirPath)) {
        dir.mkpath(dirPath);
    }

    //动态生成文件名 (格式: 统计摘要_提取模式_年月日_时分秒.csv)
    QString modeText = ui->comboBoxMode->currentText();
    QString timestamp = QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss");
    QString fileName = QString("%1/统计摘要_%2_%3.csv").arg(dirPath).arg(modeText).arg(timestamp);

    QFile file(fileName);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::critical(this, "导出失败", "无法创建文件，请检查目录权限：\n" + fileName);

        emit logMessage("导出失败,无法创建文件，请检查目录权限");
        return;
    }

    QTextStream out(&file);

    //写入 UTF-8 BOM 头，防止 Excel 打开乱码
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    out.setEncoding(QStringConverter::Utf8);
#else
    out.setCodec("UTF-8");
#endif
    out.setGenerateByteOrderMark(true);

    //写入表头
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
            if (text.contains(",")) text = "\"" + text + "\""; //防呆
            rowData << text;
        }
        out << rowData.join(",") << "\n";
    }

    file.close();
    QMessageBox::information(this, "导出成功", "统计摘要已成功导出至：\n" + fileName);

    emit logMessage(QString("导出成功,统计摘要 %1 已成功导出").arg(fileName));
}