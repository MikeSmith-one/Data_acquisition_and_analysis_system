#include "LogPage.h"
#include <QChart>

LogPage::LogPage(QWidget* parent)
    : QWidget(parent)
    , ui(new Ui::LogPageClass())
{
    ui->setupUi(this);
}

LogPage::~LogPage()
{
    delete ui;
}

