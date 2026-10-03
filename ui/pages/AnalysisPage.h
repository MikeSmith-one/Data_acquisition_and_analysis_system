#pragma once

#include <QtWidgets/QWidget>
#include "ui_AnalysisPage.h"
#include <QChart>
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonParseError>
#include <QDebug>
#include <QList>
#include <QStringListModel>
#include <QHeaderView>
#include <QFileDialog>
#include <QDesktopServices>
#include <QUrl>
#include <QSettings>
#include <QCoreApplication>
#include <QMessageBox>
#include <QDir>
#include <QStandardItemModel>
#include <QStandardItem>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QDateTime>
#include <QTextStream>
#include <QValueAxis>
#include <QLineSeries>
#include <QScatterSeries>     //散点图
#include <QBoxPlotSeries>     //箱线图
#include <QBoxSet>            //箱线图数据盒
#include <QBarCategoryAxis>   //箱线图的类别 X 轴

QT_BEGIN_NAMESPACE
namespace Ui { class AnalysisPageClass; };
QT_END_NAMESPACE
// =====================================
// 用于listView和TableWidget的数据结构体
// =====================================
struct MetricInfo2 {
    QString id;
    QString name;
    QString unit;
};

//图表类型枚举
enum class ChartType { Line, Scatter, BoxPlot };

struct AnalysisChartSlot {
    ChartType type;                    //图表类型
    QString metricY;                   //Y轴绑定的指标 (如 "Tmax", "Rth", "ΔT")
    QString metricX;                   //X轴绑定的指标 (仅散点图用，如 "dp")
    QString title;                     //图表标题
    QChartView* view = nullptr;        //对应的 UI 控件 (chart1 ~ chart4)
    QChart* chart = nullptr;           //图表对象
    QValueAxis* axisX = nullptr;       //数值 X 轴 (折线图、散点图用)
    QValueAxis* axisY = nullptr;       //数值 Y 轴
    QBarCategoryAxis* axisXBar = nullptr; //类别 X 轴 (仅箱线图用)
    QMap<int, QAbstractSeries*> seriesMap; //QAbstractSeries 以兼容不同类型的 Series
};

class AnalysisPage : public QWidget
{
    Q_OBJECT

public:
    AnalysisPage(QWidget* parent = nullptr);
    ~AnalysisPage();

signals:
    void logMessage(const QString& msg);  //日志信息信号

public slots:
    //接收来自 ConfigPage 的工位数量更新信号，实现动态扩展
    void updateStationCount(int count);

    //接收来自 ConfigManager 的配置更新信号
    void onConfigReloaded(int stationCount);

private slots:
    //日期切换时，数据库筛选槽函数
    void onDateRangeChanged();

    //通过信号槽连接，监听“提取模式”下拉框的索引变化事件，实现动态更新右侧单位 Label 和输入框占位符
    void onAnalysisExtractionModeChanged(int index);

    //左侧面板相关槽函数
    void on_loadingBtn_clicked();                        // 加载数据文件
    void on_filterData_btn_clicked();                    // 筛选数据
    void on_outputData_btn_clicked();                    // 导出数据
    void on_timeModelBox_currentIndexChanged(int index); // 分析周期下拉框联动

    //ListView 勾选状态变化时的槽函数
    void onListViewItemChanged(QStandardItem* item);

    //点击“筛选数据”或改变“分析周期”时，触发图表重绘
    void updateCharts();

    //自定义时间区间的“确实”按钮槽函数
    void on_applyDiyTimeBtn_clicked();

    //加载数据到TableWidget
    QList<MetricInfo2> loadTableMetricInfoFromJson();


    //图表区域左侧按钮，隐藏或显示左侧面板
    void on_leftBtn_clicked();

    //图表区域右侧按钮，隐藏或显示右侧面板
    void on_rightBtn_clicked();


    //右侧面板“生成摘要”按钮槽函数
    void on_getDataBtn_clicked();

    //右侧面板“导出”按钮槽函数
    void on_outputBtn_clicked();

    //顶部工具栏“清理分析数据”按钮
    void on_cleanBtn_clicked();

    //顶部工具栏“导出分析报告”按钮
    void on_outputReportBtn_clicked();

    //顶部工具栏“...”更改资源文件路径按钮
    void on_toolBtn_clicked();

    //顶部工具栏“打开”按钮，打开资源文件所在位置
    void on_openSourceBtn_clicked();
private:
	//初始化界面
    void initUI();
    QString loadQss();

    //加载数据到“提取模式”的combboBox中
    void loadAnalysisExtractionModesFromJson();

    //初始化日期选择板块
    void initDateFilters();

    //刷新表格列的内部封装函数
    void updateTableColumns(int stationCount);

    //封装一个通用的扫描数据库文件函数
    void loadDatabaseFiles(const QDate& startDate = QDate(), const QDate& endDate = QDate());

    //图表相关初始化与控制函数
    void initChartPool();
    void renderChart(AnalysisChartSlot& slot, const QString& dbFilePath);

    //辅助函数：计算坐标轴刻度范围的辅助函数和计算优雅坐标轴的辅助函数
    void calculateNiceAxis(double min, double max, int tickCount, double& outMin, double& outMax);
    double niceMax(double range, bool round);

private:
    bool leftDockisShow = true; //用于控制左侧面板的显示与隐藏

    bool rightDockisShow = true; //用于控制右侧面板的显示与隐藏

    //图表插槽池
    QList<AnalysisChartSlot> m_chartSlots;

    //用于支持 CheckBox 的高级列表模型
    QStandardItemModel* m_dbListModel = nullptr;

    //将解析出的指标数据存为成员变量，供TableWidget用
    QList<MetricInfo2> m_tableMetricList;

    int m_currentStationCount = 1; //记录当前系统的工位总数

    Ui::AnalysisPageClass* ui;
};

