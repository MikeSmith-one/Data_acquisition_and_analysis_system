#pragma once

#include <QtWidgets/QWidget>
#include "ui_ConfigPage.h"
#include <QJsonArray>
#include <QObject>
#include <QSettings>
#include <QVariantMap>
#include <QChart>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTableWidgetItem>
#include <QMessageBox>
#include <QDir>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QComboBox>
#include <QCoreApplication>
#include <QDebug>
#include <QLineEdit>
#include <QHeaderView>
#include <QFileDialog>

QT_BEGIN_NAMESPACE
namespace Ui { class ConfigPageClass; };
QT_END_NAMESPACE

class ConfigPage : public QWidget
{
    Q_OBJECT

public:
    ConfigPage(QWidget* parent = nullptr);
    ~ConfigPage();

    //供主界面设置采集状态，禁止在采集中修改配置
    void setAcquisitionRunning(bool isRunning);

signals:
	//发出工位数量变化信号，供 AcquisitionPage 接收并更新表格列数
    void stationCountChanged(int count);

public slots:
	//更新本页面的工位数量下拉框和表格列数
	void updateStationCount(int count);

private slots:
	//加载当前工位配置 (局部加载)
    void on_presentStation_currentIndexChanged(int index);

	//保存当前工位配置 (局部保存)
    void on_savePresentConfigBtn_clicked();

	//保存全局配置 (全局保存)
    void on_globalSaveBtn_clicked();

    //恢复当前工位默认配置 (从 JSON 重新加载)
    void on_resumeConfigBtn_clicked();

    //选择新的配置文件 (顶栏 ToolButton)
    void on_toolButton_2_clicked();
private:
	//初始化界面
    void initUI();
    QString loadQss();

    //通用表格初始化函数
    void initTableWidget(QTableWidget* table, const QString& jsonPath, QJsonArray& colCache);

    //从 INI 读取参数配置
    void loadTableFromIni(QTableWidget* table, const QJsonArray& colCache, const QString& section);
    void saveTableToIni(QTableWidget* table, const QJsonArray& colCache, const QString& section);

	//加载指定工位的配置数据到界面
    void loadStationData(int index);

    //各个表格的列结构缓存
    QJsonArray m_colsDataMapping; //数据点映射
    QJsonArray m_colsCommConfig;  //通信设置
    QJsonArray m_colsAlarmLimit;  //报警阈值

    Ui::ConfigPageClass* ui;
    bool m_isAcquisitionRunning = false; //采集状态标志位
};

