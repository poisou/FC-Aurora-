#include "pluginworkspace.h"

#include "ui_plugin_wid_mainwindow.h"
#include "ui_plugin_wid_stack.h"

#include "sceneparallelwidget.h"
#include "wid_datasource.h"
#include "wid_customdatasource.h"
#include "wid_review.h"
#include "protocolrulewidget.h"

#include <QMessageBox>
#include <QStackedWidget>

namespace {

// 插件设备管理页预览。该页面直接由插件 wid_stack.ui 生成。
class DevicePagePreview : public QWidget
{
public:
    explicit DevicePagePreview(QWidget *parent = nullptr)
        : QWidget(parent)
        , ui(new Ui::plugin_wid_stack)
    {
        ui->setupUi(this);
        ui->stackedWidget->setCurrentIndex(0);
    }

    ~DevicePagePreview() override
    {
        delete ui;
    }

private:
    Ui::plugin_wid_stack *ui;
};

} // namespace

PluginWorkspace::PluginWorkspace(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::wid_mainwindow)
{
    ui->setupUi(this);

    // 设备管理：插件 wid_stack.ui 的第 0 页。
    auto *devicePage = new DevicePagePreview(ui->tab);
    ui->glay_dev->addWidget(devicePage);

    // 协议规则：功能完整的规则管理页面（新建/编辑/删除）。
    m_protocolRuleWidget = new ProtocolRuleWidget(ui->tab_2);
    ui->glay_rul->addWidget(m_protocolRuleWidget);

    // 场景并行使用插件 UIWidget 中的界面。
    auto *scenePage = new SceneParallelWidget(ui->tab_7);
    ui->glay_scp->addWidget(scenePage);

    // 数据源管理页签使用堆叠容器，在数据列表和自定义数据页面间切换。
    auto *dataSourceStack = new QStackedWidget(ui->tab_5);
    m_dataSourceWidget = new wid_datasource(ui->tab_5);
    m_customDataSourceWidget = new wid_customdatasource(ui->tab_5);
    dataSourceStack->addWidget(m_dataSourceWidget);
    dataSourceStack->addWidget(m_customDataSourceWidget);
    ui->glay_datacontrol->addWidget(dataSourceStack);

    connect(m_dataSourceWidget,
            &wid_datasource::customDataSourceRequested,
            this,
            [dataSourceStack]() {
                dataSourceStack->setCurrentWidget(
                    dataSourceStack->widget(1));
            });
    connect(m_customDataSourceWidget,
            &wid_customdatasource::backRequested,
            this,
            [dataSourceStack]() {
                dataSourceStack->setCurrentIndex(0);
            });
    connect(m_customDataSourceWidget,
            &wid_customdatasource::saveRequested,
            this,
            [this](const QString &name,
                   const QString &busType,
                   const QString &filePath,
                   const QDateTime &createdAt) {
                if (m_dataSourceWidget->addGeneratedDataSource(
                        name, busType, filePath, createdAt)) {
                    QMessageBox::information(
                        this,
                        QStringLiteral("保存完成"),
                        QStringLiteral("自定义数据源已保存。"));
                }
            });

    m_reviewWidget = new WidReview(ui->tab_6);
    m_reviewWidget->setWindowFlags(Qt::Widget);
    ui->glay_reviewset->addWidget(m_reviewWidget);

    ui->tabWidget->setCurrentIndex(0);
    ui->label_2->hide();
    ui->label_3->hide();
    ui->pushButton->hide();
}

PluginWorkspace::~PluginWorkspace()
{
    delete ui;
}

bool PluginWorkspace::loadFile(const QString &filePath)
{
    if (m_reviewWidget == nullptr || !m_reviewWidget->loadFile(filePath)) {
        return false;
    }

    ui->tabWidget->setCurrentWidget(ui->tab_6);
    return true;
}
