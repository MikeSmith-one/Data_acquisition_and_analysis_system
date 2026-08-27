#include "AnalysisPage.h"
#include <QChart>

AnalysisPage::AnalysisPage(QWidget* parent)
    : QWidget(parent)
    , ui(new Ui::AnalysisPageClass())
{
    ui->setupUi(this);
}

AnalysisPage::~AnalysisPage()
{
    delete ui;
}

