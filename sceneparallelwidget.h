#pragma once

#include <QByteArray>
#include <QIcon>
#include <QMap>
#include <QString>
#include <QStringList>
#include <QVector>
#include <QWidget>

class QTimer;
class QFile;

// =============================================================================
// 接收数据落盘开关（FC / Aurora / 以太网 三条总线各自独立控制）
//
// 1：开启——该总线接收线程上报的数据会以二进制形式追加保存到
//    可执行文件目录/SAVADATA/ 下，文件名为 年月日时分_总线类型.bin：
//      FC_RECV_SAVE_ENABLED     → ..._FC.bin
//      AURORA_RECV_SAVE_ENABLED → ..._Aurora.bin
//      TCP_RECV_SAVE_ENABLED    → ..._TCP.bin
//    单个文件上限默认 1 GB，可通过 setReceiveSaveMaxBytes() 修改，
//    超过后自动新建下一个文件。
// 0：关闭——该总线只刷新界面表格，不写文件；其它总线不受影响。
//
// 需要调整时修改下面的默认值，或在编译参数里定义同名宏覆盖。
// =============================================================================
#ifndef FC_RECV_SAVE_ENABLED
#define FC_RECV_SAVE_ENABLED 1
#endif

#ifndef AURORA_RECV_SAVE_ENABLED
#define AURORA_RECV_SAVE_ENABLED 1
#endif

#ifndef TCP_RECV_SAVE_ENABLED
#define TCP_RECV_SAVE_ENABLED 1
#endif

// =============================================================================
// 接收数据单文件大小上限（三条总线各自独立控制，默认均为 1 GB）
//
// 写入前判断“当前文件大小 + 本次数据长度”是否超过该值，
// 超过则关闭旧文件、新建下一个文件继续保存。
//
//   FC_RECV_SAVE_MAX_BYTES     → FC 设备（..._FC.bin）
//   AURORA_RECV_SAVE_MAX_BYTES → Aurora 设备（..._Aurora.bin）
//   TCP_RECV_SAVE_MAX_BYTES    → 以太网设备（..._TCP.bin）
//
// 单位是字节，可用表达式书写，例如 500LL * 1024 * 1024。
// 修改示例（把 FC 改成 200 MB，其余总线仍为 1 GB）：
//   #define FC_RECV_SAVE_MAX_BYTES (200LL * 1024 * 1024)
// 也可在 .pro 中统一定义：DEFINES += TCP_RECV_SAVE_MAX_BYTES=...(表达式需加括号)
//
// 运行时也可调用 setReceiveSaveMaxBytes() 覆盖（传 0 恢复本宏默认值）；
// 优先级：运行时设置 > 本宏 > 1 GB。
// =============================================================================
#ifndef FC_RECV_SAVE_MAX_BYTES
#define FC_RECV_SAVE_MAX_BYTES (1024LL * 1024 * 1024)
#endif

#ifndef AURORA_RECV_SAVE_MAX_BYTES
#define AURORA_RECV_SAVE_MAX_BYTES (1024LL * 1024 * 1024)
#endif

#ifndef TCP_RECV_SAVE_MAX_BYTES
#define TCP_RECV_SAVE_MAX_BYTES (1024LL * 1024 * 1024)
#endif

// =============================================================================
// 场景并行“规则”下拉框显示开关（三条总线各自独立控制）
//
// 1：该总线设备显示“规则”标签和下拉框；
// 0：该总线设备隐藏“规则”标签和下拉框。
//
//   FC_SHOW_RULE_COMBO     → FC 设备（FC设备1/2通道）
//   AURORA_SHOW_RULE_COMBO → Aurora 设备（Aurora设备1~8通道）
//   TCP_SHOW_RULE_COMBO    → 以太网设备
//
// 三个宏互不影响，默认均为 1（全部显示）。
// 该宏只控制界面显示，不影响保存到配置里的规则值；
// 隐藏后仍可通过代码读写 ruleCombo。
// =============================================================================
#ifndef FC_SHOW_RULE_COMBO
#define FC_SHOW_RULE_COMBO 1
#endif

#ifndef AURORA_SHOW_RULE_COMBO
#define AURORA_SHOW_RULE_COMBO 1
#endif

#ifndef TCP_SHOW_RULE_COMBO
#define TCP_SHOW_RULE_COMBO 1
#endif

// =============================================================================
// 场景并行“周期时间”显示开关（三条总线各自独立控制）
//
// 1：该总线设备显示“周期时间:”标签与毫秒输入框；
// 0：该总线设备隐藏“周期时间:”标签与毫秒输入框。
//
//   FC_SHOW_PERIODIC_TIME     → FC 设备（FC设备1/2通道）
//   AURORA_SHOW_PERIODIC_TIME → Aurora 设备（Aurora设备1~8通道）
//   TCP_SHOW_PERIODIC_TIME    → 以太网设备
//
// 三个宏互不影响，默认均为 1（全部显示）。
// 仅控制界面显示，“周期下发”勾选框与周期值本身仍然保留，
// 隐藏后配置的周期时间依旧会随设备配置保存。
// =============================================================================
#ifndef FC_SHOW_PERIODIC_TIME
#define FC_SHOW_PERIODIC_TIME 1
#endif

#ifndef AURORA_SHOW_PERIODIC_TIME
#define AURORA_SHOW_PERIODIC_TIME 1
#endif

#ifndef TCP_SHOW_PERIODIC_TIME
#define TCP_SHOW_PERIODIC_TIME 1
#endif

namespace Ui {
class SceneParallelWidget;
}

class QListWidgetItem;
class QTableWidget;
class DeviceWork;

// 场景并行页面。
//
// 左侧固定显示 FC、Aurora 和以太网设备，右侧显示当前设备可配置的
// 数据源、协议规则、发送/接收开关及启动/暂停/停止按钮。
class SceneParallelWidget : public QWidget
{
    Q_OBJECT

public:
    // 构造场景并行页面。
    // @param parent Qt 父控件。
    explicit SceneParallelWidget(QWidget *parent = nullptr);

    // 销毁 Designer UI。
    ~SceneParallelWidget() override;

    // ── 进度条公开接口（供二次开发调用）────────────────────────────
    // 设置进度条按“总长”划分时的总字节数，并把滑块复位到 0。
    // 刻度单位：字节；总长超过 int 上限时会按比例压缩刻度，
    // 显示时再换算回真实字节数。
    // @param totalBytes 数据源总长度（字节），负值按 0 处理。
    void setProgressTotalLength(qint64 totalBytes);

    // 设置滑块当前所在位置。
    // @param positionBytes 已处理/已发送的字节数，仅在“按总长”模式生效；
    //                      超出总长时按总长处理。
    void setProgressPosition(qint64 positionBytes);

    // 预留接口：设置“按时间”划分时的总时长。
    // 时间模式的位置/进度条刻度由滑块本身表示（单位毫秒），
    // @param totalMs 总时长（毫秒），负值按 0 处理。
    void setProgressTotalTime(qint64 totalMs);

    // ── 接收数据落盘配置接口 ─────────────────────────────────────
    // 三条总线各自的单文件上限默认取自宏：
    //   FC_RECV_SAVE_MAX_BYTES / AURORA_RECV_SAVE_MAX_BYTES /
    //   TCP_RECV_SAVE_MAX_BYTES（均默认 1 GB）。
    // 页面收到接收线程上报的数据后，按“当前文件大小 + 本次长度”
    // 判断是否超限，超过则关闭旧文件、在同目录新建下一个文件继续保存。
    // 保存位置：可执行文件目录/SAVADATA/<年月日时分>_<总线类型>.bin
    //   总线类型：FC / Aurora / TCP
    //
    // 调用示例（只把 Aurora 限制为 100 MB）：
    //   page->setReceiveSaveMaxBytes(1, 100LL * 1024 * 1024);
    // 调用示例（三条总线统一限制为 500 MB）：
    //   page->setReceiveSaveMaxBytes(500LL * 1024 * 1024);
    //
    // 优先级：运行时设置 > 本总线宏 > 默认 1 GB。

    // 设置指定总线的单文件上限。
    // 只改上限，不关闭已打开的文件；下一次写入判断时才生效。
    // @param busIndex 总线索引：0=FC，1=Aurora，2=以太网；其它值忽略。
    // @param bytes 单文件上限（字节）；<= 0 时恢复该总线宏的默认值。
    void setReceiveSaveMaxBytes(int busIndex, qint64 bytes);

    // 统一设置三条总线的单文件上限。
    // 内部逐条调用上面的版本，因此传 0 时三条各自恢复自己的宏默认值。
    // @param bytes 单文件上限（字节）；<= 0 时三条总线都恢复宏默认值。
    void setReceiveSaveMaxBytes(qint64 bytes);

    // 返回指定总线当前生效的单文件上限（字节）。
    // 未调用过设置接口时返回该总线宏的值。
    // @param busIndex 总线索引：0=FC，1=Aurora，2=以太网。
    // @return 上限字节数；索引非法时返回 0。
    qint64 receiveSaveMaxBytes(int busIndex) const;

    // ── 消息表格接口（供二次开发）────────────────────────────────
    // 六张消息表格的标识（三总线 × 发送/接收）。
    enum class MessageTable {
        FcSend,       // FC 发送消息
        FcReceive,    // FC 接收消息
        AuroraSend,   // Aurora 发送消息
        AuroraReceive,// Aurora 接收消息
        TcpSend,      // 以太网发送消息
        TcpReceive    // 以太网接收消息
    };

    // 【1】设置各消息表格的列名（列数量 = 传入列表长度）。
    //
    // 每条总线的发送表 / 接收表都有一个独立接口，互不影响：
    //   setFcSendTableHeader / setFcReceiveTableHeader
    //   setAuroraSendTableHeader / setAuroraReceiveTableHeader
    //   setTcpSendTableHeader / setTcpReceiveTableHeader
    //
    // 六张表的默认列名（通道号 / 时标 / 值）也统一由这些接口设置：
    // 初始化代码位于 configureMessageTables()，要改默认列名改那里即可，
    // 不需要再改 .ui 文件。
    //
    // 共同执行流程（六个接口完全一致）：
    //   1. 取本总线本方向对应的 QTableWidget（控件为空时安全返回）；
    //   2. setColumnCount() 按列表长度重建列数，多出的列会被裁掉；
    //   3. setHorizontalHeaderLabels() 按顺序写入列名。
    //
    // 共同注意事项：
    //   - 自动刷新每 300ms 只写前 3 列（通道号/时标/值），
    //     第 4 列及之后的内容需用【3】自行填充，刷新不会覆盖；
    //   - 列数少于 3 时，刷新会忽略越界字段，不会报错；
    //   - 列宽策略在 configureMessageTables() 中设置，改列名不影响列宽。
    //
    // 调用示例（把三张接收表都扩成 4 列）：
    //   page->setFcReceiveTableHeader({"通道号", "时标", "值", "备注"});
    //   page->setAuroraReceiveTableHeader({"通道号", "时标", "值", "备注"});
    //   page->setTcpReceiveTableHeader({"通道号", "时标", "值", "备注"});

    // FC 发送消息表（fcSendTable）的列名。
    // @param columnTitles 新列名列表；传空列表表示清空列。
    void setFcSendTableHeader(const QStringList &columnTitles);

    // FC 接收消息表（fcReceiveTable）的列名。
    // @param columnTitles 新列名列表；传空列表表示清空列。
    void setFcReceiveTableHeader(const QStringList &columnTitles);

    // Aurora 发送消息表（auroraSendTable）的列名。
    // @param columnTitles 新列名列表；传空列表表示清空列。
    void setAuroraSendTableHeader(const QStringList &columnTitles);

    // Aurora 接收消息表（auroraReceiveTable）的列名。
    // @param columnTitles 新列名列表；传空列表表示清空列。
    void setAuroraReceiveTableHeader(const QStringList &columnTitles);

    // 以太网发送消息表（tcpSendTable）的列名。
    // @param columnTitles 新列名列表；传空列表表示清空列。
    void setTcpSendTableHeader(const QStringList &columnTitles);

    // 以太网接收消息表（tcpReceiveTable）的列名。
    // @param columnTitles 新列名列表；传空列表表示清空列。
    void setTcpReceiveTableHeader(const QStringList &columnTitles);

    // 【2】读取指定消息表格当前的列名列表（用于确认列数/列顺序）。
    //
    // 调用示例：
    //   const QStringList titles =
    //       page->messageTableHeader(SceneParallelWidget::MessageTable::FcSend);
    //   qDebug() << titles;   // 例如 ("通道号", "时标", "值")
    //
    // 执行流程：
    //   1. messageTable() 找到对应控件；
    //   2. 依次读取第 0 ~ columnCount-1 列的 horizontalHeaderItem 文本；
    //   3. 空表头项返回空字符串，非法枚举返回空列表。
    // @param table 目标表格。
    // @return 按列顺序返回的列名；表格无效时返回空列表。
    QStringList messageTableHeader(MessageTable table) const;

    // 【3】设置指定消息表格“第 row 行、第 column 列”的文本。
    //
    // 调用示例（在 FC 接收表第 0 行“备注”列写文字，需先按【1】扩到 4 列）：
    //   page->setMessageTableItem(
    //       SceneParallelWidget::MessageTable::FcReceive,
    //       0, 3, QStringLiteral("自定义说明"));
    //
    // 执行流程：
    //   1. messageTable() 找到对应控件；
    //   2. 校验 row/column 非负且 column 小于当前列数；
    //   3. row 超过当前行数时自动 setRowCount(row + 1) 补齐；
    //   4. 单元格不存在则新建只读 QTableWidgetItem；
    //   5. 写入文本。
    //
    // 重要限制（自动刷新会把表格“收回到容器条数”）：
    //   每 300ms 的 refreshMessageTables() 会把每张表按后台容器重新
    //   setRowCount(容器条数) 并重写前 3 列。因此：
    //   - 行号 >= 当前接收/发送条数时，自己写入的行会在下一次刷新被裁剪；
    //   - 前 3 列（通道号/时标/值）会被刷新覆盖；
    //   - 只有“已存在行 + 第 4 列及以后”的内容会保留。
    //   若需要完全自行维护表格内容，应同时调整 refreshMessageTables()
    //   的刷新逻辑（位于 sceneparallelwidget.cpp）。
    //
    // @param table 目标表格。
    // @param row 行号，从 0 开始。
    // @param column 列号，从 0 开始，需小于当前列数。
    // @param text 需要显示的文本。
    // @return 设置成功返回 true；表格/行列非法返回 false。
    bool setMessageTableItem(MessageTable table,
                             int row,
                             int column,
                             const QString &text);

    // 【4】在指定消息表格末尾追加一列（发送表 / 接收表通用）。
    //
    // 与【1】的区别：
    //   【1】的六个列名接口（如 setFcReceiveTableHeader）是“整体替换”，
    //        会把该表原有列全部清空重建；
    //   【4】addMessageTableColumn() 是“只追加”，保留已有列，新列加在最后。
    //   后期只想给某张表补一列时用本接口，不动其它列。
    //
    // 调用示例（给 FC 接收表加“备注”列，并在第 0 行写内容）：
    //   const int remarkColumn = page->addMessageTableColumn(
    //       SceneParallelWidget::MessageTable::FcReceive,
    //       QStringLiteral("备注"));
    //   if (remarkColumn >= 0) {
    //       page->setMessageTableItem(
    //           SceneParallelWidget::MessageTable::FcReceive,
    //           0, remarkColumn, QStringLiteral("自定义说明"));
    //   }
    //
    // 发送表与接收表用法完全一致，只是枚举不同：
    //   FcSend / AuroraSend / TcpSend     → 三张发送表
    //   FcReceive / AuroraReceive / TcpReceive → 三张接收表
    //
    // 执行流程：
    //   1. messageTable() 把枚举翻译成对应的 QTableWidget 控件；
    //   2. 控件为空（非法枚举）返回 -1，不做任何改动；
    //   3. 以当前列数作为新列下标，insertColumn() 在末尾插入一列；
    //   4. 写入表头文本，列宽策略设为“按内容自适应”；
    //   5. 返回新列下标，供 setMessageTableItem() 直接使用。
    //
    // 注意：
    //   - 自动刷新每 300ms 只写前 3 列，追加列的单元格不会被覆盖；
    //   - 追加列的行数跟随表格行数，表格重设行数时该列同步变化；
    //   - 只改界面，不写入后台消息容器。
    // @param table 目标表格。
    // @param columnTitle 新列的列名。
    // @return 新列下标（从 0 开始）；表格无效时返回 -1。
    int addMessageTableColumn(MessageTable table, const QString &columnTitle);

signals:
    // 用户启动设备并行配置。
    // @param deviceName 设备名称。
    // @param dataSource 数据源名称。
    // @param rule 规则名称。
    // @param sendEnabled 是否启用发送。
    // @param receiveEnabled 是否启用接收。
    // @param periodicEnabled 是否启用周期下发。
    // @param periodMs 周期时间，单位毫秒。
    void configurationConfirmed(const QString &deviceName,
                                const QString &dataSource,
                                const QString &rule,
                                bool sendEnabled,
                                bool receiveEnabled,
                                bool periodicEnabled,
                                int periodMs);

private slots:
    // 选择左侧设备并刷新右侧配置。
    // @param current 当前设备项；nullptr 表示取消选择。
    // @param previous 之前设备项，本实现不使用。
    void handleDeviceSelection(QListWidgetItem *current,
                               QListWidgetItem *previous);

    // 启动当前设备：校验并保存配置、写入消息列表，开始执行。
    void startDevice();

    // 暂停当前设备运行；暂停状态下再次触发表示继续发送。
    void pauseDevice();

    // 停止当前设备运行。
    void stopDevice();

    // 根据“周期下发”是否勾选，启用或禁用周期时间输入框。
    // @param enabled 周期下发是否启用。
    void updatePeriodicInputState(bool enabled);

    // 仅在以太网设备勾选“发送”时显示对端 IP 和端口输入框。
    void updateTcpTargetVisibility();

    // 同步 FC 专用控件的显示状态：
    //   BBCrdit 输入框：当前设备为 FC 时显示；
    //   进度模式下拉框与进度滑块：当前设备为 FC 且勾选“发送”时显示。
    void updateFcCreditVisibility();

    // 同步“周期时间:”标签与毫秒输入框的显示状态。
    // 三条总线分别由 FC_SHOW_PERIODIC_TIME / AURORA_SHOW_PERIODIC_TIME /
    // TCP_SHOW_PERIODIC_TIME 控制，切换设备时按当前设备所属总线取值。
    void updatePeriodicTimeVisibility();

    // 同步“规则”标签与下拉框的显示状态。
    // 三条总线分别由 FC_SHOW_RULE_COMBO / AURORA_SHOW_RULE_COMBO /
    // TCP_SHOW_RULE_COMBO 控制，切换设备时按当前设备所属总线取值。
    void updateRuleVisibility();

    // 同步“接收”勾选框的显示状态。
    // 三条总线（FC / Aurora / 以太网）均支持接收，Aurora 的接收数据
    // 会写入 Aurora 接收表格，因此该勾选框始终显示。
    void updateReceiveVisibility();

    // 发送线程上报进度时刷新“启动”按钮下方的进度条。
    // 信号在发送工作线程中 emit，经队列连接回到 GUI 线程执行。
    // @param deviceName 上报进度的设备名称。
    // @param sentBytes 已发送字节数。
    // @param totalBytes 数据源文件总字节数；为 0 时进度按 0% 处理。
    void handleSendProgress(const QString &deviceName,
                            qint64 sentBytes,
                            qint64 totalBytes);

    // 按当前划分方式刷新滑块范围与刻度换算比例。
    void applyProgressRange();

    // 刷新进度条右侧的“百分比 + 字节数/时间”文字。
    void updateProgressInfo();

    // 切换进度条划分方式后重新设置范围并复位到 0。
    // @param index 0=按总长，1=按时间。
    void handleProgressModeChanged(int index);

    // 按当前选择的 FC 数据源文件大小刷新进度条总长。
    void updateProgressTotalFromDataSource();

    // 当前设备任一配置发生变化时，实时记录到内存和 QSettings。
    // 切换设备、关闭页面或重新启动程序后均可恢复。
    void saveCurrentDeviceControls();

private:
    // 进度条划分方式。
    enum class ProgressMode {
        Length, // 按数据源总长（字节）划分。
        Time    // 按时间（毫秒）划分，位置暂由滑块直接表示。
    };

    // 一条设备的并行配置。
    struct DeviceConfig {
        QString dataSource;
        QString rule;
        bool sendEnabled = false;
        bool receiveEnabled = false;
        bool periodicEnabled = false;
        int periodMs = 1000;
        // TCP 发送时的对端地址和端口。
        QString peerAddress;
        int peerPort = 0;
        // FC 设备的 BBCrdit。
        int bbCredit = 0;
    };

    // 单个设备的运行状态。
    enum class RunState {
        Stopped, // 停止：只能点击启动。
        Running, // 运行：可以点击暂停和停止。
        Paused   // 暂停：可以点击启动（恢复）和停止。
    };

    // 按当前设备的运行状态刷新启动/暂停/停止按钮的可用性。
    void applyControlButtons();

    // 按“已启动 + 勾选发送”决定是否显示启动按钮下方的发送进度条，
    // 并把当前设备的进度值同步到进度条。
    void updateSendProgressVisibility();

    // 刷新“当前设备”标签：设备名后以彩色文字显示运行状态
    // （在线为绿色、暂停为黄色、停止为红色）。
    void updateCurrentDeviceLabel();

    // 生成状态圆点图标：停止红色、运行绿色、暂停黄色。
    // @param state 设备运行状态。
    static QIcon statusDotIcon(RunState state);

    // 刷新设备列表中指定设备名前的状态圆点。
    // @param deviceName 设备名称。
    void updateDeviceListEntry(const QString &deviceName);

    // 设置指定设备的运行状态，并同步按钮可用性。
    // @param deviceName 设备名称。
    // @param state 新的运行状态。
    void setDeviceState(const QString &deviceName, RunState state);

    // 返回指定设备的收发工作对象，首次调用时创建。
    // FC：发送 + 接收；Aurora：发送；以太网：发送 + 接收。
    // @param deviceName 设备名称。
    QList<DeviceWork *> deviceWorks(const QString &deviceName);

    // 从设备名称解析通道号；名称中没有数字时返回 1。
    // @param deviceName 设备名称，如“FC设备1通道”。
    static int channelForDevice(const QString &deviceName);

    // 读取指定数据源对应的文件路径，供发送线程按帧读取。
    // 只返回路径，避免把 1GB 级文件整体载入内存。
    // @param tableName 数据源表名。
    // @param name 数据源名称。
    // @return 文件绝对路径；未找到时返回空字符串。
    QString dataSourceFilePath(const QString &tableName,
                               const QString &name) const;

    // 从设备管理页的以太网接口栏读取本地 IP 和端口。
    // @param address 输出本地 IP。
    // @param port 输出本地端口。
    void readTcpLocal(QString *address, quint16 *port) const;

    // 初始化左侧设备列表。
    void initializeDeviceList();

    // 根据设备名称判断应读取哪张数据源表。
    // @param deviceName 设备名称。
    // @return table_data_FCsource、table_data_Aurorasource 或 table_data_TCPsource。
    QString dataSourceTableForDevice(const QString &deviceName) const;

    // 读取指定数据源表中的 name 字段。
    // @param tableName 数据源表名。
    // @return 数据源名称列表。
    QStringList loadDataSources(const QString &tableName) const;

    // 读取协议规则名称。
    // @return 规则名称列表。
    QStringList loadRuleNames() const;

    // 刷新当前设备右侧控件。
    // @param deviceName 当前设备名称。
    void refreshDevicePanel(const QString &deviceName);

    // 初始化 FC、Aurora、以太网三个消息表格的公共显示参数。
    void configureMessageTables();

    // 根据当前设备所属总线，返回应显示的消息页签索引。
    // @param deviceName 设备名称。
    // @return 0=FC，1=Aurora，2=以太网。
    int messageTabIndexForDevice(const QString &deviceName) const;

    // 按 MessageTable 枚举返回对应的表格控件；非法枚举返回 nullptr。
    QTableWidget *messageTable(MessageTable table) const;

    // 六张消息表格列名设置的统一实现。
    // 上面的 setFcSendTableHeader() 等六个接口都调用本函数，
    // 保证六张表的列名处理逻辑完全一致，后期只需改这一处。
    // @param table 目标控件；为空时直接返回，不做任何改动。
    // @param columnTitles 新的列名列表，长度即新列数。
    void applyMessageTableHeader(QTableWidget *table,
                                 const QStringList &columnTitles);

    // 一条消息记录，对应表格的“通道号 / 时标 / 值”三列。
    struct MessageRecord {
        QString channel; // 通道号。
        QString time;    // 时标。
        QString value;   // 数据内容摘要。
    };

    // 消息容器：每条总线分别有发送容器和接收容器，各最多保留 200 条。
    // 键为总线名（FC / Aurora / TCP）+ 方向。
    static QString containerKey(const QString &deviceName, bool sending);

    // 向指定总线方向的容器追加一条记录；超过 200 条时丢弃最旧一条。
    // @param deviceName 设备名称。
    // @param sending true 表示发送方向，false 表示接收方向。
    // @param record 需要追加的消息记录。
    void appendMessageRecord(const QString &deviceName,
                             bool sending,
                             const MessageRecord &record);

    // 接收线程收到数据后，生成记录并写入接收容器。
    // @param deviceName 设备名称。
    // @param data 收到的原始数据。
    void handleReceivedData(const QString &deviceName, const QByteArray &data);

    // 接收数据落盘入口（完整流程见下方注释）。
    //
    // 流程：
    //   1. 接收线程收到数据 → emit dataReceived；
    //   2. DeviceWork 的 dataReceived 信号转到 handleReceivedData()；
    //   3. handleReceivedData() 先刷新界面接收表格，再调用本函数；
    //   4. 本函数按设备名判定总线索引，检查该总线的开关宏，
    //      未开启则直接返回（界面刷新不受影响）；
    //   5. 取该总线当前文件，文件不存在或写入后会超过
    //      m_recvSaveMaxBytes[busIndex]（默认取该总线宏，1 GB）时
    //      调用 openReceiveSaveFile() 关闭旧文件并按时间新建；
    //   6. 追加写入并 flush，写入失败则关闭并释放句柄，下次自动重试。
    //
    // 保存位置：可执行文件目录/SAVADATA/<年月日时分>_<总线类型>.bin
    //   FC     → ..._FC.bin     受 FC_RECV_SAVE_ENABLED 控制
    //   Aurora → ..._Aurora.bin 受 AURORA_RECV_SAVE_ENABLED 控制
    //   以太网 → ..._TCP.bin    受 TCP_RECV_SAVE_ENABLED 控制
    // 单文件上限见 setReceiveSaveMaxBytes()。
    //
    // @param deviceName 设备名称，用于判断总线类型。
    // @param data 需要保存的原始接收数据。
    void saveReceivedData(const QString &deviceName, const QByteArray &data);

    // 同一落盘流程的 char* 重载，适配 recvpro 直接用 char* 上报的数据。
    //
    // 为什么必须带 length：
    //   接收数据是二进制，内部可能包含 0x00，按 '\0' 截断会丢数据，
    //   因此不能用 strlen() 推断长度，必须由调用方给出实际字节数。
    //
    // 调用示例（recvpro 的硬件接口回传 char*）：
    //   // char *buffer; int received = 硬件接收接口(buffer);
    //   page->saveReceivedData(QStringLiteral("FC设备1通道"),
    //                          buffer, received);
    // 内部行为与 QByteArray 版本完全一致（开关宏、分卷、flush、失败重试）。
    // @param deviceName 设备名称，用于判断总线类型。
    // @param data 数据首地址；为空指针时直接返回，不做任何写入。
    // @param length 数据字节数；<= 0 时直接返回。
    void saveReceivedData(const QString &deviceName,
                          const char *data,
                          qint64 length);

    // 为指定总线打开一个新的保存文件。
    // 会先关闭该总线上一个文件，再创建目录并按“年_月_日_时_分_秒.bin”命名；
    // 同一秒内分卷时自动追加序号避免覆盖。
    // @param busIndex 总线索引：0=FC，1=Aurora，2=以太网。
    // @return 目录创建并文件打开成功时返回 true。
    bool openReceiveSaveFile(int busIndex);

    // 定时器回调：把各容器前 50 条记录刷新到对应表格。
    void refreshMessageTables();

    // 从当前界面控件读取一份设备配置。
    // @return 当前界面显示的数据源、规则、方向和周期设置。
    DeviceConfig currentDeviceConfig() const;

    // 读取指定设备已经保存的配置。
    // @param deviceName 设备名称。
    // @return 已保存配置；从未保存时返回默认配置。
    DeviceConfig storedDeviceConfig(const QString &deviceName) const;

    // 将指定设备配置同时保存到内存和 QSettings。
    // @param deviceName 设备名称。
    // @param config 需要保存的设备配置。
    void persistDeviceConfig(const QString &deviceName,
                             const DeviceConfig &config);

    Ui::SceneParallelWidget *ui;
    QMap<QString, DeviceConfig> m_deviceConfigs;

    // 每个设备独立的运行状态；未记录的设备视为停止。
    QMap<QString, RunState> m_deviceStates;

    // 每个设备独立创建的收发工作对象，键为设备名称。
    QMap<QString, QList<DeviceWork *>> m_deviceWorks;

    // 每条总线各一个发送容器和一个接收容器，每个容器最多 200 条。
    QMap<QString, QVector<MessageRecord>> m_messageContainers;

    // 各设备的发送进度（0~100），键为设备名称。
    // 由发送线程 sendJD 上报后换算得到，切换设备时用于恢复显示。
    QMap<QString, int> m_deviceProgress;

    // 各总线当前正在写入的接收数据文件：
    // [0]=FC，[1]=Aurora，[2]=以太网。
    // 某总线开关宏为 0 时对应指针始终为 nullptr。
    QFile *m_recvSaveFiles[3] = {nullptr, nullptr, nullptr};

    // 三条总线各自的单文件上限（字节），初值取自
    // FC/AURORA/TCP_RECV_SAVE_MAX_BYTES 宏（默认均为 1 GB）。
    // 由 setReceiveSaveMaxBytes() 修改；写入前据此判断是否需要分卷。
    qint64 m_recvSaveMaxBytes[3] = {
        FC_RECV_SAVE_MAX_BYTES,
        AURORA_RECV_SAVE_MAX_BYTES,
        TCP_RECV_SAVE_MAX_BYTES
    };

    // 每 300ms 把容器前 50 条刷新到消息表格。
    QTimer *m_messageRefreshTimer = nullptr;

    // 进度条按总长划分时的数据源字节数。
    qint64 m_progressTotalBytes = 0;

    // 进度条按时间划分时的总时长（毫秒），预留。
    qint64 m_progressTotalMs = 0;

    // 总长超过 int 上限时，滑块刻度与真实字节数的换算比例。
    qint64 m_progressScale = 1;

    // 总时长超过 int 上限时，滑块刻度与真实毫秒数的换算比例。
    qint64 m_progressTimeScale = 1;

    QString m_currentDevice;

    // 恢复界面控件时会触发 currentIndexChanged、toggled 等信号。
    // 该标志用于阻止恢复过程中的中间状态覆盖已保存配置。
    bool m_restoringControls = false;
};
