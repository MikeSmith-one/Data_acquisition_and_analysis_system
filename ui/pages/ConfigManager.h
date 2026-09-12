#pragma once

#include <QObject>
#include <QVector>
#include <QMap>
#include <QString>
#include <QSettings>
#include <QReadWriteLock>
#include <QCoreApplication>
#include <QSet>
#include <QDebug>

// #########################################################
// 数据结构定义
// #########################################################

//报警阈值结构体
struct AlarmLimit {
    double lowerLimit = 0.0; // 报警下限
    double upperLimit = 0.0; // 报警上限
};

//单个 Modbus 点位映射结构体
struct PointMapping {
    QString paramName;      // 参数名称 (如 "Tin", "Tout", "flow", "ΔT")
    int regAddr = 0;        // 寄存器地址
    QString regType;        // 寄存器类型 ("Holding" / "Input")
    QString dataFormat;     // 数据格式 ("Float", "Int16" 等)
    double scaleFactor = 1.0; // 缩放系数
};

//单工位完整配置结构体
struct StationConfig {
    int stationId = 0;                      // 工位编号 (从 0 开始)
    QString ip = "127.0.0.1";               // 设备 IP 地址
    int port = 502;                         // TCP 端口号
    int slaveId = 1;                        // 从站 ID (Unit ID)
    int pollPeriod = 1000;                  // 轮询周期 (ms)
    QVector<PointMapping> mappings;         // 点位映射列表
    QMap<QString, AlarmLimit> alarmLimits;  // 报警阈值字典 (Key: 参数名, Value: 上下限)
};

// #########################################################
// ConfigManager：全局线程安全配置单例类 (Header-Only)
// #########################################################
class ConfigManager : public QObject {
    Q_OBJECT

public:
    static ConfigManager* instance() {
        static ConfigManager inst;
        return &inst;
    }

    void setIniPath(const QString& path) {
        QWriteLocker locker(&m_lock);
        m_iniPath = path;
    }

    QString getIniPath() const {
        QReadLocker locker(&m_lock);
        return m_iniPath;
    }

    int getStationCount() const {
        QReadLocker locker(&m_lock);
        QSettings settings(m_iniPath, QSettings::IniFormat);
        return settings.value("Global/StationCount", 1).toInt();
    }

    QVector<StationConfig> getStationsConfig() const {
        QReadLocker locker(&m_lock);
        return m_stations;
    }
	// 读取配置文件并解析为 m_stations
    void loadConfigFromIni() {
		QWriteLocker locker(&m_lock);//加锁，确保线程安全
        m_stations.clear();

        if (m_iniPath.isEmpty()) {
            m_iniPath = QCoreApplication::applicationDirPath() + "/config/dev_config.ini";
        }
		//读取全局工位数量
        QSettings settings(m_iniPath, QSettings::IniFormat);
        int count = settings.value("Global/StationCount", 0).toInt();
		//读取每个工位的配置
        for (int i = 0; i < count; ++i) {
            StationConfig station;
            station.stationId = i;
            QString stationPrefix = QString("Station_%1").arg(i + 1);

            //读取通信参数 (Station_X_Comm 节点)
            QString commGroup = QString("%1_Comm").arg(stationPrefix);
            settings.beginGroup(commGroup);
            for (const QString& key : settings.childKeys()) {
                QString val = settings.value(key).toString();
                if (key.contains("IP")) {
                    station.ip = val;
                }
                else if (key.contains("端口") || key.contains("Port")) {
                    station.port = val.toInt();
                }
                else if (key.contains("从站") || key.contains("SlaveID")) {
                    station.slaveId = val.toInt();
                }
                else if (key.contains("轮询周期") || key.contains("PollPeriod")) {
                    station.pollPeriod = val.toInt();
                }
            }
            settings.endGroup();

            //解析点位映射 (Station_X_DataMapping 节点，精确剥离后缀)
            QString dataGroup = QString("%1_DataMapping").arg(stationPrefix);
            settings.beginGroup(dataGroup);
            QStringList dataKeys = settings.childKeys();
			//构建参数名集合，去除后缀 "_reg_addr", "_reg_type", "_data_format", "_scale_factor"
            QSet<QString> paramNames;
            for (const QString& key : dataKeys) {
                QString prefix = key;
                if (prefix.endsWith("_reg_addr", Qt::CaseInsensitive)) {
                    prefix.chop(9);
                }
                else if (prefix.endsWith("_reg_type", Qt::CaseInsensitive)) {
                    prefix.chop(9);
                }
                else if (prefix.endsWith("_data_format", Qt::CaseInsensitive)) {
                    prefix.chop(12);
                }
                else if (prefix.endsWith("_scale_factor", Qt::CaseInsensitive)) {
                    prefix.chop(13);
                }
                else {
                    continue;
                }
                if (!prefix.isEmpty()) {
                    paramNames.insert(prefix);
                }
            }

			//根据参数名集合构建点位映射列表
            for (const QString& paramName : paramNames) {
                PointMapping map;
                map.paramName = paramName;
                map.regAddr = settings.value(QString("%1_reg_addr").arg(paramName),
                    settings.value(QString("%1_RegAddr").arg(paramName), 0).toInt()).toInt();
                map.regType = settings.value(QString("%1_reg_type").arg(paramName),
                    settings.value(QString("%1_RegType").arg(paramName), "Holding").toString()).toString();
                map.dataFormat = settings.value(QString("%1_data_format").arg(paramName),
                    settings.value(QString("%1_DataFormat").arg(paramName), "Float").toString()).toString();
                map.scaleFactor = settings.value(QString("%1_scale_factor").arg(paramName),
                    settings.value(QString("%1_ScaleFactor").arg(paramName), 1.0).toDouble()).toDouble();

                station.mappings.append(map);
            }
            settings.endGroup();

            //解析报警阈值 (Station_X_Alarm 节点)
            QString alarmGroup = QString("%1_Alarm").arg(stationPrefix);
            settings.beginGroup(alarmGroup);
            QStringList alarmKeys = settings.childKeys();
			//构建参数名集合，去除后缀 "_limit_lower", "_limit_upper"
            for (const QString& key : alarmKeys) {
                if (key.endsWith("_limit_lower")) {
                    QString param = key.left(key.length() - QString("_limit_lower").length());
                    station.alarmLimits[param].lowerLimit = settings.value(key).toDouble();
                }
                else if (key.endsWith("_limit_upper")) {
                    QString param = key.left(key.length() - QString("_limit_upper").length());
                    station.alarmLimits[param].upperLimit = settings.value(key).toDouble();
                }
            }
            settings.endGroup();

            m_stations.append(station);
        }
    }
signals:
    void configUpdated(int stationCount);

private:
    ConfigManager() = default;
    ~ConfigManager() = default;
    ConfigManager(const ConfigManager&) = delete;
    ConfigManager& operator=(const ConfigManager&) = delete;

    QString m_iniPath;
    QVector<StationConfig> m_stations;
    mutable QReadWriteLock m_lock;
};