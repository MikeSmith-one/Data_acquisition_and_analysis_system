#pragma once

#include <QtWidgets/QWidget>
#include "ui_LogPage.h"
#include <QChart>
#include <QFile>

QT_BEGIN_NAMESPACE
namespace Ui { class LogPageClass; };
QT_END_NAMESPACE

class LogPage : public QWidget
{
    Q_OBJECT

public:
    LogPage(QWidget* parent = nullptr);
    ~LogPage();

private:
	//初始化界面
    void initUI();
    QString loadLog();

    Ui::LogPageClass* ui;
};

