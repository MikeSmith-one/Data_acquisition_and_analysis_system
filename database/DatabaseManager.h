#pragma once

#include <QObject>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QList>
#include <QDir>
#include <QDateTime>
#include <QDebug>
#include <QTimer>
#include "network/StationData.h"
#include "ui/pages/ConfigManager.h"

class DatabaseManager : public QObject
{
	Q_OBJECT

public:
	explicit DatabaseManager(QObject* parent = nullptr);
	~DatabaseManager();

public slots:
	//接收底层发来的实时数据
	void receiveData(const StationData& data);

	//响应“开始采集”，创建新数据库、建表、启动定时器
	void startRecording(qint64 startTime);

	//响应“停止采集”，强制Flush缓存、关闭数据库、停止定时器
	void stopRecording();

signals:
	//用于想UI或日志系统发送消息
	void logMessage(const QString& msg);

private slots:
	//定时器触发的Flush槽函数
	void onFlushTimerTimeout();

private:
	//将内存缓存批量写入SQLite
	void flushBufferToDb();

	//自动清理过期数据库文件
	void autoCleanOldDatabases(const QString& dbPath);

private:
	QSqlDatabase m_db;						//数据库连接对象
	QString m_connectionName;				//独立的连接名称 (防多线程冲突)

	bool m_isRecording = false;				//记录状态标志
	qint64 m_sessionStartTime = 0;			//本次实验的绝对起点时间 (用于计算相对秒数)

	QList<StationData> m_dataBuffer;		//内存数据缓存 (Session Buffer 的写入缓冲)
	const int BATCH_SIZE_LIMIT = 100;		//容量触发阈值：满 100 条写入一次

	QTimer* m_flushTimer = nullptr;			//时间触发器：每 5 秒强制写入一次

	//降采样过滤相关变量
	qint64 m_savePeriodMs = 1000;       //数据库保存周期 (毫秒)
	QMap<int, qint64> m_lastSaveTime;   //记录每个工位上一次存入数据库的绝对时间戳
};