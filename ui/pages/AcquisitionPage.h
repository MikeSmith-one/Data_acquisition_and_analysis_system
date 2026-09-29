#pragma once

#include <QtWidgets/QWidget>
#include "ui_AcquisitionPage.h"
#include <QPointer>
#include <QTableWidgetItem>
#include <QChart>
#include <QValueAxis>
#include <QLineSeries>
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonParseError>
#include <QDebug>
#include <QStringListModel>
#include <QHeaderView>
#include "ConfigManager.h"
#include <QDateTimeAxis>
#include <QTimer>
#include <QDateTime>
#include <QMap>
#include <QList>
#include <QPointF>
#include <QGraphicsLayout>
#include "network/StationData.h"
#include <QMessageBox>
#include <QFileDialog>
#include <QDir>
#include <QDesktopServices>
#include <QUrl>
#include <QSettings>

QT_BEGIN_NAMESPACE
namespace Ui { class AcquisitionPageClass; };
QT_END_NAMESPACE

// =========================================================
// 用于listView和TableWidget的数据结构体
// =========================================================

//单位信息结构体
struct MetricInfo1 {
    QString id;
    QString name;
    QString unit;
};

//前向声明
class QStringListModel;

// =========================================================
// 用于QChart的数据结构体
// =========================================================

//插槽结构体，用于管理每个图表的状态和数据
struct ChartSlot {
    bool isOccupied = false;           //是否被占用
    QString currentMetricId;           //当前绑定的指标 ID (如 "Tin")
    QString currentMetricName;         //当前绑定的指标名称
    QChartView* view = nullptr;        //对应的 UI 控件 (chart1 ~ chart4)
    QChart* chart = nullptr;           //图表对象
    QValueAxis* axisX = nullptr;       //X轴 (时间)
    QValueAxis* axisY = nullptr;       //Y轴 (数值)
    QMap<int, QLineSeries*> seriesMap; //存放各个工位的曲线 (Key: stationId)
};

class AcquisitionPage : public QWidget
{
    Q_OBJECT

public:
    AcquisitionPage(QWidget* parent = nullptr);
    ~AcquisitionPage();

signals:
    void startAcquisitionRequested(); // 申请开始采集
    void stopAcquisitionRequested();  // 申请停止采集

public slots:
    //接收来自 ConfigPage 的工位数量更新信号，实现动态扩展
    void updateStationCount(int count);

	//接收来自 ConfigManager 的配置更新信号
    void onConfigReloaded(int stationCount);

    //接收来自 MainWindow 的实时数据，并更新界面显示
    void updateStationData(const StationData& data);

private slots:
	//监听“停止采集”按钮点击事件，用于触发数据采集停止逻辑
    void on_stopCollectBtn_clicked();

	//监听“开始采集”按钮点击事件，用于触发数据采集逻辑
    void on_startCollectBtn_clicked();

	//用于处理ListView的选择变化事件，进而实现选中数量上限为4个
    void onListViewSelectionChanged(const QItemSelection& selected, const QItemSelection& deselected);

    //监听电机控制下拉框切换事件 (基于 UI 控件自动关联)
    void on_comboMotorStation_currentIndexChanged(int index);

	//通过信号槽连接，监听“提取模式”下拉框的索引变化事件，实现动态更新右侧单位 Label 和输入框占位符
    void onAcquisitionExtractionModeChanged(int index);

    //定时刷新图表的槽函数
    void onRenderTimerTimeout();


	//图表区域左侧按钮，隐藏或显示左侧面板
	void on_leftBtn_clicked();

	//图表区域右侧按钮，隐藏或显示右侧面板
	void on_rightBtn_clicked();


    //右侧面板“提取数据”按钮槽函数
    void on_getDataBtn_clicked();

    //右侧面板“导出”按钮槽函数
    void on_outputBtn_clicked();

    //顶部工具栏“清理缓存”按钮槽函数
    void on_cleanBtn_clicked();

    //顶部工具栏“...”按钮槽函数 (选择数据库存储目录)
    void on_toolButton_clicked();

    //顶部工具栏“打开”按钮槽函数 (打开数据库存储目录)
    void on_openBtn_clicked();
private:
    //ConfigManager 测试函数，读取配置文件并打印到控制台
    //void testConfigManager();

    //初始化界面 
    void initUI();
    QString loadQss();


    //图表池相关初始化与控制函数
    void initChartPool();
    void assignChartSlot(const QString& metricId, const QString& metricName);
    void freeChartSlot(const QString& metricId);
    void updateSeriesForStationCount(ChartSlot& slot); //根据工位数量动态增删曲线

    //刷新表格列的内部封装函数
    void updateTableColumns(int stationCount);

    //加载数据到“提取模式”的combboBox中
    void loadAcquisitionExtractionModesFromJson();

private:
    //加载数据到ListView
    QList<MetricInfo1> loadMetricInfoFromJson();
    void initListViewModel(const QStringList& data);

    //加载数据到TableWidget
    QList<MetricInfo1> loadTableMetricInfoFromJson();

private:
	bool leftDockisShow = true; //用于控制左侧面板的显示与隐藏

	bool rightDockisShow = true; //用于控制右侧面板的显示与隐藏

    Ui::AcquisitionPageClass* ui;
    QPointer<QStringListModel> m_listViewModel;

    //将解析出的指标数据存为成员变量，供ListView用
    QList<MetricInfo1> m_metricList;

    //将解析出的指标数据存为成员变量，供TableWidget用
    QList<MetricInfo1> m_tableMetricList;

    //Session Buffer (全量缓存)：Key=工位ID, Value=该工位的所有历史数据包
    QMap<int, QList<StationData>> m_sessionBuffer;

    //Session Buffer 最大保留时间 (24小时 = 86400000 毫秒)
    const qint64 MAX_SESSION_DURATION_MS = 86400000;


    //图表插槽池与环形缓冲区
    QList<ChartSlot> m_chartSlots;
    int m_currentStationCount = 1; //记录当前系统的工位总数

    //环形缓冲区：Key1=指标ID, Key2=工位ID, Value=坐标点集合
    QMap<QString, QMap<int, QList<QPointF>>> m_chartDataBuffer;
    const int MAX_BUFFER_SIZE = 1000; //环形缓冲区最大长度 (保留最近 1000 个点，约 1000 秒)

    QTimer* m_renderTimer = nullptr; //控制图表刷新频率的定时器

    //控制秒表与图表冻结的变量
    bool m_isAcquiring = false;          //当前是否正在采集
    qint64 m_acquisitionStartTime = 0;   //点击“开始采集”时的系统绝对时间戳
    double m_currentRelativeSecs = 0.0;  //当前流逝的相对秒数
};

