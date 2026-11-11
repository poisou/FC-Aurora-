// 对应界面：wid_stack.ui。
// 本文件负责导航、设备状态及内容页面切换。
#include "wid_stack.h"

#include "ui_wid_stack.h"
#include "wid_customdatasource.h"
#include "wid_datasource.h"
#include "wid_review.h"

#include <QColor>
#include <QComboBox>
#include <QDateTime>
#include <QIcon>
#include <QIntValidator>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPainter>
#include <QPixmap>
#include <QPushButton>
#include <QStackedWidget>
#include <QStyle>
#include <QTimer>
#include <QTreeWidget>
#include <QTreeWidgetItem>

namespace {

// 生成设备或接口状态圆点。
// @param color 圆点填充颜色。
// @return 包含 12×12 圆点的图标。
QIcon makeStatusDot(const QColor &color)
{
    QPixmap pixmap(12, 12);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(Qt::NoPen);
    painter.setBrush(color);
    painter.drawEllipse(2, 2, 8, 8);
    return QIcon(pixmap);
}

// 设置接口状态标签的文本、圆点和动态样式。
// @param label 需要更新的接口状态标签。
// @param connected true 显示绿色在线状态，false 显示灰色离线状态。
void setInterfaceLabelState(QLabel *label, bool connected)
{
    if (label == nullptr) {
        return;
    }

    label->setText(connected ? QStringLiteral("在线") : QStringLiteral("离线"));
    label->setPixmap(
        makeStatusDot(connected ? QColor(0x35C86A) : QColor(0xAEB8C3)).pixmap(10, 10));
    label->setProperty("connected", connected);
    label->style()->unpolish(label);
    label->style()->polish(label);
    label->update();
}

} // namespace

wid_stack::wid_stack(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::wid_stack)
    , m_reviewPage(new WidReview(this))
    , m_dataSourceWidget(new wid_datasource(this))
    , m_customDataSourceWidget(new wid_customdatasource(this))
{
    ui->setupUi(this);

    // 绑定 Designer 中创建的固定控件，后续逻辑只操作这些指针。
    m_contentStack = ui->contentStack;
    m_deviceTree = ui->deviceTree;
    m_deviceNavButton = ui->deviceNavButton;
    m_dataSourceNavButton = ui->dataSourceNavButton;
    m_dataReviewNavButton = ui->dataReviewNavButton;
    m_clockLabel = ui->clockLabel;
    m_openDeviceButton = ui->openDeviceButton;
    m_loginAuthButton = ui->loginAuthButton;
    m_deviceNameEdit = ui->deviceNameEdit;
    m_deviceTypeEdit = ui->deviceTypeEdit;
    m_deviceStatusEdit = ui->deviceStatusEdit;
    m_fcInterfaceStatusLabel = ui->fcInterfaceStatusLabel;
    m_auroraInterfaceStatusLabel = ui->auroraInterfaceStatusLabel;
    m_ethernetInterfaceStatusLabel = ui->ethernetInterfaceStatusLabel;

    // 页面对象名用于应用现有 QSS。
    // 修改 QSS 选择器时，需要与这里设置的 objectName 保持一致。
    ui->devicePage->setObjectName(QStringLiteral("deviceManagementPage"));
    ui->dataSourcePage->setObjectName(QStringLiteral("dataSourceManagementPage"));

    // 设备信息字段统一使用 deviceField 样式。
    m_deviceNameEdit->setObjectName(QStringLiteral("deviceField"));
    m_deviceTypeEdit->setObjectName(QStringLiteral("deviceField"));
    m_deviceStatusEdit->setObjectName(QStringLiteral("deviceField"));
    m_deviceNameEdit->setMinimumWidth(220);

    // 设备操作按钮。
    m_openDeviceButton->setObjectName(QStringLiteral("openDeviceButton"));
    m_openDeviceButton->setCursor(Qt::PointingHandCursor);
    m_loginAuthButton->setObjectName(QStringLiteral("loginAuthButton"));
    m_loginAuthButton->setCursor(Qt::PointingHandCursor);
    m_loginAuthButton->hide();
    m_loginAuthButton->setEnabled(false);

    // 运行时和接口卡片标题。
    ui->runtimeTitle->setObjectName(QStringLiteral("statusCardTitle"));
    ui->interfaceTitle->setObjectName(QStringLiteral("statusCardTitle"));

    // 运行状态字段。
    const QList<QLabel *> metricNameLabels = {
        ui->temperatureNameLabel, ui->storageNameLabel, ui->interfaceCountNameLabel
    };
    for (QLabel *label : metricNameLabels) {
        label->setObjectName(QStringLiteral("metricNameLabel"));
    }
    ui->temperatureValueLabel->setObjectName(QStringLiteral("metricGreenValue"));
    ui->storageValueLabel->setObjectName(QStringLiteral("metricDarkValue"));
    ui->interfaceCountValueLabel->setObjectName(QStringLiteral("metricDarkValue"));

    // 接口名称和参数标签。
    const QList<QLabel *> interfaceNameLabels = {
        ui->fcInterfaceNameLabel,
        ui->auroraInterfaceNameLabel,
        ui->ethernetInterfaceNameLabel
    };
    for (QLabel *label : interfaceNameLabels) {
        label->setObjectName(QStringLiteral("interfaceNameLabel"));
    }

    const QList<QLabel *> interfaceMetaLabels = {
        ui->fcRateLabel,
        ui->fcSyncLabel,
        ui->auroraRateLabel,
        ui->auroraSyncLabel,
        ui->ipLabel,
        ui->portLabel
    };
    for (QLabel *label : interfaceMetaLabels) {
        label->setObjectName(QStringLiteral("interfaceMetaLabel"));
    }

    const QList<QLabel *> interfaceStateLabels = {
        ui->fcInterfaceStatusLabel,
        ui->auroraInterfaceStatusLabel,
        ui->ethernetInterfaceStatusLabel
    };
    for (QLabel *label : interfaceStateLabels) {
        label->setObjectName(QStringLiteral("interfaceStateLabel"));
        setInterfaceLabelState(label, false);
    }

    const QList<QComboBox *> interfaceCombos = {
        ui->fcRateCombo, ui->fcSyncCombo, ui->auroraRateCombo, ui->auroraSyncCombo
    };
    for (QComboBox *combo : interfaceCombos) {
        combo->setObjectName(QStringLiteral("interfaceCombo"));
        combo->setFixedSize(96, 30);
    }

    ui->ipEdit->setObjectName(QStringLiteral("interfaceNetworkEdit"));
    ui->portEdit->setObjectName(QStringLiteral("interfaceNetworkEdit"));
    ui->portEdit->setValidator(new QIntValidator(1, 65535, ui->portEdit));

    // 自检区域样式和圆点。
    ui->selfTestButton->setCursor(Qt::PointingHandCursor);
    ui->selfTestTitle->setObjectName(QStringLiteral("selfTestTitle"));
    ui->fcSelfTestLabel->setObjectName(QStringLiteral("selfTestItemLabel"));
    ui->auroraSelfTestLabel->setObjectName(QStringLiteral("selfTestItemLabel"));
    ui->selfTestResultLabel->setObjectName(QStringLiteral("selfTestResultLabel"));
    ui->fcSelfTestDotLabel->setPixmap(
        makeStatusDot(QColor(0x35C8A1)).pixmap(10, 10));
    ui->auroraSelfTestDotLabel->setPixmap(
        makeStatusDot(QColor(0x35C8A1)).pixmap(10, 10));

    // 顶部首页按钮和底部状态文字。
    ui->homeButton->setIcon(style()->standardIcon(QStyle::SP_DirHomeIcon));
    ui->homeButton->setIconSize(QSize(15, 15));
    ui->clockLabel->setObjectName(QStringLiteral("statusBarText"));
    ui->companyLabel->setObjectName(QStringLiteral("statusBarText"));
    ui->clockLabel->setText(
        QStringLiteral("时间: %1")
            .arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"))));

    // WidReview 作为普通 QWidget 嵌入 reviewPage，不创建独立窗口。
    m_reviewPage->setWindowFlags(Qt::Widget);
    ui->reviewLayout->addWidget(m_reviewPage);
    ui->dataSourceLayout->addWidget(m_dataSourceWidget);
    m_contentStack->addWidget(m_customDataSourceWidget);

    // “自定义数据”从数据源管理页进入独立的自定义数据源编辑页；
    // 编辑页点击“返回”后重新显示原数据源管理页。
    connect(m_dataSourceWidget,
            &wid_datasource::customDataSourceRequested,
            this,
            [this]() {
                m_contentStack->setCurrentWidget(m_customDataSourceWidget);
                setActiveNav(m_dataSourceNavButton);
            });
    connect(m_customDataSourceWidget,
            &wid_customdatasource::backRequested,
            this,
            &wid_stack::showDataSourcePage);
    connect(m_customDataSourceWidget,
            &wid_customdatasource::saveRequested,
            this,
            [this](const QString &name,
                   const QString &busType,
                   const QString &filePath,
                   const QDateTime &createdAt) {
                if (!m_dataSourceWidget->addGeneratedDataSource(
                        name, busType, filePath, createdAt)) {
                    return;
                }
                QMessageBox::information(
                    this,
                    QStringLiteral("保存完成"),
                    QStringLiteral("数据源已保存到 MYDATA 和数据库。"));
                showDataSourcePage();
            });

    initializeDeviceTree();

    connect(m_deviceNavButton,
            &QPushButton::clicked,
            this,
            &wid_stack::showDeviceManagementPage);
    connect(m_dataSourceNavButton,
            &QPushButton::clicked,
            this,
            &wid_stack::showDataSourcePage);
    connect(m_dataReviewNavButton,
            &QPushButton::clicked,
            this,
            &wid_stack::showReviewPage);
    connect(m_openDeviceButton,
            &QPushButton::clicked,
            this,
            &wid_stack::handleOpenDevice);
    connect(m_deviceTree,
            &QTreeWidget::currentItemChanged,
            this,
            &wid_stack::handleDeviceSelection);

    QTreeWidgetItem *defaultItem = m_deviceItems.value(QStringLiteral("Aurora设备"), nullptr);
    if (defaultItem != nullptr) {
        m_deviceTree->setCurrentItem(defaultItem);
    }
    m_contentStack->setCurrentWidget(ui->devicePage);
}

wid_stack::~wid_stack()
{
    delete ui;
}

bool wid_stack::loadFile(const QString &filePath)
{
    const bool accepted = m_reviewPage->loadFile(filePath);
    if (accepted) {
        m_contentStack->setCurrentWidget(ui->reviewPage);
        setActiveNav(m_dataReviewNavButton);
    }
    return accepted;
}

void wid_stack::initializeDeviceTree()
{
    m_deviceTree->clear();
    m_deviceItems.clear();

    auto *rootItem = new QTreeWidgetItem(m_deviceTree, {QStringLiteral("所有设备")});
    rootItem->setIcon(0, style()->standardIcon(QStyle::SP_ComputerIcon));

    auto *fcItem = new QTreeWidgetItem(rootItem, {QStringLiteral("FC设备")});
    fcItem->setIcon(0, makeStatusDot(QColor(0xAEB8C3)));
    // UserRole 保存设备显示名称，UserRole+1 保存设备类型。
    // 后续接口状态匹配、登录按钮显示都依赖这两个赋值。
    fcItem->setData(0, Qt::UserRole, QStringLiteral("FC设备"));
    fcItem->setData(0, Qt::UserRole + 1, QStringLiteral("FC"));
    m_deviceItems.insert(QStringLiteral("FC设备"), fcItem);

    auto *auroraItem = new QTreeWidgetItem(rootItem, {QStringLiteral("Aurora设备")});
    auroraItem->setIcon(0, makeStatusDot(QColor(0xAEB8C3)));
    // Aurora 设备类型固定为 "Aurora"，用于接口在线状态匹配。
    auroraItem->setData(0, Qt::UserRole, QStringLiteral("Aurora设备"));
    auroraItem->setData(0, Qt::UserRole + 1, QStringLiteral("Aurora"));
    m_deviceItems.insert(QStringLiteral("Aurora设备"), auroraItem);

    auto *ethernetItem = new QTreeWidgetItem(rootItem, {QStringLiteral("以太网设备")});
    ethernetItem->setIcon(0, makeStatusDot(QColor(0xAEB8C3)));
    // 界面显示“以太网设备”，设备类型保存为“以太网”。
    // 如后续需要数据库字段 TCP，请只在数据库映射层转换，不要直接改显示名称。
    ethernetItem->setData(0, Qt::UserRole, QStringLiteral("以太网设备"));
    ethernetItem->setData(0, Qt::UserRole + 1, QStringLiteral("以太网"));
    m_deviceItems.insert(QStringLiteral("以太网设备"), ethernetItem);

    rootItem->setExpanded(true);
}

void wid_stack::handleDeviceSelection(QTreeWidgetItem *current, QTreeWidgetItem *previous)
{
    Q_UNUSED(previous)

    if (current == nullptr || current->parent() == nullptr
        || m_deviceNameEdit == nullptr || m_deviceTypeEdit == nullptr
        || m_deviceStatusEdit == nullptr) {
        return;
    }

    const QString name = current->data(0, Qt::UserRole).toString();
    const QString type = current->data(0, Qt::UserRole + 1).toString();
    if (name.isEmpty() || type.isEmpty()) {
        return;
    }

    m_currentDeviceName = name;
    m_currentDeviceType = type;
    // 右侧设备信息由树节点中的 UserRole 数据赋值。
    m_deviceNameEdit->setText(name);
    m_deviceTypeEdit->setText(type);

    QTimer *statusTimer = m_deviceStatusTimers.value(name, nullptr);
    const bool connected = statusTimer != nullptr && statusTimer->isActive();
    setDeviceStatusText(connected);
    updateDeviceConnectionVisual(name, connected);

    m_openDeviceButton->setText(
        connected ? QStringLiteral("关闭设备") : QStringLiteral("打开设备"));

    const bool isFcDevice = type == QStringLiteral("FC");
    m_loginAuthButton->setVisible(isFcDevice);
    m_loginAuthButton->setEnabled(isFcDevice && connected);
}

void wid_stack::handleOpenDevice()
{
    if (m_deviceStatusEdit == nullptr || m_currentDeviceName.isEmpty()
        || m_currentDeviceType.isEmpty()) {
        return;
    }

    QTimer *statusTimer = m_deviceStatusTimers.value(m_currentDeviceName, nullptr);
    if (statusTimer == nullptr) {
        statusTimer = new QTimer(this);
        statusTimer->setInterval(1000);
        const QString deviceName = m_currentDeviceName;
        connect(statusTimer, &QTimer::timeout, this, [this, deviceName]() {
            handleDeviceStatusTick(deviceName);
        });
        m_deviceStatusTimers.insert(m_currentDeviceName, statusTimer);
    }

    if (statusTimer->isActive()) {
        statusTimer->stop();
        setDeviceStatusText(false);
        updateDeviceConnectionVisual(m_currentDeviceName, false);
        m_openDeviceButton->setText(QStringLiteral("打开设备"));
        m_loginAuthButton->setEnabled(false);
    } else {
        statusTimer->start();
        setDeviceStatusText(true);
        updateDeviceConnectionVisual(m_currentDeviceName, true);
        m_openDeviceButton->setText(QStringLiteral("关闭设备"));
        if (m_currentDeviceType == QStringLiteral("FC")) {
            m_loginAuthButton->setEnabled(true);
        }
    }
}

void wid_stack::handleDeviceStatusTick(const QString &deviceName)
{
    QTimer *statusTimer = m_deviceStatusTimers.value(deviceName, nullptr);
    if (statusTimer == nullptr || !statusTimer->isActive()) {
        return;
    }

    updateDeviceConnectionVisual(deviceName, true);
    if (deviceName == m_currentDeviceName) {
        setDeviceStatusText(true);
    }
}

void wid_stack::setDeviceStatusText(bool connected)
{
    if (m_deviceStatusEdit == nullptr) {
        return;
    }

    m_deviceStatusEdit->setText(
        connected ? QStringLiteral("已连接") : QStringLiteral("离线"));
    m_deviceStatusEdit->setProperty("connected", connected);
    m_deviceStatusEdit->style()->unpolish(m_deviceStatusEdit);
    m_deviceStatusEdit->style()->polish(m_deviceStatusEdit);
    m_deviceStatusEdit->update();
}

void wid_stack::updateDeviceConnectionVisual(const QString &deviceName, bool connected)
{
    QTreeWidgetItem *item = m_deviceItems.value(deviceName, nullptr);
    if (item == nullptr) {
        return;
    }

    item->setIcon(0, makeStatusDot(
        connected ? QColor(0x35C86A) : QColor(0xAEB8C3)));
    updateInterfaceConnectionVisual(
        item->data(0, Qt::UserRole + 1).toString(), connected);
}

void wid_stack::updateInterfaceConnectionVisual(const QString &deviceType, bool connected)
{
    if (deviceType == QStringLiteral("FC")) {
        setInterfaceLabelState(m_fcInterfaceStatusLabel, connected);
    } else if (deviceType == QStringLiteral("Aurora")) {
        setInterfaceLabelState(m_auroraInterfaceStatusLabel, connected);
    } else if (deviceType == QStringLiteral("以太网")) {
        setInterfaceLabelState(m_ethernetInterfaceStatusLabel, connected);
    }
}

void wid_stack::showDeviceManagementPage()
{
    m_contentStack->setCurrentWidget(ui->devicePage);
    setActiveNav(m_deviceNavButton);
}

void wid_stack::showDataSourcePage()
{
    m_contentStack->setCurrentWidget(ui->dataSourcePage);
    setActiveNav(m_dataSourceNavButton);
}

void wid_stack::showReviewPage()
{
    m_contentStack->setCurrentWidget(ui->reviewPage);
    setActiveNav(m_dataReviewNavButton);
}

void wid_stack::setActiveNav(QPushButton *activeButton)
{
    const QList<QPushButton *> buttons = {
        m_deviceNavButton, m_dataSourceNavButton, m_dataReviewNavButton
    };

    for (QPushButton *button : buttons) {
        if (button == nullptr) {
            continue;
        }
        button->setProperty("activeNav", button == activeButton);
        button->style()->unpolish(button);
        button->style()->polish(button);
        button->update();
    }
}
