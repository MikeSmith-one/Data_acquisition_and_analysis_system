#pragma once

#include <QObject>
#include <QTcpSocket>
#include <QTimer>
#include <QMap>
#include <QVector>
#include "StationData.h"
#include "ui/pages/ConfigManager.h"

// ==========================================
// 单工位通信上下文 (内部类)
// 维护独立的 FSM、Socket 和 定时器
// ==========================================
class StationContext : public QObject {
    Q_OBJECT
public:
    enum State {
        Disconnected,
        Connecting,
        Idle,
        WaitingHoldingResponse, // 等待 0x03 响应
        WaitingInputResponse    // 等待 0x04 响应
    };

    explicit StationContext(const StationConfig& config, QObject* parent = nullptr);
    ~StationContext();

    //开启通信与停止通信函数
    void startAcquisition();
    void stopAcquisition();

signals:
    void dataReady(const StationData& data);
    void logMessage(const QString& msg);

private slots:
    void onSocketConnected();
    void onSocketDisconnected();
    void onSocketReadyRead(); //读取响应
    void onSocketError(QAbstractSocket::SocketError error);

    //定时器响应槽函数
    void onPollTimeout();
    void onResponseTimeout();
    void onReconnectTimeout();

private:
    void changeState(State newState);//状态改变函数
    //void sendModbusRequest();
    void sendReadRequest(quint8 fc, int minAddr, int regCount);
    //void parseModbusResponse(const QByteArray& response);//解析响应报文
    void parseModbusResponse(quint8 fc, const QByteArray& response);
    void resetBackoff();//重置指数退避值

private:
    StationConfig m_config;
    State m_state = Disconnected;
    bool m_isAcquiring = false; //当前是否处于采集状态

    QTcpSocket* m_socket = nullptr;         
    QTimer* m_pollTimer = nullptr;          //推送报文定时器
    QTimer* m_timeoutTimer = nullptr;       //响应超时重连定时器
    QTimer* m_reconnectTimer = nullptr;     //断线重连定时器

    int m_transactionId = 0;
    int m_currentBackoff = 2000; //指数退避初始值 2s 用于断线重连

    //按寄存器类型拆分边界记录
    int m_holdingMinAddr = 65535, m_holdingMaxAddr = 0, m_holdingRegCount = 0;
    int m_inputMinAddr = 65535, m_inputMaxAddr = 0, m_inputRegCount = 0;

    //用于暂存当前轮询周期内解析的数据，等所有请求完成后统一发出
    StationData m_currentData;
};

// ==========================================
// 全局 Modbus 管理器 (运行在独立线程)
// ==========================================
class ModbusManager : public QObject {
    Q_OBJECT
public:
    explicit ModbusManager(QObject* parent = nullptr);
    ~ModbusManager();

public slots:
    // 接收动态配置并重建通信上下文 (由主线程调用，Qt会自动排队到子线程执行)
    void applyConfigurations(const QVector<StationConfig>& configs);

    // 新增：对外暴露的启停槽函数 (供 UI 按钮调用)
    void startAcquisition();
    void stopAcquisition();

signals:
    // 无状态数据分发
    void stationDataReceived(const StationData& data);
    // 日志分发
    void logMessage(const QString& msg);

private:
    // 停止所有通信
    void clearAllContexts();

private:
    QMap<int, StationContext*> m_contexts; //各个工位的StationContext Key: stationId
    bool m_isGlobalAcquiring = false; //全局采集状态标志
};