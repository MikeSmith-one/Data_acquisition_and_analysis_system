#include "ConfigPage.h"
#include <QChart>

ConfigPage::ConfigPage(QWidget* parent)
    : QWidget(parent)
    , ui(new Ui::ConfigPageClass())
{
    ui->setupUi(this);
}

ConfigPage::~ConfigPage()
{
    delete ui;
}

