#include "sceneparallelwidget.h"

#include "ui_SceneParallelWidget.h"
#include "binformat.h"
#include "devicework.h"

#include <QCheckBox>
#include <QColor>
#include <QComboBox>
#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHeaderView>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPainter>
#include <QPixmap>
#include <QProgressBar>
#include <QSettings>
#include <QSlider>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QSpinBox>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QTimer>

#include <limits>

namespace {

// 每条总线方向的消息容器容量，超出后丢弃最旧记录。
constexpr int kMessageContainerSize = 200;

// 表格每次最多显示容器最前面的记录条数。
constexpr int kMessageVisibleRows = 50;

// 三条总线的默认单文件上限，直接取自各自的宏（默认均为 1 GB）。
// 成员 m_recvSaveMaxBytes 的初值也使用同一组宏，运行时可通过
// setReceiveSaveMaxBytes() 覆盖，传 0 时回到这里的宏默认值。
const qint64 kRecvSaveMaxBytes[3] = {
    FC_RECV_SAVE_MAX_BYTES,
    AURORA_RECV_SAVE_MAX_BYTES,
    TCP_RECV_SAVE_MAX_BYTES
};

// 落盘文件名中的总线类型后缀，索引与 m_recvSaveFiles 一致。
const char *const kRecvSaveBusNames[3] = { "FC", "Aurora", "TCP" };

// 三条总线各自的落盘开关（编译期常量，索引与 m_recvSaveFiles 一致）。
// [0]=FC，[1]=Aurora，[2]=以太网。
// 对应宏为 0 时，该总线的接收数据只刷新界面表格、不写文件，
// 其余总线不受影响；编译期即可裁掉整段写入逻辑。
const bool kRecvSaveSwitches[3] = {
#if FC_RECV_SAVE_ENABLED
    true,
#else
    false,
#endif
#if AURORA_RECV_SAVE_ENABLED
    true,
#else
    false,
#endif
#if TCP_RECV_SAVE_ENABLED
    true,
#else
    false,
#endif
};

// “规则”下拉框的三总线开关表（编译期常量，索引与设备总线的对应关系一致）。
// [0]=FC，[1]=Aurora，[2]=以太网。
// 对应宏为 0 时，该总线设备隐藏“规则”标签与下拉框，其它总线不受影响。
const bool kRuleComboSwitches[3] = {
#if FC_SHOW_RULE_COMBO
    true,
#else
    false,
#endif
#if AURORA_SHOW_RULE_COMBO
    true,
#else
    false,
#endif
#if TCP_SHOW_RULE_COMBO
    true,
#else
    false,
#endif
};

// “周期时间”控件的三总线开关表（编译期常量）。
// [0]=FC，[1]=Aurora，[2]=以太网。
// 对应宏为 0 时，该总线设备隐藏“周期时间:”标签与输入框，其它总线不受影响。
const bool kPeriodicTimeSwitches[3] = {
#if FC_SHOW_PERIODIC_TIME
    true,
#else
    false,
#endif
#if AURORA_SHOW_PERIODIC_TIME
    true,
#else
    false,
#endif
#if TCP_SHOW_PERIODIC_TIME
    true,
#else
    false,
#endif
};

// 将毫秒格式化为 mm:ss.mmm，用于时间模式进度显示。
// @param milliseconds 待格式化的毫秒数，负值按 0 处理。
QString formatDuration(qint64 milliseconds)
{
    const qint64 ms = qMax<qint64>(0, milliseconds);
    const qint64 totalSeconds = ms / 1000;
    const qint64 minutes = totalSeconds / 60;
    const qint64 seconds = totalSeconds % 60;
    const qint64 millis = ms % 1000;

    return QStringLiteral("%1:%2.%3")
        .arg(minutes, 2, 10, QLatin1Char('0'))
        .arg(seconds, 2, 10, QLatin1Char('0'))
        .arg(millis, 3, 10, QLatin1Char('0'));
}

// 计算进度百分比（0~100）。
// 使用 qint64 避免 value * 100 在 1GB 级总长时溢出 int，
// 并对结果四舍五入，保证滑到最右端时一定是 100%。
// @param current 当前位置（字节或毫秒）。
// @param total 总长度（字节或毫秒），<= 0 时按 0% 处理。
int percentOf(qint64 current, qint64 total)
{
    if (total <= 0) {
        return 0;
    }
    const qint64 clamped = qBound<qint64>(0, current, total);
    return static_cast<int>((clamped * 100 + total / 2) / total);
}

// 创建唯一的 SQLite 连接名，避免与数据源管理、协议规则页面冲突。
QString uniqueConnectionName(const QString &prefix, const QWidget *owner)
{
    return QStringLiteral("%1_%2")
        .arg(prefix)
        .arg(reinterpret_cast<quintptr>(owner), 0, 16);
}

} // namespace

SceneParallelWidget::SceneParallelWidget(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::SceneParallelWidget)
{
    ui->setupUi(this);

    // 统一左右比例：以 FC 设备界面为基准，左侧设备列表固定 320px，
    // 右侧配置区与消息表格占用剩余全部宽度。
    // 左侧宽度已由 .ui 的 minimumSize/maximumSize 固定，这里再固定伸缩因子，
    // 保证切换 FC / Aurora / 以太网设备时左右宽度都不发生变化。
    ui->contentLayout->setStretch(0, 0);
    ui->contentLayout->setStretch(1, 1);

    // 按钮行：启动/暂停/停止/状态之后依次是进度模式下拉框、进度滑块、
    // 百分比标签。滑块占据剩余宽度，其余控件保持自身宽度。
    // 下标对应 .ui 中 confirmRowLayout 的顺序：
    // 0=启动，1=暂停，2=停止，3=状态，4=模式下拉框，5=滑块，6=百分比。
    ui->confirmRowLayout->setStretch(5, 1);
    ui->confirmRowLayout->setStretch(7, 0);

    configureMessageTables();

    // =====================================================================
    // ★ 修改消息表格列名的调用位置 ★
    //
    // 六张消息表（三总线 × 发送/接收）各有独立接口，需要改列名/列数量时
    // 在下面新增调用即可（示例，通常与 configureMessageTables() 中的
    // 默认列名二选一）：
    //
    //   setFcSendTableHeader({"通道号", "时标", "值", "备注"});
    //   setFcReceiveTableHeader({"通道号", "时标", "值", "备注"});
    //   setAuroraSendTableHeader({"通道号", "时标", "值"});
    //   setAuroraReceiveTableHeader({"通道号", "时标", "值"});
    //   setTcpSendTableHeader({"通道号", "时标", "值"});
    //   setTcpReceiveTableHeader({"通道号", "时标", "值"});
    //
    // 只追加一列而不动其它列时，用 addMessageTableColumn()：
    //   addMessageTableColumn(MessageTable::FcReceive, QStringLiteral("备注"));
    // 注意：300ms 自动刷新只写前 3 列，第 4 列及之后的内容需自行填充。
    // =====================================================================

    initializeDeviceList();
    ui->statusLabel->clear();

    // 接收线程启动后，每 300ms 从各容器取前 50 条刷新表格。
    m_messageRefreshTimer = new QTimer(this);
    m_messageRefreshTimer->setInterval(300);
    connect(m_messageRefreshTimer,
            &QTimer::timeout,
            this,
            &SceneParallelWidget::refreshMessageTables);

    connect(ui->deviceList,
            &QListWidget::currentItemChanged,
            this,
            &SceneParallelWidget::handleDeviceSelection);
    connect(ui->startButton,
            &QPushButton::clicked,
            this,
            &SceneParallelWidget::startDevice);
    connect(ui->pauseButton,
            &QPushButton::clicked,
            this,
            &SceneParallelWidget::pauseDevice);
    connect(ui->stopButton,
            &QPushButton::clicked,
            this,
            &SceneParallelWidget::stopDevice);
    connect(ui->periodicCheckBox,
            &QCheckBox::toggled,
            this,
            &SceneParallelWidget::updatePeriodicInputState);

    // 数据源、规则、方向或周期设置一发生变化就立即保存。
    // 这样即使用户没有点击“启动”便切换到其它设备，当前选择也不会丢失。
    connect(ui->dataSourceCombo,
            QOverload<int>::of(&QComboBox::currentIndexChanged),
            this,
            &SceneParallelWidget::saveCurrentDeviceControls);
    connect(ui->dataSourceCombo,
            QOverload<int>::of(&QComboBox::currentIndexChanged),
            this,
            &SceneParallelWidget::updateProgressTotalFromDataSource);
    connect(ui->ruleCombo,
            QOverload<int>::of(&QComboBox::currentIndexChanged),
            this,
            &SceneParallelWidget::saveCurrentDeviceControls);
    connect(ui->sendCheckBox,
            &QCheckBox::toggled,
            this,
            &SceneParallelWidget::saveCurrentDeviceControls);
    connect(ui->receiveCheckBox,
            &QCheckBox::toggled,
            this,
            &SceneParallelWidget::saveCurrentDeviceControls);
    connect(ui->periodicCheckBox,
            &QCheckBox::toggled,
            this,
            &SceneParallelWidget::saveCurrentDeviceControls);
    connect(ui->periodicTimeSpinBox,
            QOverload<int>::of(&QSpinBox::valueChanged),
            this,
            &SceneParallelWidget::saveCurrentDeviceControls);
    connect(ui->bbCreditSpinBox,
            QOverload<int>::of(&QSpinBox::valueChanged),
            this,
            &SceneParallelWidget::saveCurrentDeviceControls);

    connect(ui->sendCheckBox,
            &QCheckBox::toggled,
            this,
            &SceneParallelWidget::updateTcpTargetVisibility);
    // 勾选/取消“发送”时同步刷新 FC 进度控件的显示状态。
    connect(ui->sendCheckBox,
            &QCheckBox::toggled,
            this,
            &SceneParallelWidget::updateFcCreditVisibility);
    connect(ui->peerIpEdit,
            &QLineEdit::textChanged,
            this,
            &SceneParallelWidget::saveCurrentDeviceControls);
    connect(ui->peerPortEdit,
            &QLineEdit::textChanged,
            this,
            &SceneParallelWidget::saveCurrentDeviceControls);

    // 进度条：切换划分方式时重设范围；拖动/赋值时刷新百分比与字节数。
    connect(ui->progressModeCombo,
            QOverload<int>::of(&QComboBox::currentIndexChanged),
            this,
            &SceneParallelWidget::handleProgressModeChanged);
    connect(ui->dataProgressSlider,
            &QSlider::valueChanged,
            this,
            &SceneParallelWidget::updateProgressInfo);

    updatePeriodicInputState(false);
    updateTcpTargetVisibility();
    updateFcCreditVisibility();
    updatePeriodicTimeVisibility();
    applyProgressRange();
    updateSendProgressVisibility();
}

SceneParallelWidget::~SceneParallelWidget()
{
    // 关闭各总线的接收数据文件，确保缓冲区内容落盘。
    for (QFile *&file : m_recvSaveFiles) {
        if (file == nullptr) {
            continue;
        }
        file->flush();
        file->close();
        delete file;
        file = nullptr;
    }

    // 正常关闭程序时再保存一次，覆盖用户最后一次操作。
    saveCurrentDeviceControls();
    delete ui;
}

void SceneParallelWidget::initializeDeviceList()
{
    ui->deviceList->clear();

    // 放大设备列表字体并拉大每行间距。
    // 全局 app.qss 把所有 QWidget 字体压到 12px，控件级样式表优先于全局规则；
    // min-height/padding 控制每行高度，setSpacing 控制行与行之间的空隙。
    // 后续要再调整，改这里的 18px / 36px / 8px / 6 即可。
    ui->deviceList->setStyleSheet(QStringLiteral(
        "QListWidget { font-size: 18px; }"
        "QListWidget::item { min-height: 36px; padding: 8px 4px; }"));
    ui->deviceList->setSpacing(6);

    // FC 两个通道。
    ui->deviceList->addItem(QStringLiteral("FC设备1通道"));
    ui->deviceList->addItem(QStringLiteral("FC设备2通道"));

    // Aurora 八个通道。
    for (int channel = 1; channel <= 8; ++channel) {
        ui->deviceList->addItem(
            QStringLiteral("Aurora设备%1通道").arg(channel));
    }

    // 以太网设备当前作为一个并行设备项。
    ui->deviceList->addItem(QStringLiteral("以太网设备"));

    // 初始状态均为停止，为每个设备项设置红色圆点。
    for (int index = 0; index < ui->deviceList->count(); ++index) {
        updateDeviceListEntry(ui->deviceList->item(index)->text());
    }
}

QString SceneParallelWidget::dataSourceTableForDevice(
    const QString &deviceName) const
{
    if (deviceName.startsWith(QStringLiteral("FC设备"))) {
        return QStringLiteral("table_data_FCsource");
    }
    if (deviceName.startsWith(QStringLiteral("Aurora设备"))) {
        return QStringLiteral("table_data_Aurorasource");
    }
    if (deviceName == QStringLiteral("以太网设备")) {
        return QStringLiteral("table_data_TCPsource");
    }
    return QString();
}

QStringList SceneParallelWidget::loadDataSources(
    const QString &tableName) const
{
    QStringList names;
    if (tableName.isEmpty()) {
        return names;
    }

    const QString databasePath =
        QDir(QCoreApplication::applicationDirPath())
            .filePath(QStringLiteral("qsqlite/WT_FCData.db"));
    if (!QFileInfo::exists(databasePath)) {
        return names;
    }

    const QString connectionName =
        uniqueConnectionName(QStringLiteral("scene_datasource"), this);
    {
        QSqlDatabase database =
            QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connectionName);
        database.setDatabaseName(databasePath);
        if (database.open()) {
            QSqlQuery query(database);
            if (query.exec(
                    QStringLiteral("SELECT name FROM %1 ORDER BY rowid")
                        .arg(tableName))) {
                while (query.next()) {
                    const QString name = query.value(0).toString();
                    if (!name.isEmpty()) {
                        names.append(name);
                    }
                }
            }
        }
        database.close();
    }
    QSqlDatabase::removeDatabase(connectionName);
    return names;
}

QStringList SceneParallelWidget::loadRuleNames() const
{
    QStringList names;

    // 协议规则页面使用 prj/WTBTS/DataFile/WT_FCData.db，
    // 规则表名格式为 TABLE_ICD_<规则名>。
    const QString ruleDatabasePath =
        QDir(QCoreApplication::applicationDirPath())
            .filePath(QStringLiteral("prj/WTBTS/DataFile/WT_FCData.db"));
    if (QFileInfo::exists(ruleDatabasePath)) {
        const QString connectionName =
            uniqueConnectionName(QStringLiteral("scene_rule"), this);
        {
            QSqlDatabase database = QSqlDatabase::addDatabase(
                QStringLiteral("QSQLITE"), connectionName);
            database.setDatabaseName(ruleDatabasePath);
            if (database.open()) {
                QSqlQuery query(database);
                if (query.exec(
                        QStringLiteral(
                            "SELECT name FROM sqlite_master "
                            "WHERE type='table' AND name LIKE 'TABLE_ICD_%'"))) {
                    while (query.next()) {
                        QString name = query.value(0).toString();
                        name.remove(QStringLiteral("TABLE_ICD_"));
                        if (!name.isEmpty()) {
                            names.append(name);
                        }
                    }
                }
            }
            database.close();
        }
        QSqlDatabase::removeDatabase(connectionName);
    }

    // 兼容 ICD JSON 文件目录。
    const QDir icdDirectory(
        QDir(QCoreApplication::applicationDirPath())
            .filePath(QStringLiteral("prj/WTBTS/ICD")));
    const QStringList jsonFiles =
        icdDirectory.entryList({QStringLiteral("*.json")},
                               QDir::Files,
                               QDir::Name);
    for (const QString &fileName : jsonFiles) {
        const QString name = QFileInfo(fileName).completeBaseName();
        if (!names.contains(name)) {
            names.append(name);
        }
    }

    names.removeDuplicates();
    names.sort(Qt::CaseInsensitive);
    return names;
}

void SceneParallelWidget::handleDeviceSelection(
    QListWidgetItem *current,
    QListWidgetItem *previous)
{
    Q_UNUSED(previous)

    // 信号触发时右侧仍显示旧设备内容，先保存旧设备再加载体新设备。
    saveCurrentDeviceControls();

    if (current == nullptr) {
        m_restoringControls = true;
        m_currentDevice.clear();
        updateCurrentDeviceLabel();
        ui->dataSourceCombo->clear();
        ui->ruleCombo->clear();
        ui->sendCheckBox->setChecked(false);
        ui->receiveCheckBox->setChecked(false);
        ui->periodicCheckBox->setChecked(false);
        ui->periodicTimeSpinBox->setValue(1000);
        updatePeriodicInputState(false);
        applyControlButtons();
        updateTcpTargetVisibility();
        updateFcCreditVisibility();
        updatePeriodicTimeVisibility();
        updateRuleVisibility();
        updateReceiveVisibility();
        m_restoringControls = false;
        return;
    }

    m_currentDevice = current->text();
    updateCurrentDeviceLabel();
    refreshDevicePanel(m_currentDevice);
}

void SceneParallelWidget::refreshDevicePanel(const QString &deviceName)
{
    // 恢复控件期间会触发 currentIndexChanged、toggled 和 valueChanged。
    // 使用标志阻止这些中间值被误认为用户修改并覆盖之前的保存结果。
    m_restoringControls = true;

    ui->dataSourceCombo->clear();
    ui->ruleCombo->clear();

    ui->dataSourceCombo->addItem(QStringLiteral("请选择数据源"), QString());
    const QStringList dataSources =
        loadDataSources(dataSourceTableForDevice(deviceName));
    for (const QString &name : dataSources) {
        ui->dataSourceCombo->addItem(name, name);
    }

    ui->ruleCombo->addItem(QStringLiteral("请选择规则"), QString());
    const QStringList rules = loadRuleNames();
    for (const QString &name : rules) {
        ui->ruleCombo->addItem(name, name);
    }
    if (rules.isEmpty()) {
        // 没有导入 ICD 规则时保留一个可选的默认规则，避免页面无法确认。
        ui->ruleCombo->addItem(QStringLiteral("默认规则"),
                               QStringLiteral("默认规则"));
    }

    // 恢复该设备上次选择的数据源、规则、方向和周期配置。
    const DeviceConfig config = storedDeviceConfig(deviceName);
    const int dataSourceIndex = ui->dataSourceCombo->findData(config.dataSource);
    ui->dataSourceCombo->setCurrentIndex(qMax(0, dataSourceIndex));
    const int ruleIndex = ui->ruleCombo->findData(config.rule);
    ui->ruleCombo->setCurrentIndex(qMax(0, ruleIndex));
    ui->sendCheckBox->setChecked(config.sendEnabled);
    // 三条总线都支持接收，Aurora 接收数据写入 Aurora 接收表格。
    ui->receiveCheckBox->setChecked(config.receiveEnabled);
    ui->periodicCheckBox->setChecked(config.periodicEnabled);
    ui->periodicTimeSpinBox->setValue(config.periodMs);
    ui->peerIpEdit->setText(config.peerAddress);
    ui->peerPortEdit->setText(QString::number(config.peerPort));
    ui->bbCreditSpinBox->setValue(config.bbCredit);
    updatePeriodicInputState(config.periodicEnabled);
    updateTcpTargetVisibility();
    updateFcCreditVisibility();
    updatePeriodicTimeVisibility();
    updateRuleVisibility();
    updateReceiveVisibility();
    applyControlButtons();

    // Aurora 不使用规则，状态栏只显示数据源数量，不显示“规则”总数。
    if (deviceName.startsWith(QStringLiteral("Aurora设备"))) {
        ui->statusLabel->setText(
            QStringLiteral("%1：%2 个数据源")
                .arg(deviceName)
                .arg(dataSources.size()));
    } else {
        ui->statusLabel->setText(
            QStringLiteral("%1：%2 个数据源，%3 个规则")
                .arg(deviceName)
                .arg(dataSources.size())
                .arg(rules.size()));
    }

    // 切换当前设备时直接定位到该设备所属的总线页，便于查看已有消息。
    ui->messageTabWidget->setCurrentIndex(
        messageTabIndexForDevice(deviceName));

    // FC 的发送进度条按所选数据源文件总长划分，选中设备/切换数据源时刷新。
    updateProgressTotalFromDataSource();

    m_restoringControls = false;
}

void SceneParallelWidget::updateProgressTotalFromDataSource()
{
    // 只有 FC 设备显示进度条，其它设备直接忽略。
    if (!m_currentDevice.startsWith(QStringLiteral("FC设备"))) {
        return;
    }

    const QString name = ui->dataSourceCombo->currentData().toString();
    const QString sourcePath =
        dataSourceFilePath(dataSourceTableForDevice(m_currentDevice), name);
    setProgressTotalLength(
        sourcePath.isEmpty() ? 0 : QFileInfo(sourcePath).size());
}

void SceneParallelWidget::startDevice()
{
    if (m_currentDevice.isEmpty()) {
        return;
    }

    const QString dataSource =
        ui->dataSourceCombo->currentData().toString();
    const QString rule = ui->ruleCombo->currentData().toString();
    // Aurora 设备不使用协议规则，只校验数据源。
    const bool requireRule =
        !m_currentDevice.startsWith(QStringLiteral("Aurora设备"));
    if (dataSource.isEmpty() || (requireRule && rule.isEmpty())) {
        QMessageBox::warning(
            this,
            QStringLiteral("配置不完整"),
            requireRule ? QStringLiteral("请选择数据源和规则。")
                        : QStringLiteral("请选择数据源。"));
        return;
    }

    // 用户点击启动时再次保存，确保内存与持久化配置都是最新值。
    DeviceConfig config = currentDeviceConfig();
    persistDeviceConfig(m_currentDevice, config);

    // 切换到该设备所属的总线页，便于查看接收到的消息。
    ui->messageTabWidget->setCurrentIndex(
        messageTabIndexForDevice(m_currentDevice));

    ui->statusLabel->setText(
        rule.isEmpty()
            ? QStringLiteral("%1 已启动：%2").arg(m_currentDevice, dataSource)
            : QStringLiteral("%1 已启动：%2 / %3")
                  .arg(m_currentDevice, dataSource, rule));
    emit configurationConfirmed(m_currentDevice,
                                dataSource,
                                rule,
                                config.sendEnabled,
                                config.receiveEnabled,
                                config.periodicEnabled,
                                config.periodMs);

    // ── 发送线程参数设置 ──────────────────────────────────────────────
    // 所有参数都必须在 work->startWork() 之前设置：
    //   1) 所有总线：work->setSourceFile(数据源文件绝对路径);
    //   2) 以太网额外：work->setTcpTarget(对端IP, 对端端口);
    // 若线程已启动需要换参数，先 work->stopWork()，设置后再 startWork()。
    //
    // 独立使用示例（不经过本页面时）：
    //   DeviceWork *send =
    //       new DeviceWork(QStringLiteral("Aurora设备1通道"),
    //                      1, DeviceWork::Send, parentWidget);
    //   send->setSourceFile(QStringLiteral("D:/data/aurora1.bin"));
    //   send->startWork();   // 线程启动后按帧读取并交给 AuroraSendPro::send()
    // ────────────────────────────────────────────────────────────────
    const QList<DeviceWork *> works = deviceWorks(m_currentDevice);
    if (config.sendEnabled) {
        const QString sourcePath =
            dataSourceFilePath(
                dataSourceTableForDevice(m_currentDevice), dataSource);

        // 每次启动都从头计数，进度条从 0% 重新开始。
        m_deviceProgress.insert(m_currentDevice, 0);

        for (DeviceWork *work : works) {
            if (work->direction() != DeviceWork::Send) {
                continue;
            }
            if (m_currentDevice == QStringLiteral("以太网设备")) {
                work->setTcpTarget(config.peerAddress,
                                   static_cast<quint16>(config.peerPort));
            }
            work->setSourceFile(sourcePath);
            work->startWork();
        }

        // TODO: 发送成功后把帧追加到该总线的发送容器：
        // appendMessageRecord(m_currentDevice, true, record);
        // 发送表格的定时刷新已由 m_messageRefreshTimer 完成。
    }

    // ── 接收线程参数设置 ──────────────────────────────────────────────
    // 以太网接收：work->setTcpLocal(本地IP, 本地端口)，
    //             参数取自设备管理页以太网接口栏的 IP / 端口号；
    // FC / Aurora 接收：当前无额外参数，接入总线接口后在对应
    //             FCRecvPro / AuroraRecvPro::processData() 中读取并上报。
    // 接收线程收到数据后通过 dataReceived 信号写入接收容器，
    // 由 300ms 定时器刷新到表格。
    if (config.receiveEnabled) {
        QString localAddress;
        quint16 localPort = 0;
        if (m_currentDevice == QStringLiteral("以太网设备")) {
            readTcpLocal(&localAddress, &localPort);
        }

        for (DeviceWork *work : works) {
            if (work->direction() != DeviceWork::Receive) {
                continue;
            }
            if (m_currentDevice == QStringLiteral("以太网设备")) {
                work->setTcpLocal(localAddress, localPort);
            }
            work->startWork();
        }

        // 接收线程启动后开始定时刷新表格：每 300ms 取容器前 50 条。
        m_messageRefreshTimer->start();
    }

    setDeviceState(m_currentDevice, RunState::Running);
}

void SceneParallelWidget::pauseDevice()
{
    if (m_currentDevice.isEmpty()) {
        return;
    }

    const RunState state =
        m_deviceStates.value(m_currentDevice, RunState::Stopped);
    const QList<DeviceWork *> works = deviceWorks(m_currentDevice);

    if (state == RunState::Paused) {
        // 继续：线程保持运行，工作对象从当前发送位置恢复。
        for (DeviceWork *work : works) {
            work->startWork();
        }
        setDeviceState(m_currentDevice, RunState::Running);
        ui->statusLabel->setText(
            QStringLiteral("%1 已继续").arg(m_currentDevice));
        return;
    }

    for (DeviceWork *work : works) {
        work->pauseWork();
    }

    setDeviceState(m_currentDevice, RunState::Paused);
    ui->statusLabel->setText(
        QStringLiteral("%1 已暂停").arg(m_currentDevice));
}

void SceneParallelWidget::stopDevice()
{
    if (m_currentDevice.isEmpty()) {
        return;
    }

    const QList<DeviceWork *> works = deviceWorks(m_currentDevice);
    for (DeviceWork *work : works) {
        work->stopWork();
    }

    setDeviceState(m_currentDevice, RunState::Stopped);
    ui->statusLabel->setText(
        QStringLiteral("%1 已停止").arg(m_currentDevice));
}

QList<DeviceWork *> SceneParallelWidget::deviceWorks(const QString &deviceName)
{
    const auto existing = m_deviceWorks.constFind(deviceName);
    if (existing != m_deviceWorks.constEnd()) {
        return existing.value();
    }

    const int ch = channelForDevice(deviceName);
    QList<DeviceWork *> works;

    // 每个通道各自拥有发送、接收工作对象，并分别放入独立线程：
    // FC 每通道 FCsendpro + FCrecvpro；
    // Aurora 每通道 AuroraSendPro + AuroraRecvPro（8 个通道各一套）；
    // 以太网为 TcpSendPro + TcpRecvPro。
    works.append(new DeviceWork(deviceName, ch, DeviceWork::Send, this));
    works.append(new DeviceWork(deviceName, ch, DeviceWork::Receive, this));

    // 接收线程每收到一笔数据就写入该总线的接收容器。
    for (DeviceWork *work : works) {
        if (work->direction() != DeviceWork::Receive) {
            continue;
        }
        connect(work,
                &DeviceWork::dataReceived,
                this,
                [this, deviceName](const QByteArray &data) {
                    handleReceivedData(deviceName, data);
                });
    }

    // 发送线程每发完一帧就通过 sendJD 上报进度，页面按设备名刷新进度条。
    for (DeviceWork *work : works) {
        if (work->direction() != DeviceWork::Send) {
            continue;
        }
        connect(work,
                &DeviceWork::sendJD,
                this,
                [this, deviceName](qint64 sentBytes, qint64 totalBytes) {
                    handleSendProgress(deviceName, sentBytes, totalBytes);
                });
    }

    m_deviceWorks.insert(deviceName, works);
    return works;
}

int SceneParallelWidget::channelForDevice(const QString &deviceName)
{
    // 设备名形如“FC设备1通道”“Aurora设备8通道”，取其中第一个数字；
    // “以太网设备”没有数字，视为通道 1。
    for (const QChar character : deviceName) {
        if (character.isDigit()) {
            return character.digitValue();
        }
    }
    return 1;
}

QString SceneParallelWidget::dataSourceFilePath(
    const QString &tableName,
    const QString &name) const
{
    if (tableName.isEmpty() || name.isEmpty()) {
        return QString();
    }

    const QString databasePath =
        QDir(QCoreApplication::applicationDirPath())
            .filePath(QStringLiteral("qsqlite/WT_FCData.db"));
    if (!QFileInfo::exists(databasePath)) {
        return QString();
    }

    QString filePath;
    const QString connectionName =
        uniqueConnectionName(QStringLiteral("scene_send_data"), this);
    {
        QSqlDatabase database =
            QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connectionName);
        database.setDatabaseName(databasePath);
        if (database.open()) {
            QSqlQuery query(database);
            query.prepare(
                QStringLiteral("SELECT filePath FROM %1 WHERE name = :name")
                    .arg(tableName));
            query.bindValue(QStringLiteral(":name"), name);
            if (query.exec() && query.next()) {
                filePath = query.value(0).toString();
            }
        }
        database.close();
    }
    QSqlDatabase::removeDatabase(connectionName);

    return filePath;
}

void SceneParallelWidget::readTcpLocal(QString *address, quint16 *port) const
{
    if (address != nullptr) {
        address->clear();
    }
    if (port != nullptr) {
        *port = 0;
    }

    // 设备管理页与场景并行页位于同一主窗口，按控件名读取以太网接口配置。
    QWidget *topLevel = window();
    if (topLevel == nullptr) {
        return;
    }

    if (QLineEdit *ipEdit =
            topLevel->findChild<QLineEdit *>(QStringLiteral("tcpIpEdit"))) {
        if (address != nullptr) {
            *address = ipEdit->text().trimmed();
        }
    }

    if (QLineEdit *portEdit =
            topLevel->findChild<QLineEdit *>(QStringLiteral("tcpPortEdit"))) {
        bool ok = false;
        const uint value = portEdit->text().trimmed().toUInt(&ok);
        if (ok && port != nullptr) {
            *port = static_cast<quint16>(value);
        }
    }
}

void SceneParallelWidget::setDeviceState(const QString &deviceName,
                                         RunState state)
{
    m_deviceStates.insert(deviceName, state);
    updateDeviceListEntry(deviceName);
    if (deviceName == m_currentDevice) {
        applyControlButtons();
    }
}

QIcon SceneParallelWidget::statusDotIcon(RunState state)
{
    // 图标尺寸与列表字体匹配，圆形绘制在正中，四周留少量空隙。
    constexpr int Size = 14;
    constexpr int Diameter = 10;

    QColor color;
    switch (state) {
    case RunState::Running:
        color = QColor(0x00, 0x80, 0x00); // 绿色
        break;
    case RunState::Paused:
        color = QColor(0xD9, 0xA4, 0x00); // 黄色
        break;
    case RunState::Stopped:
        color = QColor(0xE0, 0x00, 0x00); // 红色
        break;
    }

    QPixmap pixmap(Size, Size);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setPen(Qt::NoPen);
    painter.setBrush(color);
    const int margin = (Size - Diameter) / 2;
    painter.drawEllipse(margin, margin, Diameter, Diameter);
    painter.end();

    return QIcon(pixmap);
}

void SceneParallelWidget::updateDeviceListEntry(const QString &deviceName)
{
    if (deviceName.isEmpty()) {
        return;
    }

    const QIcon icon =
        statusDotIcon(m_deviceStates.value(deviceName, RunState::Stopped));
    const QList<QListWidgetItem *> items =
        ui->deviceList->findItems(deviceName, Qt::MatchExactly);
    for (QListWidgetItem *item : items) {
        item->setIcon(icon);
    }
}

void SceneParallelWidget::applyControlButtons()
{
    // 未选择设备时三个按钮都禁用；
    // 只有点击过“启动”（状态为运行或暂停）后才允许点击暂停和停止。
    const bool hasDevice = !m_currentDevice.isEmpty();
    const RunState state =
        m_deviceStates.value(m_currentDevice, RunState::Stopped);

    ui->startButton->setEnabled(hasDevice && state == RunState::Stopped);
    ui->pauseButton->setEnabled(hasDevice && state != RunState::Stopped);
    ui->stopButton->setEnabled(hasDevice && state != RunState::Stopped);

    // 暂停后按钮文字变为“继续”，再次点击从当前发送位置恢复。
    ui->pauseButton->setText(
        state == RunState::Paused ? QStringLiteral("继续")
                                  : QStringLiteral("暂停"));

    // 状态变化后同步刷新“当前设备”标签上的彩色状态文字。
    if (hasDevice) {
        updateCurrentDeviceLabel();
    }

    // 同时刷新启动按钮下方的发送进度条显示状态。
    updateSendProgressVisibility();
}

void SceneParallelWidget::updateSendProgressVisibility()
{
    // 只有当前设备已启动（运行或暂停）并且勾选了发送时才显示进度条。
    const RunState state =
        m_deviceStates.value(m_currentDevice, RunState::Stopped);
    const bool visible = !m_currentDevice.isEmpty()
        && state != RunState::Stopped
        && ui->sendCheckBox->isChecked();
    ui->sendProgressBar->setVisible(visible);
    ui->sendProgressBar->setValue(
        m_deviceProgress.value(m_currentDevice, 0));
}

void SceneParallelWidget::handleSendProgress(const QString &deviceName,
                                             qint64 sentBytes,
                                             qint64 totalBytes)
{
    // 按“已发送 / 数据源总长”换算百分比，总长为 0 时按 0% 处理。
    int percent = 0;
    if (totalBytes > 0) {
        const qint64 clamped = qBound<qint64>(0, sentBytes, totalBytes);
        percent = static_cast<int>(clamped * 100 / totalBytes);
    }
    m_deviceProgress.insert(deviceName, percent);

    // 只有当前正在查看的设备才立即刷新界面，其余设备的进度留在内存中，
    // 切换设备时由 updateSendProgressVisibility() 恢复显示。
    if (deviceName == m_currentDevice) {
        updateSendProgressVisibility();
    }
}

void SceneParallelWidget::updateCurrentDeviceLabel()
{
    if (m_currentDevice.isEmpty()) {
        ui->currentDeviceLabel->setText(QStringLiteral("当前设备：未选择"));
        return;
    }

    QString stateText;
    QString stateColor;
    switch (m_deviceStates.value(m_currentDevice, RunState::Stopped)) {
    case RunState::Running:
        stateText = QStringLiteral("在线");
        stateColor = QStringLiteral("#008000");
        break;
    case RunState::Paused:
        stateText = QStringLiteral("暂停");
        // 纯黄色在白底上不易辨认，使用偏深的黄色。
        stateColor = QStringLiteral("#D9A400");
        break;
    case RunState::Stopped:
        stateText = QStringLiteral("停止");
        stateColor = QStringLiteral("#E00000");
        break;
    }

    // 使用 HTML 富文本让状态文字显示为彩色，设备名保留标签默认颜色。
    ui->currentDeviceLabel->setText(
        QStringLiteral("当前设备：%1<span style='color: %2;'>&nbsp;%3</span>")
            .arg(m_currentDevice, stateColor, stateText));
}

void SceneParallelWidget::updatePeriodicInputState(bool enabled)
{
    // 未启用周期下发时，周期时间没有意义，保持禁用。
    ui->periodicTimeSpinBox->setEnabled(enabled);
}

void SceneParallelWidget::updateTcpTargetVisibility()
{
    // 只有以太网设备勾选“发送”时才需要填写对端 IP 和端口。
    const bool visible = ui->sendCheckBox->isChecked()
        && m_currentDevice == QStringLiteral("以太网设备");
    ui->peerIpLabel->setVisible(visible);
    ui->peerIpEdit->setVisible(visible);
    ui->peerPortLabel->setVisible(visible);
    ui->peerPortEdit->setVisible(visible);
}

// 同步“规则”标签与下拉框的显示状态，执行流程：
//   1. 按当前设备名称判定所属总线：0=FC，1=Aurora，2=以太网；
//   2. 从 kRuleComboSwitches 取该总线的宏开关（三条总线互不影响）；
//   3. 同时设置“规则”标签和下拉框的可见性。
// 切换设备时都会调用本函数，因此显示状态始终与该总线的宏一致。
void SceneParallelWidget::updateRuleVisibility()
{
    int busIndex = 2;
    if (m_currentDevice.startsWith(QStringLiteral("FC设备"))) {
        busIndex = 0;
    } else if (m_currentDevice.startsWith(QStringLiteral("Aurora设备"))) {
        busIndex = 1;
    }

    const bool visible = kRuleComboSwitches[busIndex];
    ui->ruleLabel->setVisible(visible);
    ui->ruleCombo->setVisible(visible);
}

void SceneParallelWidget::updateReceiveVisibility()
{
    // 三条总线（FC / Aurora / 以太网）都提供“接收”勾选框，
    // Aurora 的接收数据会写入 Aurora 接收表格。
    ui->receiveCheckBox->setVisible(true);
}

void SceneParallelWidget::updateFcCreditVisibility()
{
    // BBCrdit 只对 FC 设备有意义。
    const bool isFc =
        m_currentDevice.startsWith(QStringLiteral("FC设备"));
    ui->bbCreditLabel->setVisible(isFc);
    ui->bbCreditSpinBox->setVisible(isFc);

    // 进度模式下拉框（按总长/按时间）与进度滑块：
    // 仅当当前设备是 FC 设备且勾选了“发送”时才显示。
    const bool showProgress = isFc && ui->sendCheckBox->isChecked();
    ui->progressModeCombo->setVisible(showProgress);
    ui->dataProgressSlider->setVisible(showProgress);
    ui->progressInfoLabel->setVisible(showProgress);
}

// 同步“周期时间”标签与输入框的显示状态，执行流程：
//   1. 按当前设备名称判定所属总线：0=FC，1=Aurora，2=以太网；
//   2. 从 kPeriodicTimeSwitches 取该总线的宏开关（三条总线互不影响）；
//   3. 同时设置标签与毫秒输入框的可见性。
// 切换设备时都会调用本函数；隐藏时“周期下发”勾选框仍保留，
// 已保存的周期时间值不受影响。
void SceneParallelWidget::updatePeriodicTimeVisibility()
{
    int busIndex = 2;
    if (m_currentDevice.startsWith(QStringLiteral("FC设备"))) {
        busIndex = 0;
    } else if (m_currentDevice.startsWith(QStringLiteral("Aurora设备"))) {
        busIndex = 1;
    }

    const bool visible = kPeriodicTimeSwitches[busIndex];
    ui->periodicTimeLabel->setVisible(visible);
    ui->periodicTimeSpinBox->setVisible(visible);
}

void SceneParallelWidget::applyProgressRange()
{
    const ProgressMode mode =
        ui->progressModeCombo->currentIndex() == 1 ? ProgressMode::Time
                                                   : ProgressMode::Length;

    m_progressScale = 1;
    m_progressTimeScale = 1;

    if (mode == ProgressMode::Length) {
        // 按总长划分：刻度单位是字节。总长超过 int 上限时按比例压缩。
        qint64 maximum = qMax<qint64>(0, m_progressTotalBytes);
        const qint64 intMaximum = std::numeric_limits<int>::max();
        if (maximum > intMaximum) {
            m_progressScale = maximum / intMaximum + 1;
            maximum = maximum / m_progressScale;
        }
        ui->dataProgressSlider->setRange(0, static_cast<int>(maximum));
    } else {
        // 按时间划分：刻度单位是毫秒。
        // 总时长超过 int 上限（约 24.8 天）时同样按比例压缩，
        // 显示和百分比再按 m_progressTimeScale 换算回真实毫秒。
        qint64 maximum = qMax<qint64>(0, m_progressTotalMs);
        const qint64 intMaximum = std::numeric_limits<int>::max();
        if (maximum > intMaximum) {
            m_progressTimeScale = maximum / intMaximum + 1;
            maximum = maximum / m_progressTimeScale;
        }
        ui->dataProgressSlider->setRange(0, static_cast<int>(maximum));
    }

    ui->dataProgressSlider->setValue(0);
    updateProgressInfo();
}

void SceneParallelWidget::updateProgressInfo()
{
    // 滑块刻度可能是压缩后的值（总长/总时长超过 int 上限时），
    // 这里先还原成真实值，再统一用 percentOf() 计算百分比，
    // 避免大数值溢出或刻度取整导致百分比不准确。
    const qint64 tick =
        static_cast<qint64>(ui->dataProgressSlider->value());

    if (ui->progressModeCombo->currentIndex() == 1) {
        // 按时间：刻度 × 压缩比例 = 当前毫秒数。
        const qint64 currentMs = qMin<qint64>(
            tick * m_progressTimeScale, m_progressTotalMs);
        const int percent = percentOf(currentMs, m_progressTotalMs);
        ui->progressInfoLabel->setText(
            QStringLiteral("%1%  %2 / %3")
                .arg(percent)
                .arg(formatDuration(currentMs))
                .arg(formatDuration(m_progressTotalMs)));
        return;
    }

    // 按总长：刻度 × 压缩比例 = 当前字节数。
    const qint64 bytes =
        qMin<qint64>(tick * m_progressScale, m_progressTotalBytes);
    const int percent = percentOf(bytes, m_progressTotalBytes);
    ui->progressInfoLabel->setText(
        QStringLiteral("%1%  %2 / %3 字节")
            .arg(percent)
            .arg(bytes)
            .arg(m_progressTotalBytes));
}

void SceneParallelWidget::handleProgressModeChanged(int index)
{
    Q_UNUSED(index)
    applyProgressRange();
}

void SceneParallelWidget::setProgressTotalLength(qint64 totalBytes)
{
    m_progressTotalBytes = qMax<qint64>(0, totalBytes);
    applyProgressRange();
}

void SceneParallelWidget::setProgressPosition(qint64 positionBytes)
{
    // 位置只在按总长模式下有效；按比例换算成滑块刻度。
    if (ui->progressModeCombo->currentIndex() == 1) {
        return;
    }

    const qint64 clamped =
        qBound<qint64>(0, positionBytes, m_progressTotalBytes);
    ui->dataProgressSlider->setValue(
        static_cast<int>(clamped / m_progressScale));
}

void SceneParallelWidget::setProgressTotalTime(qint64 totalMs)
{
    m_progressTotalMs = qMax<qint64>(0, totalMs);
    if (ui->progressModeCombo->currentIndex() == 1) {
        applyProgressRange();
    }
}

// 设置指定总线的单文件上限（接口 1/3）。
// 执行流程：
//   1) 校验 busIndex（0=FC，1=Aurora，2=以太网），非法直接返回；
//   2) bytes > 0 使用传入值，否则取该总线宏
//      FC/AURORA/TCP_RECV_SAVE_MAX_BYTES 的默认值；
//   3) 只改上限，不关闭已经打开的文件：下一次写入判断时才会生效。
// 优先级：运行时设置 > 本总线宏 > 1 GB 默认值。
void SceneParallelWidget::setReceiveSaveMaxBytes(int busIndex, qint64 bytes)
{
    if (busIndex < 0 || busIndex >= 3) {
        return;
    }
    m_recvSaveMaxBytes[busIndex] =
        bytes > 0 ? bytes : kRecvSaveMaxBytes[busIndex];
}

// 统一设置三条总线的单文件上限（接口 2/3）。
// 内部逐个调用按总线设置的版本，因此传 0 时三条总线都会恢复各自的宏默认值
// （而不是把某一条的默认值套用到全部总线上）。
void SceneParallelWidget::setReceiveSaveMaxBytes(qint64 bytes)
{
    for (int busIndex = 0; busIndex < 3; ++busIndex) {
        setReceiveSaveMaxBytes(busIndex, bytes);
    }
}

// 读取指定总线当前生效的单文件上限（接口 3/3）。
// @param busIndex 0=FC，1=Aurora，2=以太网。
// @return 上限字节数；索引非法返回 0。
qint64 SceneParallelWidget::receiveSaveMaxBytes(int busIndex) const
{
    if (busIndex < 0 || busIndex >= 3) {
        return 0;
    }
    return m_recvSaveMaxBytes[busIndex];
}

// 消息表格初始化。
// 执行流程：
//   1. 用六个独立的列名接口设置默认列名（通道号 / 时标 / 值），
//      列由代码创建，.ui 中不再配置列；
//   2. 收集六张表（FC/Aurora/以太网 × 发送/接收）；
//   3. 逐张设置表头高度、对齐方式、列宽策略（第 3 列拉伸，其余按内容）；
//   4. 关闭纵表头、设置行高与只读行为。
// 列宽策略不会被列名接口修改，调整列宽时需要在这里一并调整。
void SceneParallelWidget::configureMessageTables()
{
    // 默认列名：三总线、发送/接收共用同一套。
    // 后期要改默认列名，只改这里的列表内容即可（或单独改某张表的调用）。
    const QStringList defaultTitles = {
        QStringLiteral("通道号"),
        QStringLiteral("时标"),
        QStringLiteral("值")
    };

    // 必须先把列建出来，下面按列号设置列宽策略才有意义。
    setFcSendTableHeader(defaultTitles);
    setFcReceiveTableHeader(defaultTitles);
    setAuroraSendTableHeader(defaultTitles);
    setAuroraReceiveTableHeader(defaultTitles);
    setTcpSendTableHeader(defaultTitles);
    setTcpReceiveTableHeader(defaultTitles);

    // 六个表格分别对应：
    // FC 发送、FC 接收、Aurora 发送、Aurora 接收、以太网发送、以太网接收。
    const QList<QTableWidget *> tables = {
        ui->fcSendTable,
        ui->fcReceiveTable,
        ui->auroraSendTable,
        ui->auroraReceiveTable,
        ui->tcpSendTable,
        ui->tcpReceiveTable
    };

    for (QTableWidget *table : tables) {
        if (table == nullptr) {
            continue;
        }

        // 通道号、时标按内容自适应，值列占用剩余宽度显示数据。
        table->horizontalHeader()->setHighlightSections(false);
        // 表头高度固定为 34px，保证“通道号 / 时标 / 值”文字完整显示，
        // 不受全局样式表 padding 影响。需要调整时改这里的数值即可。
        table->horizontalHeader()->setFixedHeight(34);
        table->horizontalHeader()->setDefaultAlignment(
            Qt::AlignCenter | Qt::AlignVCenter);
        table->horizontalHeader()->setSectionResizeMode(
            QHeaderView::ResizeToContents);
        table->horizontalHeader()->setSectionResizeMode(
            2, QHeaderView::Stretch);
        table->verticalHeader()->setVisible(false);
        table->verticalHeader()->setDefaultSectionSize(28);
        table->setSelectionMode(QAbstractItemView::SingleSelection);
        table->setSelectionBehavior(QAbstractItemView::SelectRows);
        table->setEditTriggers(QAbstractItemView::NoEditTriggers);
        table->setWordWrap(false);
    }
}

int SceneParallelWidget::messageTabIndexForDevice(
    const QString &deviceName) const
{
    if (deviceName.startsWith(QStringLiteral("Aurora设备"))) {
        return 1;
    }
    if (deviceName == QStringLiteral("以太网设备")) {
        return 2;
    }
    return 0;
}

// 枚举 → 控件映射（消息表格接口的统一入口）。
// setFcSendTableHeader() 等六个列名接口、messageTableHeader()、
// setMessageTableItem()、addMessageTableColumn() 都通过这里定位控件；
// 非法枚举返回 nullptr。
QTableWidget *SceneParallelWidget::messageTable(MessageTable table) const
{
    switch (table) {
    case MessageTable::FcSend:
        return ui->fcSendTable;
    case MessageTable::FcReceive:
        return ui->fcReceiveTable;
    case MessageTable::AuroraSend:
        return ui->auroraSendTable;
    case MessageTable::AuroraReceive:
        return ui->auroraReceiveTable;
    case MessageTable::TcpSend:
        return ui->tcpSendTable;
    case MessageTable::TcpReceive:
        return ui->tcpReceiveTable;
    }
    return nullptr;
}

// 六张消息表格列名设置的统一实现（六个对外接口都走这里）：
//   1. 控件为空（未创建或映射错误）直接返回，避免崩溃；
//   2. setColumnCount() 按列表长度重建列数：
//      变多时新增空列，变少时多余列连同内容一起被裁掉；
//   3. setHorizontalHeaderLabels() 按顺序写入列名。
// 说明：只改列名和列数，不动行数据，也不改列宽策略；
// 每 300ms 的自动刷新只写前 3 列，第 4 列及之后的内容不会被覆盖。
void SceneParallelWidget::applyMessageTableHeader(
    QTableWidget *table,
    const QStringList &columnTitles)
{
    if (table == nullptr) {
        return;
    }

    table->setColumnCount(columnTitles.size());
    table->setHorizontalHeaderLabels(columnTitles);
}

// FC 发送表列名。
void SceneParallelWidget::setFcSendTableHeader(const QStringList &columnTitles)
{
    applyMessageTableHeader(ui->fcSendTable, columnTitles);
}

// FC 接收表列名。
void SceneParallelWidget::setFcReceiveTableHeader(
    const QStringList &columnTitles)
{
    applyMessageTableHeader(ui->fcReceiveTable, columnTitles);
}

// Aurora 发送表列名。
void SceneParallelWidget::setAuroraSendTableHeader(
    const QStringList &columnTitles)
{
    applyMessageTableHeader(ui->auroraSendTable, columnTitles);
}

// Aurora 接收表列名。
void SceneParallelWidget::setAuroraReceiveTableHeader(
    const QStringList &columnTitles)
{
    applyMessageTableHeader(ui->auroraReceiveTable, columnTitles);
}

// 以太网发送表列名。
void SceneParallelWidget::setTcpSendTableHeader(
    const QStringList &columnTitles)
{
    applyMessageTableHeader(ui->tcpSendTable, columnTitles);
}

// 以太网接收表列名。
void SceneParallelWidget::setTcpReceiveTableHeader(
    const QStringList &columnTitles)
{
    applyMessageTableHeader(ui->tcpReceiveTable, columnTitles);
}

// 读取当前列名，执行流程：
//   1. messageTable() 找到控件，失败返回空列表；
//   2. 按列号顺序读取 horizontalHeaderItem()->text()；
//   3. 表头项为空时补空字符串，保证返回列表长度与列数一致。
QStringList SceneParallelWidget::messageTableHeader(
    MessageTable table) const
{
    QStringList titles;
    QTableWidget *target = messageTable(table);
    if (target == nullptr) {
        return titles;
    }

    for (int column = 0; column < target->columnCount(); ++column) {
        const QTableWidgetItem *item =
            target->horizontalHeaderItem(column);
        titles.append(item != nullptr ? item->text() : QString());
    }
    return titles;
}

// 设置指定单元格文本，执行流程：
//   1. messageTable() 找到控件；控件为空、行为负、列越界均返回 false；
//   2. 行数不足时 setRowCount(row + 1) 自动补齐（会连带扩充所有列）；
//   3. 单元格不存在时新建只读 QTableWidgetItem，避免用户双击编辑；
//   4. 写入文本。
// 注意：只改界面显示，不写入 m_messageContainers；
// refreshMessageTables() 每 300ms 会按容器条数重设行数并重写前 3 列，
// 因此自定义内容应放在“已存在的行 + 第 4 列及以后”。
bool SceneParallelWidget::setMessageTableItem(
    MessageTable table,
    int row,
    int column,
    const QString &text)
{
    QTableWidget *target = messageTable(table);
    if (target == nullptr || row < 0 || column < 0
        || column >= target->columnCount()) {
        return false;
    }

    // 行数不足时自动补齐，方便外部直接向第 N 行写数据。
    if (row >= target->rowCount()) {
        target->setRowCount(row + 1);
    }

    QTableWidgetItem *item = target->item(row, column);
    if (item == nullptr) {
        item = new QTableWidgetItem;
        item->setFlags(item->flags() & ~Qt::ItemIsEditable);
        target->setItem(row, column, item);
    }
    item->setText(text);
    return true;
}

// 在表格末尾追加一列（供二次开发调用），执行流程：
//   1. messageTable() 把枚举翻译成控件，失败返回 -1；
//   2. 以当前列数作为新列下标（即“追加到末尾”），insertColumn() 插入空列；
//   3. 新建表头项写入列名；
//   4. 新列列宽设为按内容自适应，避免和第 3 列的拉伸策略冲突；
//   5. 返回新列下标，调用方直接传给 setMessageTableItem() 即可填内容。
// 说明：已有列及其内容全部保留，只新增一列；
// 每 300ms 的自动刷新只写前 3 列，不会覆盖追加列的内容。
int SceneParallelWidget::addMessageTableColumn(MessageTable table,
                                               const QString &columnTitle)
{
    QTableWidget *target = messageTable(table);
    if (target == nullptr) {
        return -1;
    }

    const int column = target->columnCount();
    target->insertColumn(column);

    // insertColumn() 只会插入空列，表头项需要手动创建并设置文本。
    target->setHorizontalHeaderItem(column,
                                    new QTableWidgetItem(columnTitle));

    // 追加列不使用 Stretch（第 3 列已拉伸），按内容自适应即可。
    target->horizontalHeader()->setSectionResizeMode(
        column, QHeaderView::ResizeToContents);

    return column;
}

QString SceneParallelWidget::containerKey(const QString &deviceName,
                                          bool sending)
{
    QString bus;
    if (deviceName.startsWith(QStringLiteral("FC设备"))) {
        bus = QStringLiteral("FC");
    } else if (deviceName.startsWith(QStringLiteral("Aurora设备"))) {
        bus = QStringLiteral("Aurora");
    } else {
        bus = QStringLiteral("TCP");
    }

    return bus + (sending ? QStringLiteral("/send")
                          : QStringLiteral("/receive"));
}

void SceneParallelWidget::appendMessageRecord(
    const QString &deviceName,
    bool sending,
    const MessageRecord &record)
{
    QVector<MessageRecord> &container =
        m_messageContainers[containerKey(deviceName, sending)];

    // 容器最多保留 200 条，超出时丢弃最旧一条。
    while (container.size() >= kMessageContainerSize) {
        container.removeFirst();
    }
    container.append(record);
}

void SceneParallelWidget::handleReceivedData(const QString &deviceName,
                                             const QByteArray &data)
{
    MessageRecord record;
    record.channel = QString::number(channelForDevice(deviceName));
    record.time = QDateTime::currentDateTime().toString(
        QStringLiteral("hh:mm:ss.zzz"));

    // 值列显示数据前 16 个字节的 HEX，避免单行过长。
    const int previewSize = qMin(data.size(), 16);
    record.value = BinFormat::toHex(data.left(previewSize));
    if (data.size() > previewSize) {
        record.value.append(QStringLiteral(" ..."));
    }

    appendMessageRecord(deviceName, false, record);

    // 接收数据落盘保存（FC / Aurora / 以太网 三条总线分别由各自的开关宏控制）。
    saveReceivedData(deviceName, data);
}

// 接收数据落盘完整流程（QByteArray / char* 两个入口共用）：
//   FCRecvPro / AuroraRecvPro / TcpRecvPro::processData()
//     └─ emit dataReceived(...)  // QByteArray 或 char* + 长度
//          └─ DeviceWork 转发（跨线程 queued connection）
//               └─ SceneParallelWidget::handleReceivedData(deviceName, data)
//                    ├─ 刷新界面接收容器（appendMessageRecord）
//                    └─ saveReceivedData(deviceName, data)   ← 本函数
//                         1) 空数据（空指针 / 长度 <= 0）直接返回；
//                         2) 按设备名前缀得到总线索引（0=FC，1=Aurora，2=TCP）；
//                         3) 检查该总线的开关宏（FC_RECV_SAVE_ENABLED 等），
//                            关闭时只保留界面刷新，不写文件；
//                         4) 取该总线当前文件句柄（每条总线互不影响）；
//                         5) 还没有文件，或 size + 本次数据 > 1 GB 时，
//                            调用 openReceiveSaveFile() 分卷新建；
//                         6) 追加写入；写入失败（磁盘满、无权限）则关闭并
//                            释放句柄，下次数据到来时自动重试；
//                         7) flush 落盘，避免异常退出丢数据。
//   程序退出：析构函数统一 flush + close 三条总线的文件。
//
// QByteArray 重载：直接转发到 char* + 长度版本，
// 保证两种数据来源走完全相同的一套存储逻辑。
void SceneParallelWidget::saveReceivedData(const QString &deviceName,
                                           const QByteArray &data)
{
    saveReceivedData(deviceName, data.constData(),
                     static_cast<qint64>(data.size()));
}

// char* + 长度重载：实际落盘实现（步骤编号对应上方流程注释）。
// recvpro 直接用 char* 上报时调用本版本，长度必须是真实字节数，
// 不能用 strlen()，否则遇到数据中的 0x00 会截断。
void SceneParallelWidget::saveReceivedData(const QString &deviceName,
                                           const char *data,
                                           qint64 length)
{
    // 1) 空指针或空数据无需写文件。
    if (data == nullptr || length <= 0) {
        return;
    }

    // 2) 按设备名称判断总线类型：0=FC，1=Aurora，2=以太网。
    int busIndex = 2;
    if (deviceName.startsWith(QStringLiteral("FC设备"))) {
        busIndex = 0;
    } else if (deviceName.startsWith(QStringLiteral("Aurora设备"))) {
        busIndex = 1;
    }

    // 3) 三条总线开关独立：只在对应宏开启时才落盘。
    if (!kRecvSaveSwitches[busIndex]) {
        return;
    }

    // 4) 取该总线自己的文件句柄。
    QFile *&file = m_recvSaveFiles[busIndex];

    // 5) 尚未创建文件，或本次写入会超过该总线的单文件上限：新建下一个文件。
    const qint64 currentSize = file != nullptr ? file->size() : 0;
    if (file == nullptr || currentSize + length > m_recvSaveMaxBytes[busIndex]) {
        if (!openReceiveSaveFile(busIndex)) {
            return;
        }
    }

    // 6) 追加写入；失败时释放句柄，等下一笔数据再重新建文件。
    if (file->write(data, length) != length) {
        file->close();
        delete file;
        file = nullptr;
        return;
    }

    // 7) 及时刷盘，避免异常退出时丢失最后一段时间的数据。
    file->flush();
}

// 为指定总线分卷新建保存文件，流程：
//   1) 参数校验（busIndex 只能为 0/1/2）；
//   2) 若该总线已有文件，先 flush + close 并释放，避免句柄泄漏；
//   3) 确保 可执行文件目录/SAVADATA 目录存在（三总线共用）；
//   4) 以“年月日时分_总线类型.bin”命名，例：202611111530_Aurora.bin；
//      同一分钟内分卷时追加序号避免覆盖；
//   5) 以只写方式打开，成功后把句柄存入 m_recvSaveFiles[busIndex]。
// 只有 openReceiveSaveFile() 会修改 m_recvSaveFiles，其余逻辑只读写该句柄。
bool SceneParallelWidget::openReceiveSaveFile(int busIndex)
{
    // 1) 索引越界保护。
    if (busIndex < 0 || busIndex >= 3) {
        return false;
    }

    QFile *&file = m_recvSaveFiles[busIndex];

    // 2) 关闭该总线上一个文件（达到上限或首发写入时都可能走到这里）。
    if (file != nullptr) {
        file->flush();
        file->close();
        delete file;
        file = nullptr;
    }

    // 3) 保存目录：可执行文件目录/SAVADATA。
    const QString directoryPath =
        QDir(QCoreApplication::applicationDirPath())
            .filePath(QStringLiteral("SAVADATA"));
    if (!QDir().mkpath(directoryPath)) {
        return false;
    }

    // 4) 文件名：年月日时分_总线类型.bin；同分钟分卷追加序号。
    const QString baseName =
        QDateTime::currentDateTime()
            .toString(QStringLiteral("yyyyMMddHHmm"))
        + QLatin1Char('_')
        + QString::fromLatin1(kRecvSaveBusNames[busIndex]);
    QString filePath =
        QDir(directoryPath).filePath(baseName + QStringLiteral(".bin"));
    int suffix = 1;
    while (QFile::exists(filePath)) {
        filePath = QDir(directoryPath)
                       .filePath(QStringLiteral("%1_%2.bin")
                                     .arg(baseName)
                                     .arg(suffix++));
    }

    // 5) 打开文件并保存句柄。
    auto *newFile = new QFile(filePath);
    if (!newFile->open(QIODevice::WriteOnly)) {
        delete newFile;
        return false;
    }

    file = newFile;
    return true;
}

// 定时刷新六张消息表格（每 300ms 触发一次），执行流程：
//   1. 固定绑定表：控件 ↔ 后台容器键（FC/Aurora/TCP × send/receive）；
//   2. 每个容器最多取前 50 条（kMessageVisibleRows）；
//   3. setRowCount(可见条数) —— 会把自己额外插入、超出该行数的行裁掉；
//   4. 逐行取容器里的 通道号/时标/值，写入第 0/1/2 列；
//      第 3 列及以后由外部通过 setMessageTableItem() 维护，刷新不覆盖。
void SceneParallelWidget::refreshMessageTables()
{
    // 每个表格显示对应容器最前面的 50 条记录。
    struct TableBinding {
        QTableWidget *table;
        QString key;
    };
    const QList<TableBinding> bindings = {
        {ui->fcSendTable, QStringLiteral("FC/send")},
        {ui->fcReceiveTable, QStringLiteral("FC/receive")},
        {ui->auroraSendTable, QStringLiteral("Aurora/send")},
        {ui->auroraReceiveTable, QStringLiteral("Aurora/receive")},
        {ui->tcpSendTable, QStringLiteral("TCP/send")},
        {ui->tcpReceiveTable, QStringLiteral("TCP/receive")}
    };

    for (const TableBinding &binding : bindings) {
        QTableWidget *table = binding.table;
        const QVector<MessageRecord> &container =
            m_messageContainers[binding.key];
        const int visibleRows = qMin(container.size(), kMessageVisibleRows);

        table->setRowCount(visibleRows);
        for (int row = 0; row < visibleRows; ++row) {
            const MessageRecord &record = container.at(row);
            const QStringList values = {
                record.channel, record.time, record.value
            };

            for (int column = 0; column < values.size(); ++column) {
                QTableWidgetItem *item = table->item(row, column);
                if (item == nullptr) {
                    item = new QTableWidgetItem;
                    item->setFlags(item->flags() & ~Qt::ItemIsEditable);
                    table->setItem(row, column, item);
                }
                item->setText(values.at(column));
            }
        }
    }
}

void SceneParallelWidget::saveCurrentDeviceControls()
{
    // 恢复控件会产生大量信号；此时不能把尚未完成恢复的中间状态写入配置。
    if (m_restoringControls || m_currentDevice.isEmpty()) {
        return;
    }

    persistDeviceConfig(m_currentDevice, currentDeviceConfig());
}

SceneParallelWidget::DeviceConfig
SceneParallelWidget::currentDeviceConfig() const
{
    DeviceConfig config;
    config.dataSource = ui->dataSourceCombo->currentData().toString();
    config.rule = ui->ruleCombo->currentData().toString();
    config.sendEnabled = ui->sendCheckBox->isChecked();
    config.receiveEnabled = ui->receiveCheckBox->isChecked();
    config.periodicEnabled = ui->periodicCheckBox->isChecked();
    config.periodMs = ui->periodicTimeSpinBox->value();
    config.peerAddress = ui->peerIpEdit->text().trimmed();
    config.peerPort = qBound(0,
                             ui->peerPortEdit->text().trimmed().toInt(),
                             65535);
    config.bbCredit = ui->bbCreditSpinBox->value();
    return config;
}

SceneParallelWidget::DeviceConfig
SceneParallelWidget::storedDeviceConfig(const QString &deviceName) const
{
    // 优先使用本次运行期间的内存缓存，避免每次切换设备都读取配置系统。
    const QMap<QString, DeviceConfig>::const_iterator cachedConfig =
        m_deviceConfigs.constFind(deviceName);
    if (cachedConfig != m_deviceConfigs.constEnd()) {
        return cachedConfig.value();
    }

    // 程序重新启动后内存缓存为空，此时从 QSettings 恢复上一次保存的值。
    QSettings settings;
    settings.beginGroup(QStringLiteral("sceneParallel/deviceConfigs"));
    settings.beginGroup(deviceName);

    DeviceConfig config;
    config.dataSource = settings.value(QStringLiteral("dataSource")).toString();
    config.rule = settings.value(QStringLiteral("rule")).toString();
    config.sendEnabled =
        settings.value(QStringLiteral("sendEnabled"), false).toBool();
    config.receiveEnabled =
        settings.value(QStringLiteral("receiveEnabled"), false).toBool();
    config.periodicEnabled =
        settings.value(QStringLiteral("periodicEnabled"), false).toBool();
    config.periodMs =
        settings.value(QStringLiteral("periodMs"), 1000).toInt();
    config.peerAddress =
        settings.value(QStringLiteral("peerAddress"),
                       QStringLiteral("127.0.0.1")).toString();
    config.peerPort =
        settings.value(QStringLiteral("peerPort"), 8080).toInt();
    config.bbCredit =
        settings.value(QStringLiteral("bbCredit"), 0).toInt();

    settings.endGroup();
    settings.endGroup();

    // 防止配置文件中出现超出控件范围的旧值或损坏值。
    config.periodMs = qBound(ui->periodicTimeSpinBox->minimum(),
                             config.periodMs,
                             ui->periodicTimeSpinBox->maximum());
    return config;
}

void SceneParallelWidget::persistDeviceConfig(
    const QString &deviceName,
    const DeviceConfig &config)
{
    if (deviceName.isEmpty()) {
        return;
    }

    // 内存缓存保证同一次运行中的设备切换速度快。
    m_deviceConfigs.insert(deviceName, config);

    // QSettings 使用 main.cpp 中配置的组织名和应用名。
    // 每个设备使用独立分组，互不覆盖。
    QSettings settings;
    settings.beginGroup(QStringLiteral("sceneParallel/deviceConfigs"));
    settings.beginGroup(deviceName);
    settings.setValue(QStringLiteral("dataSource"), config.dataSource);
    settings.setValue(QStringLiteral("rule"), config.rule);
    settings.setValue(QStringLiteral("sendEnabled"), config.sendEnabled);
    settings.setValue(QStringLiteral("receiveEnabled"), config.receiveEnabled);
    settings.setValue(QStringLiteral("periodicEnabled"),
                      config.periodicEnabled);
    settings.setValue(QStringLiteral("periodMs"), config.periodMs);
    settings.setValue(QStringLiteral("peerAddress"), config.peerAddress);
    settings.setValue(QStringLiteral("peerPort"), config.peerPort);
    settings.setValue(QStringLiteral("bbCredit"), config.bbCredit);
    settings.endGroup();
    settings.endGroup();
}
