#include "DatabaseManager.h"
#include <QCoreApplication>
#include <QVariantList>

DatabaseManager::DatabaseManager(QObject* parent) : QObject(parent)
{
	//生成唯一的数据库连接名，防止多线程下连接名冲突
	m_connectionName = QString("DB_Thread_%1").arg(reinterpret_cast<quintptr>(this));

	//初始化5秒强制Flush定时器
	m_flushTimer = new QTimer(this);
	connect(m_flushTimer, &QTimer::timeout, this, &DatabaseManager::onFlushTimerTimeout);
}

DatabaseManager::~DatabaseManager()
{
	stopRecording();
}

void DatabaseManager::startRecording(qint64 startTime)
{
	if (m_isRecording) return;

	m_sessionStartTime = startTime;
	m_dataBuffer.clear();
	m_lastSaveTime.clear(); //清空上次保存时间记录

	//取 ConfigManager 中的“数据保存周期”配置
	QString iniPath = ConfigManager::instance()->getIniPath();
	QSettings settings(iniPath, QSettings::IniFormat);
	QString dbLocation = settings.value("Global/DatabaseLocation", "").toString();
	QString periodStr = settings.value("Global/SavePeriod", "1s").toString();

	//将 "1s", "5s", "60s" 转换为毫秒数值
	m_savePeriodMs = periodStr.remove("s").toInt() * 1000;
	if (m_savePeriodMs <= 0) m_savePeriodMs = 1000; //防呆默认 1 秒

	//获取数据库的路径
	QString dbDir;
	if (dbLocation.isEmpty()) {
		dbDir = QCoreApplication::applicationDirPath() + "/database"; //默认后备路径
	}
	else {
		dbDir = dbLocation;
	}

	QDir dir;
	if (!dir.exists(dbDir)) {
		dir.mkpath(dbDir);
		qDebug() << "数据库目录创建成功";
	}

	//执行自动清理逻辑（读取ConfigManager配置）
	autoCleanOldDatabases(dbDir);

	//按次建库：生成独立的文件名（例如：Exp_20231027_153000.db）
	QString timestampStr = QDateTime::fromMSecsSinceEpoch(startTime).toString("yyyyMMdd_HHmmss");
	QString dbFilePath = QString("%1/Exp_%2.db").arg(dbDir).arg(timestampStr);

	//初始化并打开数据库连接
	if (QSqlDatabase::contains(m_connectionName)) {
		QSqlDatabase::removeDatabase(m_connectionName);
	}
	m_db = QSqlDatabase::addDatabase("QSQLITE",m_connectionName);
	m_db.setDatabaseName(dbFilePath);

	if (!m_db.open()) {
		emit logMessage("数据库创建失败：" + m_db.lastError().text());
		qDebug() << "数据库创建失败：" + m_db.lastError().text();
		return;
	}

	//开启WAL模式（预写日志，读写互不阻塞，极大提升并发性能）
	m_db.exec("PRAGMA journal_mode=WAL;");
	m_db.exec("PRAGMA synchronous=NORMAL;"); //配合 WAL 模式提升写入速度

	//创建数据表
	QSqlQuery query(m_db);
	QString createTableSql = R"(
        CREATE TABLE IF NOT EXISTS history_data (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            abs_time INTEGER NOT NULL,
            rel_time REAL NOT NULL,
            station_id INTEGER NOT NULL,
            flow REAL,
            dp REAL,
            tin REAL,
            tout REAL,
            tavg REAL,
            tmax REAL,
            dt REAL,
            rth REAL
        );
    )";

	if (!query.exec(createTableSql)) {
		emit logMessage("数据表创建失败：" + query.lastError().text());
		return;
	}

	//为时间戳和工位号创建索引 (极大提升 AnalysisPage 的查询速度)
	query.exec("CREATE INDEX IF NOT EXISTS idx_time_station ON history_data (abs_time, station_id);");

	m_isRecording = true;
	m_flushTimer->start(5000); //启动5秒定时器
	emit logMessage("数据库已就绪，开始记录数据：" + dbFilePath);
}

void DatabaseManager::stopRecording()
{
	if (!m_isRecording) return;

	m_isRecording = false;
	m_flushTimer->stop();

	//强制Flush内存中遗留的数据 (停止事件触发)
	flushBufferToDb();

	//关闭数据库
	m_db.close();
	QSqlDatabase::removeDatabase(m_connectionName);
	emit logMessage("采集已停止，数据库已安全关闭。");
}

void DatabaseManager::receiveData(const StationData& data)
{
	if (!m_isRecording) return;

	//降采样过滤逻辑：判断距离上次保存是否达到了设定的周期
	qint64 lastTime = m_lastSaveTime.value(data.stationId, 0);

	//如果时间差小于设定的保存周期，直接丢弃该数据，不存入数据库
	if (data.timestamp - lastTime < m_savePeriodMs) {
		return;
	}

	//更新该工位的最后保存时间
	m_lastSaveTime[data.stationId] = data.timestamp;

	//放行数据，存入写入缓存
	m_dataBuffer.append(data);

	//容量触发：满100条立即写入
	if (m_dataBuffer.size() >= BATCH_SIZE_LIMIT) {
		flushBufferToDb();
	}
}

void DatabaseManager::onFlushTimerTimeout()
{
	//定时器触发：哪怕只有1条数据也强制写入，防止断电丢失
	if (m_isRecording && !m_dataBuffer.isEmpty()) {
		flushBufferToDb();
	}
}

//#############接下来这两个函数需要仔细研究#######################
void DatabaseManager::flushBufferToDb()
{
	if (m_dataBuffer.isEmpty() || !m_db.isOpen()) return;

	//开启事务 (Transaction)
	m_db.transaction();

	QSqlQuery query(m_db);
	query.prepare("INSERT INTO history_data (abs_time, rel_time, station_id, flow, dp, tin, tout, tavg, tmax, dt, rth) "
		"VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)");

	//准备批量绑定的 QVariantList（用Variant类型是因为StationData内部有int、bool、QString等多种类型的数据）
	QVariantList absTimeList, relTimeList, stIdList;
	QVariantList flowList, dpList, tinList, toutList, tavgList, tmaxList, dtList, rthList;

	for (const StationData& d : m_dataBuffer) {
		absTimeList << d.timestamp;
		relTimeList << (d.timestamp - m_sessionStartTime) / 1000.0;
		stIdList << d.stationId;

		//提取各项指标，如果没有则存入 NULL (QVariant())
		flowList << (d.values.contains("flow") ? QVariant(d.values["flow"]) : QVariant());
		dpList << (d.values.contains("ΔP") ? QVariant(d.values["ΔP"]) : QVariant());
		tinList << (d.values.contains("Tin") ? QVariant(d.values["Tin"]) : QVariant());
		toutList << (d.values.contains("Tout") ? QVariant(d.values["Tout"]) : QVariant());
		tavgList << (d.values.contains("Tavg") ? QVariant(d.values["Tavg"]) : QVariant());
		tmaxList << (d.values.contains("Tmax") ? QVariant(d.values["Tmax"]) : QVariant());
		dtList << (d.values.contains("ΔT") ? QVariant(d.values["ΔT"]) : QVariant());
		rthList << (d.values.contains("Rth") ? QVariant(d.values["Rth"]) : QVariant());
	}

	//批量绑定
	query.addBindValue(absTimeList);
	query.addBindValue(relTimeList);
	query.addBindValue(stIdList);
	query.addBindValue(flowList);
	query.addBindValue(dpList);
	query.addBindValue(tinList);
	query.addBindValue(toutList);
	query.addBindValue(tavgList);
	query.addBindValue(tmaxList);
	query.addBindValue(dtList);
	query.addBindValue(rthList);

	//执行批量插入 (execBatch 性能极高)
	if (!query.execBatch()) {
		emit logMessage("批量写入数据库失败：" + query.lastError().text());
		qDebug() << ("批量写入数据库失败：" + query.lastError().text());
		m_db.rollback();
	}
	else {
		m_db.commit(); //提交事务
	}

	//清空缓存
	m_dataBuffer.clear();
}

void DatabaseManager::autoCleanOldDatabases(const QString& dbPath)
{
	//读取 ConfigManager 中的全局配置
	QString iniPath = ConfigManager::instance()->getIniPath();
	QSettings settings(iniPath, QSettings::IniFormat);

	bool autoClean = settings.value("Global/AutoCleanDB", false).toBool();
	int retainDays = settings.value("Global/dbRetainDays", 0).toInt();

	if (!autoClean || retainDays <= 0) return;

	QDir dir(dbPath);
	QStringList filters;
	filters << "Exp_*.db"; //只清理实验数据库文件
	dir.setNameFilters(filters);

	QFileInfoList fileList = dir.entryInfoList();
	QDateTime now = QDateTime::currentDateTime();

	int deletedCount = 0;
	for (const QFileInfo& fileInfo : fileList) {
		//计算文件最后修改时间到现在的天数
		qint64 daysOld = fileInfo.lastModified().daysTo(now);

		if (daysOld > retainDays) {
			QFile::remove(fileInfo.absoluteFilePath());
			deletedCount++;
		}
	}

	if (deletedCount > 0) {
		emit logMessage(QString("自动清理机制已触发：删除了 %1 个过期的数据库文件。").arg(deletedCount));
	}
}