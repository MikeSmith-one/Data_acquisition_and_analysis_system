#pragma once
/*用于全局配置管理，将ConfigPage的配置信息集中管理，便于其它页面调用以及更新dev-config.ini配置文件*/
#include <QObject>
#include <QSettings>
#include <QVariantMap>
#include <QVector>
#include <QMap>

struct PointMapping {
    QString paramName;//参数名
    int regAddr;//寄存器地址 
    QString regType;//"Holding" / "Input"
    QString dataFormat;//"Float" / "Int16"
    double scaleFactor;//缩放系数
};

struct StationConfig {
    //通信参数
    QString protocol;//通信协议
    QString ip;
    int port;
    int slaveId;//从站ID（单位标识符）
    int pollPeriod;//轮询周期
    //数据映射与阈值
	QVector<PointMapping> mappings;//参数映射列表，存放了每个工位的配置信息
    QMap<QString, QPair<double, double>> alarmLimits;//参数名 -> <下限, 上限> //各个工位的报警阈值
};

struct GlobalConfig {
	int stationCount;//工位数量
    QVector<StationConfig> stations;//工位配置列表，存放了所有工位的配置信息
};


//全局配置管理器
class ConfigManager : public QObject {
    Q_OBJECT
public:
    static ConfigManager* instance() {
        static ConfigManager manager;
        return &manager;
    }

    //设置并获取当前 INI 文件路径
    void setIniPath(const QString& path) { m_iniPath = path; }
    QString getIniPath() const { return m_iniPath; }

    //获取工位数量 (无文件时默认返回 1)
    int getStationCount() {
        QSettings settings(m_iniPath, QSettings::IniFormat);
        return settings.value("Global/StationCount", 1).toInt();
    }

signals:
    //当配置发生更改时，发出此信号。采集线程/页面接收后重新加载数据
    void configUpdated(int currentStationCount);

private:
    ConfigManager() = default;
    QString m_iniPath;
};
