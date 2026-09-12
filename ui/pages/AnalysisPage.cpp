#include "AnalysisPage.h"
#include "./pages/ConfigManager.h"

AnalysisPage::AnalysisPage(QWidget* parent)
    : QWidget(parent)
    , ui(new Ui::AnalysisPageClass())
{
    ui->setupUi(this);

    initUI();

    initDateFilters();

    //连接“提取模式”下拉框的信号槽实现动态更新右侧单位 Label 和输入框占位符
    connect(ui->comboBoxMode, QOverload<int>::of(&QComboBox::currentIndexChanged),
        this, &AnalysisPage::onAnalysisExtractionModeChanged);
}

AnalysisPage::~AnalysisPage()
{
    delete ui;
}

void AnalysisPage::initUI()
{
    //加载数据到“提取模式”下拉框
    loadAnalysisExtractionModesFromJson();

	//设置qss样式表
    QString qss = loadQss();
    this->setStyleSheet(qss);
}

//从json加载qss样式表
QString AnalysisPage::loadQss()
{
    QFile file(":/Resource/qss/AnalysisPage.css");
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        qWarning() << "文件打开失败";
        return"";
    }

    return file.readAll();
}

void AnalysisPage::initDateFilters()
{
    //开启日历弹窗
    ui->dateEditStart->setCalendarPopup(true);
    ui->dateEditEnd->setCalendarPopup(true);

    //设置标准的日期显示格式
    ui->dateEditStart->setDisplayFormat("yyyy-MM-dd");
    ui->dateEditEnd->setDisplayFormat("yyyy-MM-dd");

    //设置默认显示范围（默认展示最近一星期的数据）
    ui->dateEditStart->setDate(QDate::currentDate().addDays(-7));
    ui->dateEditEnd->setDate(QDate::currentDate());

    //绑定信号：当日期改变时，自动重新查询数据库更新 ListView 列表
    connect(ui->dateEditStart, &QDateEdit::dateChanged, this, &AnalysisPage::onDateRangeChanged);
    connect(ui->dateEditEnd, &QDateEdit::dateChanged, this, &AnalysisPage::onDateRangeChanged);
}

//日期切换时的数据库筛选槽函数
void AnalysisPage::onDateRangeChanged()
{
    QString startDate = ui->dateEditStart->date().toString("yyyy-MM-dd");
    QString endDate = ui->dateEditEnd->date().toString("yyyy-MM-dd");

    // SQL 示例: 查询该时间段内的实验记录并刷新左侧 ListView
    // SELECT exp_name FROM experiment_records WHERE test_date BETWEEN :start AND :end
}


//加载数据到“提取模式”下拉框，并绑定元数据
//从json加载数据到“提取模式”下拉框，并绑定元数据
void AnalysisPage::loadAnalysisExtractionModesFromJson()
{
    QFile file(":/Resource/uiData/analysisExtractionModes.json");
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "无法打开 analysisExtractionModes.json";
        return;
    }
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    file.close();

    if (!doc.isObject()) {
        qWarning() << "analysisExtractionModes.json 格式不正确";
        return;
    }

    ui->comboBoxMode->clear();
    QJsonArray modes = doc.object()["analysis_modes"].toArray();

    for (const QJsonValue& val : modes) {
        QJsonObject obj = val.toObject();

        //绑定元数据到ItemData
        QVariantMap meta;
        meta["id"] = obj["id"].toString();
        meta["unit"] = obj["unit"].toString();
        meta["default_val"] = obj["default_val"].toString();

        ui->comboBoxMode->addItem(obj["text"].toString(), meta);
    }
}

//处理“提取模式”下拉框的索引变化事件，实现动态更新右侧单位 Label 和输入框占位符
void AnalysisPage::onAnalysisExtractionModeChanged(int index)
{
    if (index < 0) return;

    //获取当前选中项的 JSON 元数据
    QVariantMap metaData = ui->comboBoxMode->itemData(index).toMap();

    //动态更新右侧的单位 Label 和输入框占位符
    ui->labelUnit->setText(metaData["unit"].toString());
    ui->lineEditTime->setPlaceholderText(metaData["placeholder"].toString());
}