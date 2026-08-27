#include "AcquisitionPage.h"
#include <QChart>

AcquisitionPage::AcquisitionPage(QWidget* parent)
    : QWidget(parent)
    , ui(new Ui::AcquisitionPageClass())
{
    ui->setupUi(this);
}

AcquisitionPage::~AcquisitionPage()
{
    delete ui;
}

