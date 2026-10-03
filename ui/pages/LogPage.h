#pragma once

#include <QtWidgets/QWidget>
#include "ui_LogPage.h"
#include <QChart>
#include <QFile>
#include <QDateTime>
#include <QList>
#include <QTextStream>
#include <QFileDialog>
#include <QMessageBox>
#include <QDir>
#include <QCoreApplication>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonParseError>
#include "ui/pages/ConfigManager.h"

QT_BEGIN_NAMESPACE
namespace Ui { class LogPageClass; };
QT_END_NAMESPACE

// ============================================
// 单条日志的数据结构
// ============================================
struct LogItem {
    QDateTime timestamp; //发生时间
    QString level;       //日志级别 (INFO, WARN, ERROR)
    QString module;      //触发模块
    QString message;     //详细描述
};

class LogPage : public QWidget
{
    Q_OBJECT

public:
    LogPage(QWidget* parent = nullptr);
    ~LogPage();

signals:
    void logMessage(const QString& msg);  //日志信息信号

public slots:
    //接收全局日志的槽函数（供MainWindow调用）
    void receiveLog(const QString& level, const QString& module, const QString& msg);

private slots:
    //顶部工具栏按钮槽函数
    void on_toolButton_clicked();       //选择日志文件路径
    void on_importBtn_clicked();        //导入数据
    void on_queryBtn_clicked();         //查询/筛选
    void on_resetBtn_clicked();         //重置筛选条件
    void on_exportLogBtn_clicked();     //导出当前显示到日志
    void on_cleanShowBtn_clicked();     //清空显示并回复Live模式

    //点击表格某一行时，下方文本框显示完整详情
    void on_logShowList_itemSelectionChanged();

private:
	//初始化界面
    void initUI();
    QString loadLog();
    void loadModuleFiltersFromJson();//初始化左侧“来源模块筛选”

    //辅助函数
    void appendLogToUI(const LogItem& item); //将单条日志渲染到表格
    void updateStatistics();                // 更新左侧的统计数量
    void writeLogToFile(const LogItem& item);// 将日志写入当天的 .log 文件

private:
    //核心状态与缓存
    QList<LogItem> m_logItems;      //内存中的日志列表
    bool m_isLiveMode = true;       //当前是否处于实时监控模式

    Ui::LogPageClass* ui;
};

