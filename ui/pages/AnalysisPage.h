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
#include <QStringListModel>
#include <QHeaderView>

QT_BEGIN_NAMESPACE
namespace Ui { class AnalysisPageClass; };
QT_END_NAMESPACE

//用于listView和TableWidget的数据结构体
struct MetricInfo2 {
    QString id;
    QString name;
    QString unit;
};

class AnalysisPage : public QWidget
{
    Q_OBJECT

public:
    AnalysisPage(QWidget* parent = nullptr);
    ~AnalysisPage();

private slots:
    //日期切换时，数据库筛选槽函数
    void onDateRangeChanged();

    //通过信号槽连接，监听“提取模式”下拉框的索引变化事件，实现动态更新右侧单位 Label 和输入框占位符
    void onAnalysisExtractionModeChanged(int index);

private:
	//初始化界面
    void initUI();
    QString loadQss();

    //加载数据到“提取模式”的combboBox中
    void loadAnalysisExtractionModesFromJson();

    //初始化日期选择板块
    void initDateFilters();

    Ui::AnalysisPageClass* ui;
};

