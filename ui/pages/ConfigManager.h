#pragma once
/*用于全局配置管理，将ConfigPage的配置信息集中管理，便于其它页面调用以及更新dev-config.ini配置文件*/
#include <QObject>
#include <QSettings>
#include <QVariantMap>
#include <QVector>
#include <QMap>

struct PointMapping {
    QString paramName;
    int regAddr;//寄存器地址 
    QString regType;//"Holding" / "Input"
    QString dataFormat;//"Float" / "Int16"
    double scaleFactor;
};

struct StationConfig {
    //通信参数
    QString protocol;
    QString ip;
    int port;
    int slaveId;
    int pollPeriod;
    //数据映射与阈值
    QVector<PointMapping> mappings;
    QMap<QString, QPair<double, double>> alarmLimits;//参数名 -> <下限, 上限>
};

struct GlobalConfig {
    int stationCount;
    QVector<StationConfig> stations;
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
