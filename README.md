# 🌡️ 液冷散热测试装置数据采集分析系统
**(Data Acquisition and Analysis System for Liquid Cooling Device)**

![C++](https://img.shields.io/badge/C++-17-blue.svg)
![Qt](https://img.shields.io/badge/Qt-6.x-41CD52.svg)
![SQLite](https://img.shields.io/badge/SQLite-3-003B57.svg)
![License](https://img.shields.io/badge/License-MIT-green.svg)

## 📖 项目简介

本项目是一款专为液冷散热装置（如冷板、散热器）热力学测试研发的**工业级上位机监控与分析软件 (SCADA)**。
系统支持 1~N 个测试工位的并发数据采集，具备实时曲线绘制、多级报警监控、海量数据高频存储、历史数据多维度可视化分析以及自动化报告生成等核心功能。

项目在架构设计上严格遵循**“高并发、高可用、低耦合”**原则，完美解决了传统上位机软件常见的 UI 假死、数据竞态、内存泄漏及大文件查询卡顿等痛点。

---

## ✨ 核心功能与特性

### 📡 1. 纯异步多线程 Modbus 通信
*   **非阻塞架构**：基于 `QTcpSocket` 底层重写 Modbus TCP 协议栈，运行于独立子线程，彻底杜绝 UI 卡顿。
*   **有限状态机 (FSM)**：各工位维护独立状态机，支持断线自动**指数退避重连**，有效防止网络风暴。
*   **软测量技术 (Soft Sensing)**：在硬件传感器受限时，内置流体力学经验公式，实时动态推算压降（ΔP）等衍生指标。

### 📈 2. 极速实时监控与动态 UI
*   **双层缓存机制**：
    *   **UI 环形缓存 (Ring Buffer)**：定长队列专供 `QChart` 渲染，保证多线并发滚动时的极致丝滑。
    *   **全量会话缓存 (Session Buffer)**：驻留内存，支持毫秒级的“最近邻搜索”与“区间求平均”数据快照提取。
*   **数据驱动 UI**：摒弃硬编码，通过解析 JSON 配置文件动态生成参数表格与下拉菜单，极具可扩展性。

### 💾 3. 工业级高频数据存储
*   **按次建库策略**：每次启动采集自动生成独立的 SQLite 数据库文件（如 `Exp_20231027.db`），天然契合实验批次管理，避免单文件无限膨胀。
*   **WAL 模式与批量事务**：开启 SQLite 预写日志模式（Write-Ahead Logging），实现读写互不阻塞；采用 `execBatch` 配合容量与时间双触发机制，实现极速落盘且防断电丢失。
*   **动态降采样 (Downsampling)**：支持自定义数据库存储周期（如 5s/次），自动过滤高频冗余数据。

### 📊 4. 历史数据多维可视化分析
*   **短连接模式**：分析页采用即用即毁的数据库连接策略，彻底杜绝文件句柄占用和多线程死锁。
*   **SQL 聚合计算**：将稳态平均、峰值工况等复杂计算交由底层数据库引擎执行，大幅降低内存开销。
*   **自适应优雅刻度 (Nice Numbers)**：内置统计算法，根据历史数据的极差自动计算并调整坐标轴刻度，支持鼠标框选无损放大，呈现商业级图表质感。

### 🚨 5. 全局报警与日志追溯
*   **防抖报警锁存**：底层精准比对阈值，触发非模态合并弹窗，避免“报警风暴”。
*   **按天轮转日志 (Daily Rolling)**：系统生命体征开机即录，按天生成 `.log` 文件，支持内存级的高速多条件关键字检索。

## 🏗️ 系统架构图 (Architecture)

系统采用星型路由与高度解耦的数据流转机制：
*   **单一数据源 (Single Source of Truth)**：由底层网络模块在解析完成的绝对瞬间打上时间戳，保证全系统数据在时间维度上的绝对一致性。
*   **线程隔离**：`UI 线程`、`Modbus 采集线程`、`SQLite 存储线程` 严格物理隔离，通过 `Qt::QueuedConnection` 安全通信。

```text
[下位机 PLC/仪表] 
       │ (Modbus TCP)
       ▼
[ModbusManager 线程] ──(打上绝对时间戳)──┐
       │                               │ (跨线程 Signal)
       ▼                               ▼
[DatabaseManager 线程]          [MainWindow 主线程路由]
       │                               │
(批量事务 + WAL模式)                   ├──► [AcquisitionPage] (环形缓存画图 / Session提取)
       │                               ├──► [AnalysisPage]    (短连接查库 / 聚合计算)
       ▼                               ├──► [ConfigPage]      (JSON驱动 / INI持久化)
[SQLite 数据库文件]                    └──► [LogPage]         (追加写入 / 内存检索)

## 🛠️ 编译与运行 (Build & Run)
操作系统：Windows 10 / 11
开发环境：Qt Creator / Visual Studio 2022
构建工具：CMake (>= 3.16)
依赖库：Qt 6 (Core, Gui, Widgets, Network, Sql, Charts)
