#include "ModbusManager.h"
#include <QDataStream>
#include <QDateTime>
#include <QtEndian>
#include <QDebug>

// ==========================================
// StationContext 实现
// ==========================================
StationContext::StationContext(const StationConfig& config, QObject* parent)
    : QObject(parent), m_config(config)
{
    m_socket = new QTcpSocket(this);
    m_pollTimer = new QTimer(this);
    m_timeoutTimer = new QTimer(this);
    m_reconnectTimer = new QTimer(this);

    //定时器配置
    m_timeoutTimer->setSingleShot(true);
    m_reconnectTimer->setSingleShot(true);

    //信号槽连接
    connect(m_socket, &QTcpSocket::connected, this, &StationContext::onSocketConnected);
    connect(m_socket, &QTcpSocket::disconnected, this, &StationContext::onSocketDisconnected);
    connect(m_socket, &QTcpSocket::readyRead, this, &StationContext::onSocketReadyRead);
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    connect(m_socket, &QTcpSocket::errorOccurred, this, &StationContext::onSocketError);
#else
    connect(m_socket, QOverload<QAbstractSocket::SocketError>::of(&QTcpSocket::error), this, &StationContext::onSocketError);
#endif

    connect(m_pollTimer, &QTimer::timeout, this, &StationContext::onPollTimeout);
    connect(m_timeoutTimer, &QTimer::timeout, this, &StationContext::onResponseTimeout);
    connect(m_reconnectTimer, &QTimer::timeout, this, &StationContext::onReconnectTimeout);

    //分别计算 Holding Registers (FC 0x03) 与 Input Registers (FC 0x04) 的边界
    for (const auto& mapping : m_config.mappings) {
        int regsNeeded = (mapping.dataFormat.toUpper() == "FLOAT") ? 2 : 1;
        QString regType = mapping.regType.toUpper();

        if (regType == "INPUT") {
            m_inputMinAddr = qMin(m_inputMinAddr, (int)mapping.regAddr);
            m_inputMaxAddr = qMax(m_inputMaxAddr, (int)mapping.regAddr + regsNeeded - 1);
        }
        else { //默认按 Holding 处理
            m_holdingMinAddr = qMin(m_holdingMinAddr, (int)mapping.regAddr);
            m_holdingMaxAddr = qMax(m_holdingMaxAddr, (int)mapping.regAddr + regsNeeded - 1);
        }
    }

    m_holdingRegCount = (m_holdingMinAddr <= m_holdingMaxAddr) ? (m_holdingMaxAddr - m_holdingMinAddr + 1) : 0;
    m_inputRegCount = (m_inputMinAddr <= m_inputMaxAddr) ? (m_inputMaxAddr - m_inputMinAddr + 1) : 0;
}

StationContext::~StationContext() {
    stopAcquisition();
}

//开始采集
void StationContext::startAcquisition() {
    if (m_isAcquiring) return;
    m_isAcquiring = true;

    //防呆提示：如果发现没有配置任何寄存器，打印警告
    if (m_holdingRegCount <= 0 && m_inputRegCount <= 0) {
        QString warnMsg = QString("工位 %1 启动采集失败：未读取到任何有效的寄存器映射配置！请检查配置文件。").arg(m_config.stationId + 1);
        emit logMessage(warnMsg);
        qWarning() << warnMsg;
        return; //直接返回，不发起连接
    }

    if (m_state == Disconnected && (m_holdingRegCount > 0 || m_inputRegCount > 0)) {
        resetBackoff();
        changeState(Connecting);
        m_socket->connectToHost(m_config.ip, m_config.port);
    }
}

//停止采集
void StationContext::stopAcquisition() {
    m_isAcquiring = false;

    //停止所有定时器
    m_pollTimer->stop();
    m_timeoutTimer->stop();
    m_reconnectTimer->stop();

    //主动断开连接，释放下位机资源
    if (m_socket->isOpen()) {
        m_socket->abort();
    }
    changeState(Disconnected);
}

void StationContext::changeState(State newState) {
    m_state = newState;
}

void StationContext::resetBackoff() {
    m_currentBackoff = 2000;//初始 2 秒
}

void StationContext::onSocketConnected() {
    emit logMessage(QString("工位 %1 成功连接到 %2:%3").arg(m_config.stationId + 1).arg(m_config.ip).arg(m_config.port));
	qDebug() << "Connected to" << m_config.ip << ":" << m_config.port;
    resetBackoff();
    changeState(Idle);

    //只有在处于采集状态时才启动轮询
    if (m_isAcquiring) {
        m_pollTimer->start(m_config.pollPeriod);
        QTimer::singleShot(0, this, &StationContext::onPollTimeout);
    }
}

void StationContext::onSocketDisconnected() {
    if (m_state != Disconnected) {
        changeState(Disconnected);
        m_pollTimer->stop();
        m_timeoutTimer->stop();

        //发送离线状态
        StationData data;
        data.stationId = m_config.stationId;
        data.isOnline = false;
        emit dataReady(data);

        //只有在“正在采集”期间发生的意外断线，才触发指数退避重连
        if (m_isAcquiring) {
            emit logMessage(QString("工位 %1 断开连接，%2 秒后重试...").arg(m_config.stationId + 1).arg(m_currentBackoff / 1000));
			qDebug() << "Disconnected from" << m_config.ip << ":" << m_config.port << ", retrying in" << m_currentBackoff / 1000 << "seconds";
            m_reconnectTimer->start(m_currentBackoff);
            m_currentBackoff = qMin(m_currentBackoff * 2, 15000);//最大 15 秒
        }
        else {
            emit logMessage(QString("工位 %1 采集已停止，连接断开").arg(m_config.stationId + 1));
			qDebug() << "Disconnected from" << m_config.ip << ":" << m_config.port << ", acquisition stopped";
        }
    }
}

void StationContext::onSocketError(QAbstractSocket::SocketError error) {
    Q_UNUSED(error);
    m_socket->abort(); //触发 disconnected 信号
}

void StationContext::onReconnectTimeout() {
    changeState(Connecting);
    m_socket->connectToHost(m_config.ip, m_config.port);
}

//优先请求 Holding，如果没有则请求 Input
void StationContext::onPollTimeout() {
    if (m_state == Idle) {
        // 初始化当前轮询的数据包
        m_currentData.stationId = m_config.stationId;
        m_currentData.isOnline = true;
        m_currentData.values.clear();

        if (m_holdingRegCount > 0) {
            sendReadRequest(0x03, m_holdingMinAddr, m_holdingRegCount);
            changeState(WaitingHoldingResponse);
        }
        else if (m_inputRegCount > 0) {
            sendReadRequest(0x04, m_inputMinAddr, m_inputRegCount);
            changeState(WaitingInputResponse);
        }
    }
}

void StationContext::onResponseTimeout() {
    emit logMessage(QString("工位 %1 响应超时，断开重连").arg(m_config.stationId + 1));
	qDebug() << "Response timeout for station" << m_config.stationId + 1 << ", aborting connection for reconnection";
    m_socket->abort(); //超时直接断开，走指数退避重连逻辑
}

void StationContext::sendReadRequest(quint8 fc, int minAddr, int regCount) {
    if (regCount <= 0 || regCount > 125) return;

    m_transactionId = (m_transactionId + 1) % 65535;

    QByteArray request;
    QDataStream stream(&request, QIODevice::WriteOnly);
    stream.setByteOrder(QDataStream::BigEndian);

    // MBAP Header
    stream << (quint16)m_transactionId;
    stream << (quint16)0x0000;
    stream << (quint16)0x0006;
    stream << (quint8)m_config.slaveId;

    // PDU
    stream << (quint8)fc;
    stream << (quint16)minAddr;
    stream << (quint16)regCount;

    m_socket->write(request);
    m_timeoutTimer->start(2000);
}

void StationContext::onSocketReadyRead() {
    if (m_state != WaitingHoldingResponse && m_state != WaitingInputResponse)//只有当状态机处于 WaitingResponse（等待响应）时，才认为发出的请求与收到的数据匹配
    {
        m_socket->readAll();//丢弃非预期数据
        return;
    }

    //Modbus TCP 最小响应长度为 9 字节 (MBAP 7 + FC 1 + ByteCount 1)
    if (m_socket->bytesAvailable() < 9) return;

    QByteArray header = m_socket->peek(9);
    QDataStream headerStream(header);
    headerStream.setByteOrder(QDataStream::BigEndian);

    quint16 transId, protoId, length;//事务处理标识符、协议标识符、长度
    quint8 unitId, fc, byteCount;//单位标识符、功能码、字节数
    headerStream >> transId >> protoId >> length >> unitId >> fc >> byteCount;

    //校验报文完整性 (length 字段表示 Unit ID 之后的字节数)
    if (m_socket->bytesAvailable() < (6 + length)) return;

    //读取完整报文
    QByteArray response = m_socket->read(6 + length);
    m_timeoutTimer->stop();

    //changeState(Idle);

    if (transId != m_transactionId) return;

    //判断从站异常响应 (功能码最高位置 1)
    if (fc & 0x80) {
        quint8 exceptionCode = (response.size() >= 9) ? static_cast<quint8>(response.at(8)) : 0;
        emit logMessage(QString("工位 %1 收到 Modbus 异常响应！功能码: 0x%2, 错误码: 0x%3")
            .arg(m_config.stationId + 1)
            .arg(fc, 2, 16, QChar('0'))
            .arg(exceptionCode, 2, 16, QChar('0')));
        // 发生异常时，跳过当前解析，直接进入下一步
    }
    else {
        // 正常解析
        parseModbusResponse(fc, response.mid(9, byteCount));
    }
    
    //两阶段状态流转
    if (m_state == WaitingHoldingResponse) {
        //Holding 读完了，检查是否需要读 Input
        if (m_inputRegCount > 0) {
            sendReadRequest(0x04, m_inputMinAddr, m_inputRegCount);
            changeState(WaitingInputResponse);
        }
        else {
            //不需要读 Input，本轮结束
            changeState(Idle);
            emit dataReady(m_currentData);
        }
    }
    else if (m_state == WaitingInputResponse) {
        //Input 读完了，本轮结束
        changeState(Idle);
        emit dataReady(m_currentData);
    }
}

void StationContext::parseModbusResponse(quint8 fc, const QByteArray& payload) {

    QDataStream stream(payload);
    stream.setByteOrder(QDataStream::BigEndian);//QDataStream默认是小端排序，所以每次用需要指定为大端排序

    int currentMinAddr = (fc == 0x03) ? m_holdingMinAddr : m_inputMinAddr;
    QString targetRegType = (fc == 0x03) ? "HOLDING" : "INPUT";

    for (const auto& mapping : m_config.mappings) {
        //过滤掉非当前请求类型的点位
        QString regType = mapping.regType.toUpper();
        if (regType.isEmpty()) regType = "HOLDING"; //兼容空配置默认 Holding
        if (regType != targetRegType) continue;

        int byteOffset = (mapping.regAddr - currentMinAddr) * 2;//字节偏移量计算（默认值 m_minAddr = 100, m_maxAddr = 113）
        if (byteOffset < 0 || byteOffset + 2 > payload.size()) continue;

        QString format = mapping.dataFormat.toUpper();//调整为全大写
        double realValue = 0.0;//真实的数据

        if (format == "FLOAT" && byteOffset + 4 <= payload.size()) {
            //解析 32位 Float (假设标准的 ABCD 大端序)
            quint32 raw;
            QDataStream valStream(payload.mid(byteOffset, 4));
            valStream.setByteOrder(QDataStream::BigEndian);
            valStream >> raw;
            float fVal;
            memcpy(&fVal, &raw, sizeof(float));
            realValue = fVal;
        }
        else if (format == "INT16") {
            qint16 raw;
            QDataStream valStream(payload.mid(byteOffset, 2));
            valStream.setByteOrder(QDataStream::BigEndian);
            valStream >> raw;
            realValue = raw;
        }
        else if (format == "UINT16") {
            quint16 raw;
            QDataStream valStream(payload.mid(byteOffset, 2));
            valStream.setByteOrder(QDataStream::BigEndian);
            valStream >> raw;
            realValue = raw;
        }

        //存入当前轮询的缓存中
        m_currentData.values[mapping.paramName] = realValue * mapping.scaleFactor;
    }
}

// ==========================================
// ModbusManager 实现
// ==========================================
ModbusManager::ModbusManager(QObject* parent) : QObject(parent) {
    qRegisterMetaType<StationData>("StationData");

    // 【新增】注册自定义类型，允许它们在跨线程队列中使用
    //qRegisterMetaType<StationConfig>("StationConfig");
    //qRegisterMetaType<QVector<StationConfig>>("QVector<StationConfig>");
}

ModbusManager::~ModbusManager() {
    clearAllContexts();
}

void ModbusManager::applyConfigurations(const QVector<StationConfig>& configs) {
    clearAllContexts();

    for (const auto& config : configs) {
        StationContext* ctx = new StationContext(config, this);

        //转发内部信号到外部
        connect(ctx, &StationContext::dataReady, this, &ModbusManager::stationDataReceived);
        connect(ctx, &StationContext::logMessage, this, &ModbusManager::logMessage);

        m_contexts.insert(config.stationId, ctx);

        // 如果当前全局处于采集状态，新应用配置后应立即启动采集
        if (m_isGlobalAcquiring) {
            ctx->startAcquisition();
        }
    }

    emit logMessage(QString("Modbus 通信服务已加载 %1 个工位配置并启动").arg(configs.size()));
	qDebug() << "Modbus communication service loaded" << configs.size() << "station configurations and started";
}

//开始采集槽函数
void ModbusManager::startAcquisition() {
    if (m_isGlobalAcquiring) return;
    m_isGlobalAcquiring = true;

    for (auto ctx : m_contexts) {
        ctx->startAcquisition();
    }
    emit logMessage("系统已启动全局数据采集");
	qDebug() << "Global data acquisition started";
}

//停止采集槽函数
void ModbusManager::stopAcquisition() {
    if (!m_isGlobalAcquiring) return;
    m_isGlobalAcquiring = false;

    for (auto ctx : m_contexts) {
        ctx->stopAcquisition();
    }
    emit logMessage("系统已停止全局数据采集");
	qDebug() << "Global data acquisition stopped";
}

void ModbusManager::clearAllContexts() {
    for (auto ctx : m_contexts) {
        ctx->stopAcquisition();
        ctx->deleteLater();
    }
    m_contexts.clear();
}