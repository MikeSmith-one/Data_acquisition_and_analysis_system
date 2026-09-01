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

QT_BEGIN_NAMESPACE
namespace Ui { class AcquisitionPageClass; };
QT_END_NAMESPACE

//用于listView和TableWidget的数据结构体
struct MetricInfo1 {
    QString id;
    QString name;
    QString unit;
};

class QStringListModel;

class AcquisitionPage : public QWidget
{
    Q_OBJECT

public:
    AcquisitionPage(QWidget* parent = nullptr);
    ~AcquisitionPage();

public slots:
    //接收来自 ConfigPage 的工位数量更新信号，实现动态扩展
    void updateStationCount(int count);

	//接收来自 ConfigManager 的配置更新信号
    void onConfigReloaded(int stationCount);

private slots:
	//用于处理ListView的选择变化事件，进而实现选中数量上限为4个
    void onListViewSelectionChanged(const QItemSelection& selected, const QItemSelection& deselected);

    //监听电机控制下拉框切换事件 (基于 UI 控件自动关联)
    void on_comboMotorStation_currentIndexChanged(int index);

	//通过信号槽连接，监听“提取模式”下拉框的索引变化事件，实现动态更新右侧单位 Label 和输入框占位符
    void onAcquisitionExtractionModeChanged(int index);

private:
    //初始化界面
    void initUI();
    QString loadQss();

    //加载数据到ListView
    QList<MetricInfo1> loadMetricInfoFromJson();
    void initListViewModel(const QStringList& data);

    //加载数据到TableWidget
    QList<MetricInfo1> loadTableMetricInfoFromJson();

    //刷新表格列的内部封装函数
    void updateTableColumns(int stationCount);

	//加载数据到“提取模式”的combboBox中
    void loadAcquisitionExtractionModesFromJson();

    Ui::AcquisitionPageClass* ui;
    QPointer<QStringListModel> m_listViewModel;

    //将解析出的指标数据存为成员变量，供ListView用
    QList<MetricInfo1> m_metricList;

    //将解析出的指标数据存为成员变量，供TableWidget用
    QList<MetricInfo1> m_tableMetricList;
};

