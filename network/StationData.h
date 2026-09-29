#pragma once

#include <QMap>
#include <QString>
#include <QMetaType>

//单个工位的实时数据快照
struct StationData {
    int stationId = 0;                  //工位ID
    bool isOnline = false;              //当前是否在线
    qint64 timestamp = 0;               //数据的绝对时间戳 (从1970年算起的毫秒数)，作为数据的"出生证明"
    QMap<QString, double> values;       //解析并缩放后的物理量 (Key: 参数名, Value: 实际值)
};

//注册为 Qt 元类型，允许通过跨线程的 Signal/Slot 传递
Q_DECLARE_METATYPE(StationData)

//报警信息结构体
struct AlarmInfo {
    int stationId = 0;          //发生报警的工位 ID
    QString paramName;          //异常的参数名 (如 "Tin")
    double currentValue = 0.0;  //当前异常值
    double limitValue = 0.0;    //触碰到的阈值
    bool isUpperLimit = true;   //true: 超上限, false: 超下限
};
Q_DECLARE_METATYPE(AlarmInfo)