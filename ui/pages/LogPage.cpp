#include "LogPage.h"
#include <QHeaderView>

LogPage::LogPage(QWidget* parent)
    : QWidget(parent)
    , ui(new Ui::LogPageClass())
{
    ui->setupUi(this);

    initUI();
}

LogPage::~LogPage()
{
    delete ui;
}

void LogPage::initUI()
{
    QString qss = loadLog();
    this->setStyleSheet(qss);

    //初始化时间选择器 (默认显示今天)
    ui->startDateTimeEdit->setDateTime(QDateTime::currentDateTime().addDays(-1));
    ui->endDateTimeEdit->setDateTime(QDateTime::currentDateTime().addDays(1));

    //初始化表格列
    ui->logShowList->setColumnCount(4);
    ui->logShowList->setHorizontalHeaderLabels({ "发生时间", "日志级别", "触发模块", "日志详细描述" });
    ui->logShowList->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Stretch); // 描述列自动拉伸
    ui->logShowList->setColumnWidth(0, 180);
    ui->logShowList->setColumnWidth(1, 80);
    ui->logShowList->setColumnWidth(2, 120);
    ui->logShowList->setEditTriggers(QAbstractItemView::NoEditTriggers);
    ui->logShowList->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->logShowList->setSelectionMode(QAbstractItemView::SingleSelection);

    //初始化模块筛选列表 (默认添加几个核心模块)
    loadModuleFiltersFromJson();

    //绑定表格点击事件
    connect(ui->logShowList, &QTableWidget::itemSelectionChanged, this, &LogPage::on_logShowList_itemSelectionChanged);
}

QString LogPage::loadLog()
{
    QFile file(":/Resource/qss/LogPage.css");

    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "文件打开失败";

        emit logMessage("文件打开失败");
    }

    return file.readAll();
}

// =====================================================================
// 核心：接收实时日志 (Live 模式)
// =====================================================================
void LogPage::receiveLog(const QString& level, const QString& module, const QString& msg)
{
    LogItem item;
    item.timestamp = QDateTime::currentDateTime();
    item.level = level;
    item.module = module;
    item.message = msg;

    //永远写入当天的物理文件 (后台静默执行)
    writeLogToFile(item);

    //如果当前处于 Live 模式，则实时更新到界面
    if (m_isLiveMode) {
        m_logItems.append(item);
        appendLogToUI(item);
        updateStatistics();
    }
}

//将日志写入文件
void LogPage::writeLogToFile(const LogItem& item)
{
    //获取全局配置的日志路径
    QString iniPath = ConfigManager::instance()->getIniPath();
    QSettings settings(iniPath, QSettings::IniFormat);
    QString logDir = settings.value("Global/LogSaveLocation", QCoreApplication::applicationDirPath() + "Logs").toString();

    QDir dir;
    if (!dir.exists(logDir))
        dir.mkpath(logDir);

    //按天生成文件名：sys_20231027.log
    QString fileName = QString("%1/sys_%2.log").arg(logDir).arg(item.timestamp.toString("yyyyMMdd"));
    QFile file(fileName);

    if (file.open(QIODevice::Append | QIODevice::Text)) {
        QTextStream out(&file);
        //写入格式：[2023-10-27 15:30:00] [INFO] [Modbus TCP] 设备连接成功
        out << QString("[%1] [%2] [%3] %4\n")
            .arg(item.timestamp.toString("yyyy-MM-dd HH:mm:ss"))
            .arg(item.level)
            .arg(item.module)
            .arg(item.message);
        file.close();
    }
}

//将新生成的日志追加到UI上
void LogPage::appendLogToUI(const LogItem& item)
{
    int row = ui->logShowList->rowCount();
    ui->logShowList->insertRow(row);

    ui->logShowList->setItem(row, 0, new QTableWidgetItem(item.timestamp.toString("yyyy-MM-dd HH:mm:ss")));

    QTableWidgetItem* levelItem = new QTableWidgetItem(item.level);
    if (item.level == "ERROR")
        levelItem->setForeground(Qt::red);
    else if (item.level == "WARN")
        levelItem->setForeground(QColor(200, 150, 0));
    else
        levelItem->setForeground(Qt::blue);

    ui->logShowList->setItem(row, 1, levelItem);
    ui->logShowList->setItem(row, 2, new QTableWidgetItem(item.module));
    ui->logShowList->setItem(row, 3, new QTableWidgetItem(item.message));

    //自动滚动到最底部
    ui->logShowList->scrollToBottom();
}

// =====================================================================
// 交互：点击表格显示详情
// =====================================================================
void LogPage::on_logShowList_itemSelectionChanged()
{
    int row = ui->logShowList->currentRow();
    if (row >= 0 && row < m_logItems.size()) {
        //在下方文本框显示完整信息
        QString detail = QString("时间: %1\n级别: %2\n模块: %3\n\n描述:\n%4")
            .arg(m_logItems[row].timestamp.toString("yyyy-MM-dd HH:mm:ss"))
            .arg(m_logItems[row].level)
            .arg(m_logItems[row].module)
            .arg(m_logItems[row].message);
        ui->logInfoList->setText(detail);
    }
}

// =====================================================================
// 历史模式：导入与解析日志文件
// =====================================================================

void LogPage::on_toolButton_clicked()
{
    QString iniPath = ConfigManager::instance()->getIniPath();
    QSettings settings(iniPath, QSettings::IniFormat);
    QString defaultDir = settings.value("Global/LogSaveLocation", QCoreApplication::applicationDirPath() + "Logs").toString();

    QString fileName = QFileDialog::getOpenFileName(this, "选择历史日志文件", defaultDir, "Log Files(*.log)");
    if (!fileName.isEmpty()) {
        ui->logPathEdit->setText(fileName);
    }
}

void LogPage::on_importBtn_clicked()
{
    QString fileName = ui->logPathEdit->text();
    QFile file(fileName);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QMessageBox::warning(this, "错误", "无法打开日志文件");

        emit logMessage("错误,无法打开日志文件");
        return;
    }

    //切换为历史模式，清空当前内存和界面
    m_isLiveMode = false;
    m_logItems.clear();
    ui->logShowList->setRowCount(0);
    ui->logInfoList->clear();

    //逐行解析文件
    QTextStream in(&file);
    while (!in.atEnd()) {
        QString line = in.readLine();
        if(line.isEmpty())
            continue;

        //解析格式：[2023-10-27 15:30:00] [INFO] [Modbus TCP] Message...
        int idx1 = line.indexOf("] [");
        int idx2 = line.indexOf("] [", idx1 + 1);
        int idx3 = line.indexOf("] ", idx2 + 1);

        if (idx1 != -1 && idx2 != -1 && idx3 != -1) {
            LogItem item;
            item.timestamp = QDateTime::fromString(line.mid(1, idx1 - 1), "yyyy-MM-dd HH:mm:ss");
            item.level = line.mid(idx1 + 3, idx2 - idx1 - 3);
            item.module = line.mid(idx2 + 3, idx3 - idx2 - 3);
            item.message = line.mid(idx3 + 2);

            m_logItems.append(item);
            appendLogToUI(item);
        }
    }
    file.close();
    updateStatistics();
    QMessageBox::information(this, "导入成功", QString("成功导入 %1 条历史日志！\n(系统已暂停实时显示，点击'清空显示'恢复)").arg(m_logItems.size()));

    emit logMessage(QString("成功导入 %1 条历史日志！\n(系统已暂停实时显示，点击'清空显示'恢复)").arg(m_logItems.size()));
}

// =====================================================================
// 内存筛选与统计逻辑
// =====================================================================
void LogPage::on_queryBtn_clicked()
{
    QDateTime startTime = ui->startDateTimeEdit->dateTime();
    QDateTime endTime = ui->endDateTimeEdit->dateTime();
    QString targetLevel = ui->logLevelBox->currentText();
    QString keyword = ui->keySearchLineEdit->text().trimmed();
    QString targetModule = ui->listWidget->currentItem() ? ui->listWidget->currentItem()->text() : "全部模块";

    int visibleCount = 0;

    //遍历表格的所有行，进行显示/隐藏
    for (int row = 0; row < m_logItems.size();++row) {
        const LogItem& item = m_logItems[row];
        bool match = true;

        //时间过滤
        if (item.timestamp < startTime || item.timestamp > endTime)
            match = false;
        //级别过滤
        if (targetLevel != "全部（All）" && !targetLevel.contains(item.level)) 
            match = false;
        //模块过滤
        if (targetModule != "全部模块" && item.module != targetModule) 
            match = false;
        //关键字过滤
        if (!keyword.isEmpty() && !item.message.contains(keyword, Qt::CaseInsensitive)) 
            match = false;

        ui->logShowList->setRowHidden(row, !match);
        if (match)
            visibleCount++;
    }

    updateStatistics();
}

void LogPage::on_resetBtn_clicked()
{
    ui->logLevelBox->setCurrentIndex(0);
    ui->keySearchLineEdit->clear();
    ui->listWidget->setCurrentRow(0);

    //显示所有行
    for (int row = 0; row < ui->logShowList->rowCount(); ++row) {
        ui->logShowList->setRowHidden(row, false);
    }
    updateStatistics();
}

void LogPage::updateStatistics()
{
    int err = 0, warn = 0, info = 0;

    //统计当前【可见】的行
    for (int row = 0; row < m_logItems.size(); ++row) {
        if (!ui->logShowList->isRowHidden(row)) {
            if (m_logItems[row].level == "ERROR") err++;
            else if (m_logItems[row].level == "WARN") warn++;
            else info++;
        }
    }

    ui->error->setText(QString("🔴 ERROR: %1 次").arg(err));
    ui->warn->setText(QString("🟡 WARN: %1 次").arg(warn));
    ui->info->setText(QString("🔵 INFO: %1 次").arg(info));
}

// =====================================================================
// 清空与导出
// =====================================================================
void LogPage::on_cleanShowBtn_clicked()
{
    m_logItems.clear();
    ui->logShowList->setRowCount(0);
    ui->logInfoList->clear();
    updateStatistics();

    //恢复 Live 模式
    m_isLiveMode = true;
    ui->logPathEdit->clear();
}

void LogPage::on_exportLogBtn_clicked()
{
    if (ui->logShowList->rowCount() == 0) return;

    QString fileName = QFileDialog::getSaveFileName(this, "导出日志", QCoreApplication::applicationDirPath() + "/ExportedLogs.csv", "CSV Files (*.csv)");
    if (fileName.isEmpty()) return;

    QFile file(fileName);
    if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream out(&file);
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
        out.setEncoding(QStringConverter::Utf8);
#else
        out.setCodec("UTF-8");
#endif
        out.setGenerateByteOrderMark(true);

        out << "发生时间,日志级别,触发模块,详细描述\n";

        //遍历导出未logShowList中被隐藏的行
        for (int row = 0; row < m_logItems.size(); ++row) {
            if (!ui->logShowList->isRowHidden(row)) {
                const LogItem& item = m_logItems[row];
                QString safeMsg = item.message;
                if (safeMsg.contains(",")) safeMsg = "\"" + safeMsg + "\"";
                out << item.timestamp.toString("yyyy-MM-dd HH:mm:ss") << ","
                    << item.level << ","
                    << item.module << ","
                    << safeMsg << "\n";
            }
        }
        file.close();
        QMessageBox::information(this, "成功", "日志导出成功！");
    }
}

void LogPage::loadModuleFiltersFromJson()
{
    QFile file(":/Resource/uiData/logModuleFilters.json");
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "无法打开 logModuleFilters.json 文件！";

        emit logMessage("打开 logModuleFilters.json 文件失败！");
        // 如果加载失败，给一个保底的默认选项
        ui->listWidget->addItem("全部模块");
        ui->listWidget->setCurrentRow(0);
        return;
    }

    QByteArray fileData = file.readAll();
    file.close();

    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(fileData, &parseError);

    if (parseError.error != QJsonParseError::NoError || !doc.isArray()) {
        qWarning() << "logModuleFilters.json 格式解析错误！";

        emit logMessage("logModuleFilters.json 格式解析错误！");
        ui->listWidget->addItem("全部模块");
        ui->listWidget->setCurrentRow(0);
        return;
    }

    ui->listWidget->clear();
    QJsonArray array = doc.array();

    for (const QJsonValue& val : array) {
        QJsonObject obj = val.toObject();
        QString name = obj["name"].toString();
        QString id = obj["id"].toString();

        //创建 ListWidgetItem
        QListWidgetItem* item = new QListWidgetItem(name);

        //将 id 作为隐藏的 UserRole 数据绑定到 Item 上，方便未来做更精确的程序逻辑判断
        item->setData(Qt::UserRole, id);

        ui->listWidget->addItem(item);
    }

    //默认选中第一项（通常是"全部模块"）
    if (ui->listWidget->count() > 0) {
        ui->listWidget->setCurrentRow(0);
    }
}