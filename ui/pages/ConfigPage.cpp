#include "ConfigPage.h"
#include "ConfigManager.h"

ConfigPage::ConfigPage(QWidget* parent)
    : QWidget(parent)
    , ui(new Ui::ConfigPageClass())
{
    ui->setupUi(this);

    initUI();
}

ConfigPage::~ConfigPage()
{
    delete ui;
}

void ConfigPage::initUI()
{
    QString qss = loadQss();
    this->setStyleSheet(qss);

    //设置默认路径并显示在顶部 LineEdit
    QString defaultPath = QCoreApplication::applicationDirPath() + "/config/dev_config.ini";
    ConfigManager::instance()->setIniPath(defaultPath);
    ui->iniPath->setText(defaultPath);

    //通过 JSON 绘制三个表格的 UI 骨架
    initTableWidget(ui->tableWidget, ":/Resource/uiData/configTableWidget1.json", m_colsDataMapping);
    initTableWidget(ui->tableWidget_2, ":/Resource/uiData/configTableWidget2.json", m_colsCommConfig);
    initTableWidget(ui->tableWidget_3, ":/Resource/uiData/configTableWidget3.json", m_colsAlarmLimit);

    //从 INI 读取全局工位数量并同步 UI
    int count = ConfigManager::instance()->getStationCount();
    ui->stationNum->setValue(count);
    updateStationCount(count); // 刷新 presentStation 下拉框

    //默认加载工位 1 的数据
    loadStationData(0);

    //初始化完成后，将配置文件载入 ConfigManager 内存
    ConfigManager::instance()->loadConfigFromIni();

    //初始化最后，加载右下角的全局配置到控件中
    loadGlobalConfig();
}

QString ConfigPage::loadQss()
{
    QFile file(":/Resource/qss/ConfigPage.css");

    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "打开文件失败!";
        return "";
    }

    return file.readAll();
}

//槽函数：更新工位数量下拉框
void ConfigPage::updateStationCount(int count)
{
    if (count <= 0) return;

    //刷新当前配置下拉框
    ui->presentStation->blockSignals(true); //隔离信号，防止清理重建时越界
    ui->presentStation->clear();
    for (int i = 1; i <= count; ++i) {
        ui->presentStation->addItem(QString("工位 %1").arg(i), i - 1);
    }
    ui->presentStation->blockSignals(false);
}

//槽函数：切换工位 (下拉框切换)
void ConfigPage::on_presentStation_currentIndexChanged(int index)
{
    if (index < 0) return;

    //更新 GroupBox 标题
    QString titleSuffix = QString("(工位%1)").arg(index + 1);
    ui->telemessageBox->setTitle("通信设置 " + titleSuffix);
    ui->dataMappingBox->setTitle("数据点表映射 " + titleSuffix);
    ui->warningBox->setTitle("报警阈值 " + titleSuffix);

    //从 INI 中加载新工位的数据并覆写到表格中
    loadStationData(index);
}

//槽函数：保存当前工位配置 (局部保存)
void ConfigPage::on_savePresentConfigBtn_clicked()
{
	//防止在采集中修改当前工位配置
    if (m_isAcquisitionRunning) {
        QMessageBox::warning(this, "警告", "正在采集中，无法保存配置！");
        return;
    }

    int index = ui->presentStation->currentIndex();
    QString stationPrefix = QString("Station_%1").arg(index + 1);

    //调用之前写的通用保存函数
    saveTableToIni(ui->tableWidget, m_colsDataMapping, stationPrefix + "_DataMapping");
    saveTableToIni(ui->tableWidget_2, m_colsCommConfig, stationPrefix + "_Comm");
    saveTableToIni(ui->tableWidget_3, m_colsAlarmLimit, stationPrefix + "_Alarm");

    //保存 INI 后，让 ConfigManager 同步加载最新的 INI 数据到内存
    ConfigManager::instance()->loadConfigFromIni();

    //弹窗提示
    QMessageBox::information(this, "提示", QString("工位 %1 配置已保存！").arg(index + 1));

    //通知后台，当前工位配置已变
    emit ConfigManager::instance()->configUpdated(ui->stationNum->value());
}

//槽函数：保存全局配置 (全局保存)
void ConfigPage::on_globalSaveBtn_clicked()
{
	//防止在采集中修改全局配置
    if (m_isAcquisitionRunning) {
        QMessageBox::warning(this, "警告", "正在采集中，无法保存配置！");
        return;
    }

    int newCount = ui->stationNum->value();
    QString path = ConfigManager::instance()->getIniPath();
    QSettings settings(path, QSettings::IniFormat);

    //写入全局变量至 INI 文件
    settings.setValue("Global/StationCount", newCount);
    settings.setValue("Global/SavePeriod", ui->dataSavePeriod->currentText());
    settings.setValue("Global/LogSaveLocation", ui->saveLocation->text());
    settings.setValue("Global/DatabaseLocation", ui->DatabaseLocation->text());
    settings.setValue("Global/AutoCleanLog", ui->switchAutoCleanLog->isChecked());
    settings.setValue("Global/LogRetainDays", ui->logSaveDates->value());
    settings.setValue("Global/AutoCleanDB", ui->switchAutoCleanDB->isChecked());
    settings.setValue("Global/dbRetainDays", ui->dbSaveDates->value());

    settings.sync();

    //全局保存后，同步刷新内存数据
    ConfigManager::instance()->loadConfigFromIni();

    //刷新下拉框
    updateStationCount(newCount);

    QMessageBox::information(this, "提示", "全局配置已保存！");

    //广播全局信号，采集页面收到后重构界面列数
    emit ConfigManager::instance()->configUpdated(newCount);
}

//槽函数：恢复当前工位默认配置
void ConfigPage::on_resumeConfigBtn_clicked()
{
    int currentIndex = ui->presentStation->currentIndex();
    if (currentIndex < 0) return;

    //防误触弹窗提示
    QMessageBox::StandardButton reply = QMessageBox::question(
        this, "确认重置",
        QString("是否确定将【工位 %1】的配置恢复为系统默认配置？\n\n（注意：此操作仅重置当前界面显示，需要点击「保存当前工作配置」才会覆写到配置文件）")
        .arg(currentIndex + 1),
        QMessageBox::Yes | QMessageBox::No
    );

    if (reply != QMessageBox::Yes) return;

    //重新调用 initTableWidget 从 JSON 文件读取默认模版并填充表格 UI
    initTableWidget(ui->tableWidget, ":/Resource/uiData/configTableWidget1.json", m_colsDataMapping);
    initTableWidget(ui->tableWidget_2, ":/Resource/uiData/configTableWidget2.json", m_colsCommConfig);
    initTableWidget(ui->tableWidget_3, ":/Resource/uiData/configTableWidget3.json", m_colsAlarmLimit);

    //完成提示
    QMessageBox::information(this, "提示", QString("工位 %1 已成功恢复为默认配置！").arg(currentIndex + 1));
}

//槽函数：选择新的配置文件 (顶栏 ToolButton)
void ConfigPage::on_toolButton_2_clicked()
{
    QString fileName = QFileDialog::getOpenFileName(this, "选择配置文件", "", "Config Files (*.ini)");
    if (!fileName.isEmpty()) {
        ui->iniPath->setText(fileName);
        ConfigManager::instance()->setIniPath(fileName);

        //重新读取并刷新整个界面
        int count = ConfigManager::instance()->getStationCount();
        ui->stationNum->setValue(count);
        updateStationCount(count);
        loadStationData(0);
    }
}

//选择数据库存储位置
void ConfigPage::on_toolButton_3_clicked()
{
    // 防呆：采集中不允许修改配置
    if (m_isAcquisitionRunning) {
        QMessageBox::warning(this, "警告", "正在采集中，无法更改存储位置！");
        return;
    }

    QString currentPath = ui->DatabaseLocation->text();
    QString dir = QFileDialog::getExistingDirectory(this, "选择数据库存储目录", currentPath,
        QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);

    if (!dir.isEmpty()) {
        //仅更新 UI 显示，点击“全局保存”按钮写入 INI
        ui->DatabaseLocation->setText(dir);
    }
}

//选择数据库存储位置
void ConfigPage::on_toolButton_clicked()
{
    // 防呆：采集中不允许修改配置
    if (m_isAcquisitionRunning) {
        QMessageBox::warning(this, "警告", "正在采集中，无法更改存储位置！");
        return;
    }

    QString currentPath = ui->saveLocation->text();
    QString dir = QFileDialog::getExistingDirectory(this, "选择日志存储目录", currentPath,
        QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);

    if (!dir.isEmpty()) {
        //仅更新 UI 显示，点击“全局保存”按钮写入 INI
        ui->saveLocation->setText(dir);
    }
}

//从json文件中加载数据初始化表格
void ConfigPage::initTableWidget(QTableWidget* table, const QString& jsonPath, QJsonArray& colCache)
{
    QFile file(jsonPath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) { 
        qWarning() << "打开文件失败!";
        return; }

    QJsonObject root = QJsonDocument::fromJson(file.readAll()).object();
    file.close();

    colCache = root["columns"].toArray();
    QJsonArray rowDatas = root["default_data"].toArray();

	//清空表格并设置行列数
    table->clear();
    table->setColumnCount(colCache.size());
    table->setRowCount(rowDatas.size());

	//设置表格自适应列宽
    table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);

	//设置表头文字
    QStringList headers;
    for (const auto& colVal : colCache) headers.append(colVal.toObject()["title"].toString());
    table->setHorizontalHeaderLabels(headers);

    for (int row = 0; row < rowDatas.size(); ++row) {
        QJsonObject rowObj = rowDatas[row].toObject();

        for (int col = 0; col < colCache.size(); ++col) {
            QJsonObject config = colCache[col].toObject();
            QString type = config["type"].toString();
			QString key = config["key"].toString();//用于从 rowObj 中获取对应的值，加载默认数据的关键

            //若类型为行相关(如通信设置)，则从该行的 widget_def 提取具体配置
            if (type == "row_dependent") {
                config = rowObj["widget_def"].toObject();
                type = config["type"].toString();
            }

            if (type == "label") {
                QTableWidgetItem* item = new QTableWidgetItem(rowObj[key].toString());
                item->setFlags(item->flags() & ~Qt::ItemIsEditable);
                table->setItem(row, col, item);
            }
            else if (type == "lineedit") {
                QLineEdit* le = new QLineEdit(rowObj[key].toString());
                table->setCellWidget(row, col, le);
            }
            else if (type == "spinbox") {
                QSpinBox* sb = new QSpinBox();
                sb->setRange(config["min"].toInt(), config["max"].toInt());
                sb->setValue(rowObj[key].toInt());
                table->setCellWidget(row, col, sb);
            }
            else if (type == "double_spinbox") {
                QDoubleSpinBox* dsb = new QDoubleSpinBox();
                dsb->setRange(config["min"].toDouble(), config["max"].toDouble());
                dsb->setDecimals(config["decimals"].toInt());
                dsb->setValue(rowObj[key].toDouble());
                table->setCellWidget(row, col, dsb);
            }
            else if (type == "combobox") {
                QComboBox* cb = new QComboBox();
                for (const auto& opt : config["options"].toArray()) {
                    cb->addItem(opt.toString());
                }
                cb->setCurrentText(rowObj[key].toString());
                table->setCellWidget(row, col, cb);
            }
        }
    }
}

//从 INI 文件中加载对应工位的数据
void ConfigPage::loadStationData(int index)
{
    QString stationPrefix = QString("Station_%1").arg(index + 1);

    //调用底层的通用读取函数
    loadTableFromIni(ui->tableWidget, m_colsDataMapping, stationPrefix + "_DataMapping");
    loadTableFromIni(ui->tableWidget_2, m_colsCommConfig, stationPrefix + "_Comm");
    loadTableFromIni(ui->tableWidget_3, m_colsAlarmLimit, stationPrefix + "_Alarm");
}

//从 INI 文件中读取数据覆写表格
void ConfigPage::loadTableFromIni(QTableWidget* table, const QJsonArray& colCache, const QString& section)
{
    QSettings settings(ConfigManager::instance()->getIniPath(), QSettings::IniFormat);

    for (int row = 0; row < table->rowCount(); ++row) {
        QString paramName = table->item(row, 0)->text();

        for (int col = 1; col < colCache.size(); ++col) {
            QJsonObject colDef = colCache[col].toObject();
            QString keyBase = colDef["key"].toString().remove("param_");
            QString iniKey = QString("%1/%2_%3").arg(section, paramName, keyBase);

            if (!settings.contains(iniKey)) continue;

            QWidget* widget = table->cellWidget(row, col);
            if (auto* le = qobject_cast<QLineEdit*>(widget)) {
                le->setText(settings.value(iniKey).toString());
            }
            else if (auto* sb = qobject_cast<QSpinBox*>(widget)) {
                sb->setValue(settings.value(iniKey).toInt());
            }
            else if (auto* dsb = qobject_cast<QDoubleSpinBox*>(widget)) {
                dsb->setValue(settings.value(iniKey).toDouble());
            }
            else if (auto* cb = qobject_cast<QComboBox*>(widget)) {
                cb->setCurrentText(settings.value(iniKey).toString());
            }
        }
    }
}

//从表格读取数据覆写到 INI 文件
void ConfigPage::saveTableToIni(QTableWidget* table, const QJsonArray& colCache, const QString& section)
{
    QDir().mkpath(QCoreApplication::applicationDirPath() + "/config");
    QSettings settings(ConfigManager::instance()->getIniPath(), QSettings::IniFormat);

    for (int row = 0; row < table->rowCount(); ++row) {
        QString paramName = table->item(row, 0)->text();

        for (int col = 1; col < colCache.size(); ++col) {
            QJsonObject colDef = colCache[col].toObject();
            QString keyBase = colDef["key"].toString().remove("param_");
            QString iniKey = QString("%1/%2_%3").arg(section, paramName, keyBase);

            QWidget* widget = table->cellWidget(row, col);
            if (auto* le = qobject_cast<QLineEdit*>(widget)) {
                settings.setValue(iniKey, le->text());
            }
            else if (auto* sb = qobject_cast<QSpinBox*>(widget)) {
               settings.setValue(iniKey, sb->value());
            }
            else if (auto* dsb = qobject_cast<QDoubleSpinBox*>(widget)) {
                settings.setValue(iniKey, dsb->value());
            }
            else if (auto* cb = qobject_cast<QComboBox*>(widget)) {
                settings.setValue(iniKey, cb->currentText());
            }
        }
    }
    settings.sync();
}

//加载全局静态配置 (数据存储与日志面板)
void ConfigPage::loadGlobalConfig()
{
    //获取INI文件路径并打开 Settings
    QString iniPath = ConfigManager::instance()->getIniPath();
    QSettings settings(iniPath, QSettings::IniFormat);

    //读取并设置：数据保存周期 (带默认值 "1s")
    QString savePeriod = settings.value("Global/SavePeriod", "1s").toString();
    ui->dataSavePeriod->setCurrentText(savePeriod);

    //读取并设置：数据库存储位置
    QString dbLoc = settings.value("Global/DatabaseLocation", "").toString();
    if (dbLoc.isEmpty()) {
        dbLoc = QCoreApplication::applicationDirPath() + "/database";
    }
    ui->DatabaseLocation->setText(dbLoc);

    //读取并设置：日志存储位置
    QString logLoc = settings.value("Global/LogSaveLocation", "").toString();
    if (logLoc.isEmpty()) {
        logLoc = QCoreApplication::applicationDirPath() + "/Logs";
    }
    ui->saveLocation->setText(logLoc);


    //读取并设置：数据库清理配置 (带默认值 false 和 180天)
    bool autoCleanDB = settings.value("Global/AutoCleanDB", false).toBool();
    int dbRetainDays = settings.value("Global/dbRetainDays", 180).toInt();
    ui->switchAutoCleanDB->setChecked(autoCleanDB);
    ui->dbSaveDates->setValue(dbRetainDays);

    //读取并设置：日志清理配置 (带默认值 false 和 30天)
    bool autoCleanLog = settings.value("Global/AutoCleanLog", false).toBool();
    int logRetainDays = settings.value("Global/LogRetainDays", 30).toInt();
    ui->switchAutoCleanLog->setChecked(autoCleanLog);
    ui->logSaveDates->setValue(logRetainDays);
}

void ConfigPage::setAcquisitionRunning(bool isRunning)
{
    m_isAcquisitionRunning = isRunning;

    // 采集中禁用保存和重置按钮，防止误操作
    ui->savePresentConfigBtn->setEnabled(!isRunning);
    ui->globalSaveBtn->setEnabled(!isRunning);
    ui->resumeConfigBtn->setEnabled(!isRunning);
}