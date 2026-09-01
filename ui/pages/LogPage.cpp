#include "LogPage.h"

LogPage::LogPage(QWidget* parent)
    : QWidget(parent)
    , ui(new Ui::LogPageClass())
{
    ui->setupUi(this);

    initUI();
}

LogPage::~LogPage()
{
    delete ui;
}

void LogPage::initUI()
{
    QString qss = loadLog();
    this->setStyleSheet(qss);
}

QString LogPage::loadLog()
{
    QFile file(":/Resource/qss/LogPage.css");

    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "文件打开失败";
    }

    return file.readAll();
}

