// 对应界面：wid_review.ui。
// 本文件负责数据文件解析、数据表赋值、分页和 BIN 修改保存。
#include "wid_review.h"

// =============================================================================
// 主窗口实现
//
// 线程模型：
//   UI 线程       -> 文件对话框、表格、分页、进制切换、保存
//   导入工作线程  -> 只扫描包头并生成 BinPacket 元数据
//
// 数据加载策略：
//   导入阶段不读取数据段；只有当前选中包才会按需读取最多 64 KB。
//   这样即使打开 1 GB 文件，也不会把整个文件放入内存。
//
// -----------------------------------------------------------------------------
// 【数据回看完整调用链】传入文件 → 数据解包 → 显示到数据浏览表格
//
// 后续要修改“解包算法”，按下面的步骤定位即可，无需改动界面代码。
//
// ① 用户传入文件
//      chooseFile() / dropEvent() / loadFile()
//        └─ loadFile() 只做 O(1) 校验（存在性 + .bin 后缀），
//           调用 applySelectedFileInfo() 记录 m_selectedFilePath，
//           此时**不读取任何文件内容**。
//
// ② 选择总线类型（决定用哪套解包规则）
//      handleBusTypeChanged(index)
//        ├─ index 2 = Aurora → BinFormat::Protocol::Aurora
//        ├─ index 1 = FC     → BinFormat::Protocol::TCP（当前 FC 复用 TCP 规则）
//        └─ index 3 = 以太网 → BinFormat::Protocol::TCP
//        └─ beginImport(路径, 协议, 总线类型)
//
// ③ 创建解包线程（解包算法真正执行的地方）
//      beginImport()
//        ├─ new QThread + 按协议创建导入器：
//        │    Aurora → AuroraFileImporter（6 字节包头 + n × 1024 字节数据段）
//        │    FC     → FcFileImporter    （6 字节包头 + n 字节数据段）
//        │    以太网 → TcpFileImporter   （6 字节包头 + n 字节数据段）
//        ├─ importer->moveToThread(thread)
//        └─ thread started → importer->importFile(路径)   ← 阻塞式扫描
//
//      ★ 解包算法唯一需要修改的文件：
//          aurorafileimporter.cpp / fcfileimporter.cpp / tcpfileimporter.cpp
//        每个 importFile() 内部循环做三件事：
//          a) 读取固定 6 字节包头；
//          b) BinFormat::decodeDataCount(header) 解析 n
//             （字节序由 binformat.h 的 BIN_HEADER_BIG_ENDIAN 统一控制）；
//          c) 计算数据段长度 payloadLength（Aurora = n × 1024，其余 = n），
//             据此得到整包长度 packetSize() = 6 + payloadLength，
//             只把 offset / dataCount / payloadLength / header 记入 BinPacket，
//             不读取数据段内容（大文件不占内存的关键）。
//        修改包头长度或数据段长度规则时，同步调整 binformat.h 的
//        HeaderSize / PayloadSize 常量与上面的公式即可。
//
// ④ 解包结果回到 UI 线程
//      importer 信号（queued connection）：
//        progress → handleImportProgress()  刷新百分比与包数
//        finished → handleImportFinished()  接收 QVector<BinPacket>
//        failed   → handleImportFailed()    弹窗提示格式错误
//
// ⑤ 元数据缓存与界面摘要
//      handleImportFinished()
//        ├─ m_packets = packets;      ← 解包结果的唯一数据源，供分页/表格/详情共用
//        ├─ m_currentFilePath 记录路径，后续按需读取数据段
//        ├─ 刷新文件摘要（名称/大小/包数/目录）
//        ├─ populateCurrentPage()    ← 见 ⑥
//        └─ selectFirstPacketOnPage()→ 选中第一包，触发详情加载（见 ⑦）
//
// ⑥ 数据浏览表格填充（每页只建 10 行）
//      populateCurrentPage()
//        ├─ 计算当前页包索引范围：[m_currentPage × 10, +10)
//        ├─ 按 busType 由 setPacketTableSchema() 决定列结构：
//        │    列名、列顺序、列数量都来自 m_packetColumns（见 initializePacketTableColumns）：
//        │      FC / 以太网：包序号 / 文件偏移 / 数据长度 / 数据预览
//        │      Aurora    ：包序号 / 文件偏移 / 6 字节包头 / 数据块 n / 数据长度 / 数据预览
//        └─ 逐行用 BinPacket 的字段创建单元格，并 seek 到
//           offset + HeaderSize 只读前 12 字节作为“数据预览”
//      ★ 修改列名/列数量：
//          调用三条总线各自的公开接口
//            setFcPacketTableColumns(columns)      // 0=FC
//            setAuroraPacketTableColumns(columns)  // 1=Aurora
//            setTcpPacketTableColumns(columns)     // 2=以太网
//          或直接改 initializePacketTableColumns() 中的默认值。
//          每列的写法见 ReviewColumn：
//            内置列  {列名, ReviewColumnField::xxx}
//            数据列  {列名, ReviewDataUnit::Byte/Bit, 起始字节, 起始位,
//                     字节长, 位长}
//          数据列的值由 customColumnText() 按位置从该行数据段读取，
//          起始字节对应文件绝对偏移 = 包 offset + 6 字节包头 + 起始字节。
//          若需要新的内置数据来源，先在 ReviewColumnField 中加枚举值，
//          再在 packetCellText() 的 switch 中补一个 case。
//
// ⑦ 选中包后显示数据段（十六进制编辑区）
//      packetTable selectionChanged → updatePacketDetails()
//        └─ loadHexPayload(packetIndex)
//             ├─ 按 packet.offset + HeaderSize 读取数据段，上限 64 KB
//             ├─ 分块（每批 128 行）创建十六进制表格，避免大包卡顿
//             └─ saveCurrentPacket() 按 offset 原位写回文件
// =============================================================================

#include "aurorafileimporter.h"
#include "binfileimporterbase.h"
#include "chunkedtablefiller.h"
#include "fcfileimporter.h"
#include "tcpfileimporter.h"
#include "ui_wid_review.h"

#include <QBrush>
#include <QColor>
#include <QComboBox>
#include <QDir>
#include <QDragEnterEvent>
#include <QDragLeaveEvent>
#include <QDropEvent>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFrame>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QIcon>
#include <QItemSelectionModel>
#include <QKeySequence>
#include <QLabel>
#include <QMessageBox>
#include <QMimeData>
#include <QProgressBar>
#include <QPushButton>
#include <QResizeEvent>
#include <QShortcut>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QStandardPaths>

#include <QStyle>
#include <QStringList>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QThread>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>

namespace {

// 状态值会写入 QLabel 的动态属性 state，由 QSS 决定颜色和边框。
const QString IdleState = QStringLiteral("idle");
const QString SuccessState = QStringLiteral("success");
const QString ErrorState = QStringLiteral("error");

// 数据段表格单次最多加载 64 KB。
// 过大的包只允许查看和保存前 64 KB，避免创建过多表格单元格。
constexpr qint64 MaxHexPreviewBytes = 64 * 1024;

// 将字节数格式化为 B/KB/MB/GB，同时保留精确字节数。
// 例如 1073741824 -> "1.00 GB  (1073741824 B)"。
// @param bytes 待格式化的精确字节数。
// @return 带单位和精确字节数的显示文本。
QString formatFileSize(qint64 bytes)
{
    if (bytes < 1024) {
        return QStringLiteral("%1 B").arg(bytes);
    }

    double value = static_cast<double>(bytes) / 1024.0;
    QString unit = QStringLiteral("KB");
    if (bytes >= 1024LL * 1024 * 1024) {
        value = static_cast<double>(bytes) / (1024.0 * 1024.0 * 1024.0);
        unit = QStringLiteral("GB");
    } else if (bytes >= 1024LL * 1024) {
        value = static_cast<double>(bytes) / (1024.0 * 1024.0);
        unit = QStringLiteral("MB");
    }

    return QStringLiteral("%1 %2  (%3 B)")
        .arg(QString::number(value, 'f', 2))
        .arg(unit)
        .arg(bytes);
}

// 将文件偏移格式化为固定八位大写 HEX。
// @param offset 文件绝对字节偏移。
// @return 例如 966 返回 "0x000003C6"。
QString formatHexOffset(qint64 offset)
{
    return QStringLiteral("0x")
        + QString::number(offset, 16)
              .rightJustified(8, QLatin1Char('0'))
              .toUpper();
}

// 重新应用控件的动态属性样式。
// @param widget 需要刷新样式的控件；传 nullptr 时函数直接返回。
void refreshStyle(QWidget *widget)
{
    if (widget == nullptr) {
        return;
    }

    widget->style()->unpolish(widget);
    widget->style()->polish(widget);
    widget->update();
}

// 判断拖放 URL 是否为本机存在的 .bin 文件。
// @param url 拖放事件提供的文件 URL。
// @return 本地存在的 .bin 文件返回 true，否则返回 false。
bool isBinUrl(const QUrl &url)
{
    if (!url.isLocalFile()) {
        return false;
    }

    const QFileInfo info(url.toLocalFile());
    return info.isFile() && info.suffix().compare(QStringLiteral("bin"), Qt::CaseInsensitive) == 0;
}

} // namespace

WidReview::WidReview(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::WidReview)
{
    // Ui::setupUi 创建所有控件并应用 Designer 中定义的布局和初始属性。
    ui->setupUi(this);

    // WidReview 是页面栈中的独立 QWidget。
    // 顶层布局由原 centralWidget 和底部状态栏组成。
    auto *rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(0);
    rootLayout->addWidget(ui->centralWidget, 1);

    // 主窗口本身接收拖放事件。这样拖到上传区、表格或空白区域都能导入文件。
    setAcceptDrops(true);
    ui->chooseButton->setIcon(QIcon(QStringLiteral(":/icons/folder-open.svg")));
    ui->chooseButton->setIconSize(QSize(16, 16));
    ui->clearButton->setIcon(QIcon(QStringLiteral(":/icons/trash-2.svg")));
    ui->clearButton->setIconSize(QSize(15, 15));
    // 页面内部状态栏，用于显示扫描、保存和错误等状态。
    auto *statusFrame = new QFrame;
    statusFrame->setObjectName(QStringLiteral("reviewStatusBar"));
    statusFrame->setFixedHeight(22);
    auto *statusLayout = new QHBoxLayout(statusFrame);
    statusLayout->setContentsMargins(10, 0, 10, 0);
    statusLayout->setSpacing(8);
    m_reviewStatusLabel = new QLabel(QStringLiteral("就绪"));
    m_reviewStatusLabel->setObjectName(QStringLiteral("reviewStatusText"));
    statusLayout->addWidget(m_reviewStatusLabel, 1);

    // 进度条仅在后台导入期间显示。
    m_importProgress = new QProgressBar(this);
    m_importProgress->setRange(0, 100);
    m_importProgress->setValue(0);
    m_importProgress->setTextVisible(true);
    m_importProgress->setFixedWidth(260);
    m_importProgress->setFixedHeight(14);
    m_importProgress->hide();
    statusLayout->addWidget(m_importProgress);
    rootLayout->addWidget(statusFrame);

    // BinPacket 和容器类型会通过 queued signal 跨线程传递，必须先注册元类型。
    qRegisterMetaType<BinPacket>("BinPacket");
    qRegisterMetaType<QVector<BinPacket>>("QVector<BinPacket>");

    // 数据包列表和可编辑数据段表格的列、尺寸及交互方式在代码中统一配置。
    // 先初始化三条总线的默认列定义，再配置表格本身。
    initializePacketTableColumns();

    // =====================================================================
    // ★ 修改数据浏览表列名的调用位置 ★
    //
    // 需要改某条总线的列名 / 列数量时，在下面按总线新增调用即可（示例）：
    //
    //   // 1) FC 数据浏览表（总线类型选 FC 时生效）
    //   setFcPacketTableColumns({
    //       {QStringLiteral("包序号"), ReviewColumnField::Index},
    //       {QStringLiteral("文件偏移"), ReviewColumnField::Offset},
    //       // 数据段第 0 字节，从最高位起取 4 位，列名“通道号”
    //       {QStringLiteral("通道号"), ReviewDataUnit::Bit, 0, 0, 0, 4}
    //   });
    //
    //   // 2) Aurora 数据浏览表
    //   setAuroraPacketTableColumns({
    //       {QStringLiteral("包序号"), ReviewColumnField::Index},
    //       {QStringLiteral("文件偏移"), ReviewColumnField::Offset},
    //       {QStringLiteral("6 字节包头"), ReviewColumnField::Header},
    //       {QStringLiteral("数据块 n"), ReviewColumnField::DataCount},
    //       {QStringLiteral("数据长度"), ReviewColumnField::PayloadLength},
    //       {QStringLiteral("数据预览"), ReviewColumnField::PayloadPreview}
    //   });
    //
    //   // 3) 以太网数据浏览表
    //   setTcpPacketTableColumns({
    //       {QStringLiteral("包序号"), ReviewColumnField::Index},
    //       {QStringLiteral("文件偏移"), ReviewColumnField::Offset},
    //       {QStringLiteral("前 2 字节"), ReviewDataUnit::Byte, 0, 0, 2, 0}
    //   });
    //
    // 三条总线互不影响；传入列表长度即列数、顺序即列顺序。
    // 若只是微调默认列名，直接改 initializePacketTableColumns() 里的默认
    // 列表即可，不必在这里重复调用。
    // =====================================================================

    configureTable();
    configureHexTable();

    // 首次打开文件时定位到用户文档目录。
    m_lastDirectory =
        QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    if (m_lastDirectory.isEmpty()) {
        m_lastDirectory = QDir::homePath();
    }

    // ---------------------------------------------------------------------
    // 信号与槽连接
    //
    // 所有文件打开、分页、进制切换、单元格编辑和保存操作最终都汇聚到
    // WidReview 的私有槽函数，便于统一维护界面状态。
    // ---------------------------------------------------------------------
    connect(ui->chooseButton, &QPushButton::clicked, this, &WidReview::chooseFile);
    connect(ui->clearButton, &QPushButton::clicked, this, &WidReview::clearFile);
    connect(ui->prevPageButton, &QPushButton::clicked, this, &WidReview::showPreviousPage);
    connect(ui->nextPageButton, &QPushButton::clicked, this, &WidReview::showNextPage);
    connect(ui->jumpPageButton, &QPushButton::clicked, this, &WidReview::jumpToPage);
    connect(ui->pageJumpSpinBox,
            &QSpinBox::editingFinished,
            this,
            &WidReview::jumpToPage);
    connect(ui->savePacketButton,
            &QPushButton::clicked,
            this,
            &WidReview::saveCurrentPacket);
    connect(ui->displayModeCombo,
            static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
            this,
            &WidReview::changeNumberBase);
    connect(ui->busTypeCombo,
            static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
            this,
            &WidReview::handleBusTypeChanged);
    connect(ui->hexTable,
            &QTableWidget::itemChanged,
            this,
            &WidReview::handleHexItemChanged);
    connect(ui->packetTable->selectionModel(),
            &QItemSelectionModel::selectionChanged,
            this,
            &WidReview::updatePacketDetails);
    // Ctrl+O 与“选择文件”按钮保持相同行为。
    auto *openShortcut = new QShortcut(QKeySequence::Open, this);
    connect(openShortcut, &QShortcut::activated, this, &WidReview::chooseFile);

    // 十六进制表格分块填充器（封装定时器、重绘开关和信号屏蔽）：
    // 每次只填充一小批行，避免 Aurora 大包一次性创建数万个单元格
    // 导致界面卡顿。填充完成后统一重算列宽。
    m_hexFiller = new ChunkedTableFiller(ui->hexTable, this);
    connect(m_hexFiller,
            &ChunkedTableFiller::finished,
            this,
            &WidReview::updateHexColumnWidths);

    resetView();
}

WidReview::~WidReview()
{
    // 线程销毁顺序非常重要：
    //   1. 先设置取消标志，让导入循环在当前包边界退出；
    //   2. quit() 请求工作线程事件循环结束；
    //   3. wait() 等待 importFile() 真正返回，防止 QThread 运行中被释放。
    if (m_importThread != nullptr && m_importThread->isRunning()) {
        if (m_importer != nullptr) {
            m_importer->requestCancel();
        }
        m_importThread->quit();
        m_importThread->wait();
    }
    delete ui;
}

void WidReview::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    // 布局计算需要等当前 resize 事件处理完成后再读取 viewport 宽度，
    // 因此通过事件队列延迟到下一轮事件循环。
    QTimer::singleShot(0, this, &WidReview::updateHexColumnWidths);
}

void WidReview::configureTable()
{
    // 数据包列表的列定义由 m_packetColumns 决定，见
    // initializePacketTableColumns() 与三条总线的列定义接口。
    // 这里只配置与列定义无关的通用行为。
    // 表格只填充当前页，避免为 65,536 个包创建 65,536 行控件。
    auto *table = ui->packetTable;

    // 数据包元数据列表本身只读，数据修改在下方 hexTable 中进行。
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setSelectionMode(QAbstractItemView::SingleSelection);
    table->setShowGrid(false);
    table->setAlternatingRowColors(false);
    table->verticalHeader()->setVisible(false);
    table->verticalHeader()->setDefaultSectionSize(30);

    auto *header = table->horizontalHeader();
    header->setHighlightSections(false);
    header->setMinimumSectionSize(56);
    // 固定高度保证每页 10 包始终完整显示，不随窗口或最后一页包数伸缩。
    header->setFixedHeight(30);
    table->setFixedHeight(332);
}

void WidReview::initializePacketTableColumns()
{
    // ★ 默认列名修改位置 ★
    // 下面三个列表就是 FC / Aurora / 以太网 数据浏览表的默认列名，
    // 直接改列表内容即可；若需要在运行时按条件切换列名，
    // 到构造函数中标注“★ 修改数据浏览表列名的调用位置 ★”处添加调用。
    //
    // 三条总线的默认列定义。
    // 默认值同样通过三条总线的公开接口设置（busIndex：0=FC，1=Aurora，2=以太网），
    // 后续修改列名、列数量或新增“按字节/位取值”的列时，
    // 只要改这里的调用即可，.ui 与填充逻辑都不用动。
    //
    // FC / 以太网：包序号、文件偏移、数据长度、数据预览。
    const QList<ReviewColumn> fcColumns = {
        {QStringLiteral("包序号"), ReviewColumnField::Index},
        {QStringLiteral("文件偏移"), ReviewColumnField::Offset},
        {QStringLiteral("数据长度"), ReviewColumnField::PayloadLength},
        {QStringLiteral("数据预览"), ReviewColumnField::PayloadPreview}
    };

    // Aurora：额外显示包头和数据块 n。
    const QList<ReviewColumn> auroraColumns = {
        {QStringLiteral("包序号"), ReviewColumnField::Index},
        {QStringLiteral("文件偏移"), ReviewColumnField::Offset},
        {QStringLiteral("6 字节包头"), ReviewColumnField::Header},
        {QStringLiteral("数据块 n"), ReviewColumnField::DataCount},
        {QStringLiteral("数据长度"), ReviewColumnField::PayloadLength},
        {QStringLiteral("数据预览"), ReviewColumnField::PayloadPreview}
    };

    setFcPacketTableColumns(fcColumns);
    setAuroraPacketTableColumns(auroraColumns);
    setTcpPacketTableColumns(fcColumns);
}

// 设置 FC 数据浏览表的列定义（总线索引 0）。
// 只保存定义并按需刷新界面，具体刷新步骤在 applyPacketTableColumns() 中：
//   1. m_packetColumns[0] = columns（长度即列数，顺序即列顺序）；
//   2. 当前总线不是 FC 时到此结束，等切换到 FC 时再应用；
//   3. 当前正是 FC 时重建表头，并重填当前页、选中第一行。
// 列的取值方式见 ReviewColumn：内置字段或“字节/位 + 位置 + 长度”。
void WidReview::setFcPacketTableColumns(const QList<ReviewColumn> &columns)
{
    applyPacketTableColumns(0, columns);
}

// 设置 Aurora 数据浏览表的列定义（总线索引 1），行为与 FC 版本一致。
// Aurora 数据段长度 = n × 1024，自定义列的起始字节同样从数据段第 0 字节算起。
void WidReview::setAuroraPacketTableColumns(const QList<ReviewColumn> &columns)
{
    applyPacketTableColumns(1, columns);
}

// 设置以太网数据浏览表的列定义（总线索引 2），行为与 FC 版本一致。
// 以太网数据段长度 = n（n ≤ 1024），自定义列的位置同样以数据段为基准。
void WidReview::setTcpPacketTableColumns(const QList<ReviewColumn> &columns)
{
    applyPacketTableColumns(2, columns);
}

// 列定义设置的统一实现：
//   1. 校验总线索引，非法直接返回；
//   2. 保存该总线的列定义（列数 = 列表长度，顺序 = 表格列顺序）；
//   3. 若当前正显示该总线 → 重新应用表头（列名/列数）；
//   4. 已有数据时重新填充当前页并选中第一行，使新列立即生效。
void WidReview::applyPacketTableColumns(int busIndex,
                                        const QList<ReviewColumn> &columns)
{
    if (busIndex < 0 || busIndex >= 3) {
        return;
    }

    m_packetColumns[busIndex] = columns;

    // 如果当前正显示该总线，立即刷新表头；已有数据时重新填充当前页，
    // 使新列名/列数量马上生效。
    const int currentBusIndex = ui->busTypeCombo->currentIndex() - 1;
    if (currentBusIndex == busIndex) {
        if (busIndex == 0) {
            setPacketTableSchema(PacketTableSchema::FC);
        } else if (busIndex == 1) {
            setPacketTableSchema(PacketTableSchema::Aurora);
        } else {
            setPacketTableSchema(PacketTableSchema::Ethernet);
        }

        if (!m_packets.isEmpty()) {
            populateCurrentPage();
            selectFirstPacketOnPage();
        }
    }
}

QList<ReviewColumn> WidReview::packetTableColumns(int busIndex) const
{
    if (busIndex < 0 || busIndex >= 3) {
        return QList<ReviewColumn>();
    }
    return m_packetColumns[busIndex];
}

QString WidReview::packetCellText(int busIndex,
                                  int column,
                                  int row,
                                  const BinPacket &value) const
{
    // 行号预留给后续可能依赖行位置的列（例如序号、行号、时间轴等），
    // 当前各列都直接取自 BinPacket，因此这里不参与计算。
    Q_UNUSED(row)

    if (busIndex < 0 || busIndex >= 3) {
        return QString();
    }
    const QList<ReviewColumn> &columns = m_packetColumns[busIndex];
    if (column < 0 || column >= columns.size()) {
        return QString();
    }

    // ── 列 → 显示文本的统一映射 ──────────────────────────────────
    // 修改列的显示格式：改对应的 case；
    // 新增数据来源：在 ReviewColumnField 加枚举值并在此补一个 case；
    // 表格填充、翻页、保存后的刷新都会自动使用这里的结果。
    switch (columns.at(column).field) {
    case ReviewColumnField::Index:
        return QStringLiteral("%1")
            .arg(value.index + 1, 2, 10, QLatin1Char('0'));
    case ReviewColumnField::Offset:
        return formatHexOffset(value.offset);
    case ReviewColumnField::Header:
        return BinFormat::toHex(value.header);
    case ReviewColumnField::DataCount:
        return QString::number(value.dataCount);
    case ReviewColumnField::PayloadLength:
        return QStringLiteral("%1 B").arg(value.payloadSize());
    case ReviewColumnField::PayloadPreview: {
        // 预览只读取数据段前 12 字节，避免为大包加载整段数据。
        // 若该包正是当前正在编辑的包，则直接使用内存中的最新数据，
        // 保证保存后预览与文件内容一致。
        QByteArray previewBytes;
        if (value.index == m_editablePacketIndex
            && !m_editablePayload.isEmpty()) {
            previewBytes = m_editablePayload.left(12);
        } else {
            QFile file(m_currentFilePath);
            if (file.open(QIODevice::ReadOnly)
                && file.seek(value.offset + BinFormat::HeaderSize)) {
                previewBytes =
                    file.read(qMin<qint64>(12, value.payloadSize()));
            }
        }

        QString preview = BinFormat::toHex(previewBytes);
        if (value.payloadSize() > 12) {
            preview.append(QStringLiteral(" ..."));
        } else if (preview.isEmpty()) {
            preview = QStringLiteral("—");
        }
        return preview;
    }
    case ReviewColumnField::CustomData:
        // 自定义列：按列定义中的字节/位参数从数据段取值。
        return customColumnText(columns.at(column), value);
    }

    return QString();
}

// 自定义数据列取值实现。
// 列定义参数含义：
//   unit       Byte=按字节，Bit=按位；
//   startByte  该行数据段内的起始字节（0 表示数据段第 1 个字节）；
//   startBit   起始字节内的起始位（0=最高位 bit7），仅 Bit 模式使用；
//   byteLength 字节长（1~8），仅 Byte 模式使用；
//   bitLength  位长（1~64），仅 Bit 模式使用。
// 实际读取位置 = 包 offset + 6 字节包头 + startByte。
QString WidReview::customColumnText(const ReviewColumn &column,
                                    const BinPacket &value) const
{
    if (column.startByte < 0) {
        return QString();
    }

    const bool bitMode = column.unit == ReviewDataUnit::Bit;
    // 参数归一化：位模式下按位长换算需要读取的字节数（可能跨字节），
    // 字节模式最多读 8 字节，位模式最多 64 位。
    const int startBit = qBound(0, column.startBit, 7);
    const int bitLength = qBound(1, column.bitLength, 64);
    const int byteLength = qBound(1, column.byteLength, 8);
    const int neededBytes =
        bitMode ? (startBit + bitLength + 7) / 8 : byteLength;

    // 越界判断：所需字节必须落在该包数据段内。
    if (column.startByte + neededBytes > value.payloadSize()) {
        return QString();
    }

    // 取数据：当前正在编辑的包直接用内存中的最新内容，
    // 其它包按需从文件的“包头之后 + 起始字节”位置读取。
    QByteArray bytes;
    if (value.index == m_editablePacketIndex && !m_editablePayload.isEmpty()) {
        bytes = m_editablePayload.mid(column.startByte, neededBytes);
    }
    // 内存缓存不足（数据段按 64 KB 截断）或不是当前编辑包时，回退到文件读取。
    if (bytes.size() < neededBytes) {
        QFile file(m_currentFilePath);
        if (file.open(QIODevice::ReadOnly)
            && file.seek(value.offset + BinFormat::HeaderSize
                         + column.startByte)) {
            bytes = file.read(neededBytes);
        }
    }
    if (bytes.size() < neededBytes) {
        return QString();
    }

    quint64 result = 0;

    if (!bitMode) {
        // 字节模式：按项目统一的大小端宏组合成一个整数。
#if BIN_HEADER_BIG_ENDIAN
        for (const char byte : bytes) {
            result = (result << 8) | static_cast<quint8>(byte);
        }
#else
        for (int index = 0; index < bytes.size(); ++index) {
            result |= static_cast<quint64>(
                          static_cast<quint8>(bytes.at(index)))
                << (8 * index);
        }
#endif
        return QString::number(result);
    }

    // 位模式：从起始字节的起始位开始，按高位在前的顺序逐位拼接。
    for (int index = 0; index < bitLength; ++index) {
        const int globalBit = startBit + index;
        const int byteIndex = globalBit / 8;
        const int bitInByte = globalBit % 8;
        const quint8 byte =
            static_cast<quint8>(bytes.at(byteIndex));
        const quint64 bit = (byte >> (7 - bitInByte)) & 0x01;
        result = (result << 1) | bit;
    }
    return QString::number(result);
}

void WidReview::setPacketTableSchema(PacketTableSchema schema)
{
    auto *table = ui->packetTable;

    int busIndex = -1;
    if (schema == PacketTableSchema::FC) {
        busIndex = 0;
    } else if (schema == PacketTableSchema::Aurora) {
        busIndex = 1;
    } else if (schema == PacketTableSchema::Ethernet) {
        busIndex = 2;
    }

    // Empty 或非法状态：清空列，等待用户选择总线类型。
    if (busIndex < 0 || m_packetColumns[busIndex].isEmpty()) {
        table->setColumnCount(0);
        return;
    }

    // 列名和列数量完全来自该总线的列定义，
    // 修改列定义后无需再改本函数。
    const QList<ReviewColumn> &columns = m_packetColumns[busIndex];
    QStringList titles;
    titles.reserve(columns.size());
    for (const ReviewColumn &column : columns) {
        titles.append(column.title);
    }

    table->setColumnCount(columns.size());
    table->setHorizontalHeaderLabels(titles);

    // 数据预览列占满剩余宽度，其余列按内容自适应。
    auto *header = table->horizontalHeader();
    for (int index = 0; index < columns.size(); ++index) {
        header->setSectionResizeMode(
            index,
            columns.at(index).field == ReviewColumnField::PayloadPreview
                ? QHeaderView::Stretch
                : QHeaderView::ResizeToContents);
    }
}

void WidReview::configureHexTable()
{
    // 列 0 显示行偏移，列 1-16 分别对应 00-0F 字节。
    // 表头保持为字节位置，不随 16/10 进制切换而变化。
    auto *table = ui->hexTable;
    QStringList headers;
    headers << QStringLiteral("偏移");
    for (int column = 0; column < 16; ++column) {
        headers << QStringLiteral("%1")
                   .arg(column, 2, 16, QLatin1Char('0'))
                   .toUpper();
    }

    table->setColumnCount(headers.size());
    table->setHorizontalHeaderLabels(headers);
    table->verticalHeader()->setVisible(false);
    table->verticalHeader()->setDefaultSectionSize(24);
    // 支持双击编辑、按 F2/Enter 编辑以及选中单元格后直接输入。
    table->setEditTriggers(QAbstractItemView::DoubleClicked
                           | QAbstractItemView::EditKeyPressed
                           | QAbstractItemView::AnyKeyPressed);
    table->setSelectionMode(QAbstractItemView::SingleSelection);
    table->setSelectionBehavior(QAbstractItemView::SelectItems);
    table->setShowGrid(true);
    table->setAlternatingRowColors(false);
    // 固定滚动条策略，避免滚动条的出现/消失改变可用宽度。
    table->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
    table->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    auto *header = table->horizontalHeader();
    header->setHighlightSections(false);
    // 16 个数据列不使用 Stretch，而是在 updateHexColumnWidths() 中
    // 根据实际 viewport 宽度计算固定宽度，防止 0F 列被滚动条压窄。
    header->setStretchLastSection(false);
    header->setMinimumSectionSize(28);
    header->setSectionResizeMode(0, QHeaderView::Fixed);
    table->setColumnWidth(0, 82);
    for (int column = 1; column < table->columnCount(); ++column) {
        header->setSectionResizeMode(column, QHeaderView::Fixed);
    }
    header->setFixedHeight(28);
}

void WidReview::updateHexColumnWidths()
{
    if (ui->hexTable->columnCount() != 17) {
        return;
    }

    // viewport 已排除竖向滚动条宽度；再减去固定偏移列，就是 16 列可用空间。
    auto *header = ui->hexTable->horizontalHeader();
    const int dataAreaWidth =
        ui->hexTable->viewport()->width() - header->sectionSize(0);
    // 设置最小宽度 28，避免窗口过窄时列宽变成 0 或负数。
    const int columnWidth = qMax(28, dataAreaWidth / 16);
    for (int column = 1; column < 17; ++column) {
        header->resizeSection(column, columnWidth);
    }
}

void WidReview::chooseFile()
{
    // 以最近一次成功导入的目录作为起始位置。
    // 对话框返回空字符串代表用户取消，不改变当前文件。
    const QString filePath = QFileDialog::getOpenFileName(
        this,
        QStringLiteral("选择 BIN 文件"),
        m_lastDirectory,
        QStringLiteral("BIN 文件 (*.bin);;所有文件 (*.*)"));

    if (!filePath.isEmpty()) {
        loadFile(filePath);
    }
}

bool WidReview::loadFile(const QString &filePath)
{
    // 同一时间只允许一个后台导入任务。
    // 导入期间的“清空当前文件”按钮会临时变为“取消导入”。
    if (m_isImporting) {
        setReviewStatus(QStringLiteral("当前文件仍在导入，请等待完成或取消导入。"));
        return false;
    }

    const QFileInfo info(filePath);

    // 这里只做 O(1) 的快速校验；真正的包头格式检查由后台线程完成。
    if (!info.exists() || !info.isFile()) {
        resetView();
        showLoadError(QStringLiteral("所选文件不存在，或无法作为本地文件读取。"));
        return false;
    }

    if (info.suffix().compare(QStringLiteral("bin"), Qt::CaseInsensitive) != 0) {
        resetView();
        showLoadError(QStringLiteral("文件类型不正确，请选择扩展名为 .bin 的文件。"));
        return false;
    }

    // 选择文件时只记录路径和文件信息，不立即读取数据。
    // 真正解析必须等待用户选择 Aurora 总线类型。
    resetView();
    applySelectedFileInfo(info.absoluteFilePath());
    m_busType = BusType::None;

    {
        // resetView 已将下拉框重置为“请选择”；这里只恢复可用状态。
        QSignalBlocker blocker(ui->busTypeCombo);
        ui->busTypeCombo->setCurrentIndex(0);
        ui->busTypeCombo->setEnabled(true);
    }

    setStatus(QStringLiteral("等待选择总线类型"), IdleState);
    setFileState(QStringLiteral("文件已选择 · 请选择总线类型"), IdleState);
    setReviewStatus(
        QStringLiteral("已选择文件：%1").arg(QDir::toNativeSeparators(info.absoluteFilePath())));
    return true;
}

void WidReview::applySelectedFileInfo(const QString &filePath)
{
    const QFileInfo info(filePath);
    // ---------------------------------------------------------------------
    // 文件信息赋值区
    // m_selectedFilePath / m_selectedFileSize 会参与后续解析；
    // 下方 fileNameValue、filePathValue 等只负责界面显示。
    // 需要调整界面信息展示时，集中修改这一组值即可。
    // ---------------------------------------------------------------------
    m_selectedFilePath = info.absoluteFilePath();
    m_selectedFileSize = info.size();
    m_lastDirectory = info.absolutePath();

    ui->fileNameValue->setText(info.fileName());
    ui->fileNameValue->setToolTip(info.fileName());
    ui->fileSizeValue->setText(formatFileSize(info.size()));
    ui->fileSizeSummaryValue->setText(formatFileSize(info.size()));
    ui->filePathValue->setText(QDir::toNativeSeparators(info.absolutePath()));
    ui->filePathValue->setToolTip(QDir::toNativeSeparators(info.absoluteFilePath()));
    ui->currentFileLabel->setText(QStringLiteral("当前文件：%1").arg(info.fileName()));
    ui->currentFileLabel->setToolTip(QDir::toNativeSeparators(info.absoluteFilePath()));
    ui->dropTitle->setText(info.fileName());
    ui->dropHint->setText(QStringLiteral("已选择文件，请选择总线类型"));
    ui->clearButton->setEnabled(true);
    ui->busTypeCombo->setEnabled(true);
}

void WidReview::handleBusTypeChanged(int index)
{
    if (m_isImporting || m_selectedFilePath.isEmpty()) {
        return;
    }

    BusType selectedType = BusType::None;
    if (index == 1) {
        selectedType = BusType::FC;
    } else if (index == 2) {
        selectedType = BusType::Aurora;
    } else if (index == 3) {
        selectedType = BusType::Ethernet;
    }

    m_busType = selectedType;

    // Aurora 使用独立协议；FC 暂时复用 TCP 解析规则。
    if (selectedType == BusType::Aurora) {
        beginImport(m_selectedFilePath,
                    BinFormat::Protocol::Aurora,
                    BusType::Aurora);
        return;
    }

    if (selectedType == BusType::Ethernet) {
        beginImport(m_selectedFilePath,
                    BinFormat::Protocol::TCP,
                    BusType::Ethernet);
        return;
    }

    if (selectedType == BusType::FC) {
        beginImport(m_selectedFilePath,
                    BinFormat::Protocol::TCP,
                    BusType::FC);
        return;
    }

    // 尚未选择总线类型时只保留文件路径和信息，不读取文件数据。
    const QString selectedPath = m_selectedFilePath;
    const qint64 selectedSize = m_selectedFileSize;
    resetView();
    m_selectedFilePath = selectedPath;
    m_selectedFileSize = selectedSize;
    m_busType = selectedType;
    applySelectedFileInfo(selectedPath);

    setPacketTableSchema(PacketTableSchema::Empty);

    {
        QSignalBlocker blocker(ui->busTypeCombo);
        ui->busTypeCombo->setCurrentIndex(index);
        ui->busTypeCombo->setEnabled(true);
    }

    setStatus(QStringLiteral("等待选择总线类型"), IdleState);
    setFileState(QStringLiteral("文件已选择 · 请选择总线类型"), IdleState);
    setReviewStatus(QStringLiteral("请选择总线类型后再执行数据解析。"));
}

void WidReview::beginImport(const QString &filePath,
                            BinFormat::Protocol protocol,
                            BusType busType)
{
    const QFileInfo selectedInfo(filePath);
    const QString selectedPath = selectedInfo.absoluteFilePath();
    const qint64 selectedSize = selectedInfo.size();

    // resetView() 先清除上一个文件的结果，避免新文件扫描期间误操作旧数据。
    resetView();
    if (busType == BusType::Aurora) {
        setPacketTableSchema(PacketTableSchema::Aurora);
    } else if (busType == BusType::FC) {
        setPacketTableSchema(PacketTableSchema::FC);
    } else {
        setPacketTableSchema(PacketTableSchema::Ethernet);
    }

    m_selectedFilePath = selectedPath;
    m_selectedFileSize = selectedSize;
    m_busType = busType;
    applySelectedFileInfo(selectedPath);

    {
        QSignalBlocker blocker(ui->busTypeCombo);
        int comboIndex = 3;
        if (busType == BusType::FC) {
            comboIndex = 1;
        } else if (busType == BusType::Aurora) {
            comboIndex = 2;
        }
        ui->busTypeCombo->setCurrentIndex(comboIndex);
    }

    m_isImporting = true;

    // generation 用于识别本轮导入发出的信号。
    // 如果未来允许连续快速切换文件，旧线程的迟到信号会被忽略。
    const quint64 generation = ++m_importGeneration;

    // 在导入开始前即展示文件名、文件大小和目录，让用户知道当前任务。
    const QFileInfo info(selectedPath);
    ui->dropTitle->setText(QStringLiteral("正在导入文件"));
    ui->dropHint->setText(info.fileName());

    setImportingUi(true);
    m_importProgress->setValue(0);
    setStatus(QStringLiteral("正在导入"), IdleState);
    setFileState(QStringLiteral("正在后台解析包头…"), IdleState);
    setReviewStatus(QStringLiteral("正在导入：%1").arg(info.fileName()));

    // ---------------------------------------------------------------------
    // 创建后台线程（解包算法入口，调用链第 ③ 步）
    //
    // 根据协议创建独立导入器，并 moveToThread() 转移线程亲和性。
    // 所有文件读取和包头解析都在工作线程中执行，主线程保持响应。
    //
    // 需要修改解包算法时：
    //   - 只改对应导入器 .cpp 里的 importFile()，本函数无需改动；
    //   - Aurora 的包头/数据段公式 → aurorafileimporter.cpp
    //   - FC    的包头/数据段公式 → fcfileimporter.cpp
    //   - 以太网 的包头/数据段公式 → tcpfileimporter.cpp
    //   - 包长常量（HeaderSize / PayloadSize）与字节序 → binformat.h
    // 导入器解析完只发出 finished(QVector<BinPacket>, 路径, 文件大小)，
    // 界面侧由 handleImportFinished() 接收，两者通过 BinPacket 解耦。
    // ---------------------------------------------------------------------
    auto *thread = new QThread(this);
    BinFileImporterBase *importer = nullptr;
    if (protocol == BinFormat::Protocol::Aurora) {
        importer = new AuroraFileImporter;
    } else if (busType == BusType::FC) {
        // FC 当前使用与 TCP 相同的规则，但由独立的 FcFileImporter 解析。
        importer = new FcFileImporter;
    } else {
        importer = new TcpFileImporter;
    }
    importer->moveToThread(thread);
    m_importThread = thread;
    m_importer = importer;

    // 线程事件循环启动后，再执行阻塞式 importFile()。
    connect(thread, &QThread::started, importer, [importer, filePath]() {
        importer->importFile(filePath);
    });
    // 导入进度、成功、失败、取消信号都以 queued connection 回到主线程。
    connect(importer,
            &BinFileImporterBase::progress,
            this,
            [this, generation](qint64 processedBytes, qint64 totalBytes, int packetCount) {
                if (generation == m_importGeneration) {
                    handleImportProgress(processedBytes, totalBytes, packetCount);
                }
            });
    connect(importer,
            &BinFileImporterBase::finished,
            this,
            [this, generation](const QVector<BinPacket> &packets,
                               const QString &importedPath,
                               qint64 fileSize) {
                if (generation == m_importGeneration) {
                    handleImportFinished(packets, importedPath, fileSize);
                }
            });
    connect(importer,
            &BinFileImporterBase::failed,
            this,
            [this, generation](const QString &message) {
                if (generation == m_importGeneration) {
                    handleImportFailed(message);
                }
            });
    connect(importer,
            &BinFileImporterBase::cancelled,
            this,
            [this, generation]() {
                if (generation == m_importGeneration) {
                    handleImportCancelled();
                }
            });

    // 任意终态都结束工作线程事件循环。
    // worker 的 deleteLater 必须连接在 QThread::finished 上，
    // 这样对象会在其所属线程停止前被安全释放。
    connect(importer, &BinFileImporterBase::finished, thread, &QThread::quit);
    connect(importer, &BinFileImporterBase::failed, thread, &QThread::quit);
    connect(importer, &BinFileImporterBase::cancelled, thread, &QThread::quit);
    connect(thread, &QThread::finished, importer, &QObject::deleteLater);
    // 线程完全结束后再清除指针并恢复所有交互控件。
    /* 说明：
     * 不能在收到 finished(data) 时立即允许下一次导入，因为此时 QThread
     * 可能仍在执行退出流程。以 QThread::finished 为最终状态更安全。
     */
    connect(thread, &QThread::finished, this, [this, thread, generation]() {
        if (generation == m_importGeneration) {
            m_importThread = nullptr;
            m_importer = nullptr;
            m_isImporting = false;
            setImportingUi(false);
            updatePagination();
        }
        thread->deleteLater();
    });

    thread->start();
}

void WidReview::setImportingUi(bool importing)
{
    // 导入期间禁用依赖文件内容的操作，避免对空数据集执行翻页或保存。
    // “清空当前文件”仍保持可用，但文本切换为“取消导入”。
    ui->chooseButton->setEnabled(!importing);
    ui->packetTable->setEnabled(!importing);
    ui->hexTable->setEnabled(!importing);
    ui->displayModeCombo->setEnabled(
        !importing && m_editablePacketIndex >= 0 && !m_editablePayload.isEmpty());
    ui->busTypeCombo->setEnabled(!importing && !m_selectedFilePath.isEmpty());
    ui->savePacketButton->setEnabled(!importing && m_hexDirty);
    ui->pageJumpSpinBox->setEnabled(!importing && !m_packets.isEmpty());
    ui->jumpPageButton->setEnabled(!importing && !m_packets.isEmpty());
    ui->clearButton->setEnabled(
        importing || !m_packets.isEmpty() || !m_selectedFilePath.isEmpty());
    ui->clearButton->setText(
        importing ? QStringLiteral("取消导入") : QStringLiteral("清空当前文件"));

    if (importing) {
        ui->prevPageButton->setEnabled(false);
        ui->nextPageButton->setEnabled(false);
        m_importProgress->show();
    } else {
        m_importProgress->hide();
    }
}

void WidReview::handleImportProgress(qint64 processedBytes,
                                      qint64 totalBytes,
                                      int packetCount)
{
    // processedBytes 和 totalBytes 都是文件字节偏移，不使用包数计算百分比，
    // 因为每个包的数据段长度可能不同。
    const int percent = totalBytes > 0
                            ? static_cast<int>(processedBytes * 100 / totalBytes)
                            : 0;
    m_importProgress->setValue(percent);
    ui->fileStateLabel->setText(
        QStringLiteral("正在后台解析 · 已识别 %1 包").arg(packetCount));
    setReviewStatus(
        QStringLiteral("正在解析 %1% · 已识别 %2 包 · %3 / %4")
            .arg(percent)
            .arg(packetCount)
            .arg(formatFileSize(processedBytes))
            .arg(formatFileSize(totalBytes)));
}

//接收工作线程解包的值
void WidReview::handleImportFinished(const QVector<BinPacket> &packets,
                                      const QString &filePath,
                                      qint64 fileSize)
{
    // 解包结果接收点（调用链第 ⑤ 步）：
    // 工作线程只返回元数据列表，界面不关心它是如何算出来的；
    // 只要导入器按 BinPacket 结构填充字段，本函数及后续表格显示都无需改动。
    // 把结果移交给主窗口并更新所有摘要信息。
    // QVector 是隐式共享容器，此处赋值不会复制每个 BinPacket 的堆数据。
    const QFileInfo info(filePath);
    // m_packets 是分页、表格行和数据详情共用的唯一包元数据来源。
    m_packets = packets;
    m_currentFilePath = filePath;
    m_lastDirectory = info.absolutePath();
    m_currentPage = 0;

    // 导入完成后的摘要赋值区：文件名、文件大小、包数和路径。
    ui->fileNameValue->setText(info.fileName());
    ui->fileSizeValue->setText(formatFileSize(fileSize));
    ui->packetCountValue->setText(QString::number(m_packets.size()));
    ui->packetCountBadge->setText(QString::number(m_packets.size()));
    ui->fileSizeSummaryValue->setText(formatFileSize(fileSize));
    ui->filePathValue->setText(QDir::toNativeSeparators(info.absolutePath()));
    ui->filePathValue->setToolTip(QDir::toNativeSeparators(filePath));
    ui->dropTitle->setText(QStringLiteral("文件已载入"));
    ui->dropHint->setText(QStringLiteral("可将新的 .bin 文件拖到窗口中替换"));
    m_importProgress->setValue(100);

    // 导入完成后默认展示第一页，并自动选中第一包。
    populateCurrentPage();
    setStatus(QStringLiteral("解析完成"), SuccessState);
    setFileState(
        QStringLiteral("格式校验通过 · 共 %1 个数据包").arg(m_packets.size()),
        SuccessState);
    setReviewStatus(
        QStringLiteral("已载入 %1 个数据包  |  %2")
            .arg(m_packets.size())
            .arg(QDir::toNativeSeparators(filePath)));

    selectFirstPacketOnPage();
}

void WidReview::handleImportFailed(const QString &message)
{
    // 失败时清除半成品状态，再通过模态对话框显示具体错误原因。
    m_importProgress->hide();
    resetView();
    showLoadError(message);
}

void WidReview::handleImportCancelled()
{
    // 取消不是错误，因此界面回到空状态并显示普通提示。
    m_importProgress->hide();
    resetView();
    setStatus(QStringLiteral("已取消"), IdleState);
    setFileState(QStringLiteral("导入已取消"), IdleState);
    setReviewStatus(QStringLiteral("导入已取消"));
}

void WidReview::clearFile()
{
    // 导入中点击“清空当前文件”等价于取消后台任务。
    // 再次点击前禁用按钮，避免重复发出取消请求。
    if (m_isImporting) {
        if (m_importer != nullptr) {
            m_importer->requestCancel();
        }
        ui->clearButton->setEnabled(false);
        setStatus(QStringLiteral("正在取消"), IdleState);
        setReviewStatus(QStringLiteral("正在取消导入…"));
        return;
    }
    resetView();
}

void WidReview::showPreviousPage()
{
    // 第一页没有上一页；按钮正常情况下已禁用，这里再做一次防御检查。
    if (m_currentPage <= 0) {
        return;
    }

    --m_currentPage;
    // 每次翻页都只重建 10 行，并默认选中新页第一包。
    populateCurrentPage();

    selectFirstPacketOnPage();
}

void WidReview::showNextPage()
{
    // 向上取整得到总页数，例如 20 包对应 2 页。
    const int pageCount =
        (m_packets.size() + BinFormat::PacketsPerPage - 1) / BinFormat::PacketsPerPage;
    if (m_currentPage + 1 >= pageCount) {
        return;
    }

    ++m_currentPage;
    populateCurrentPage();

    selectFirstPacketOnPage();
}

void WidReview::jumpToPage()
{
    if (m_isImporting || m_packets.isEmpty()) {
        return;
    }

    // 页码输入框从 1 开始，内部索引从 0 开始。
    const int pageCount =
        (m_packets.size() + BinFormat::PacketsPerPage - 1) / BinFormat::PacketsPerPage;
    // qBound 防止手输或程序状态变化造成页码越界。
    const int targetPage =
        qBound(1, ui->pageJumpSpinBox->value(), pageCount) - 1;

    if (targetPage == m_currentPage) {
        ui->pageJumpSpinBox->setValue(m_currentPage + 1);
        return;
    }

    m_currentPage = targetPage;
    populateCurrentPage();

    selectFirstPacketOnPage();
}

bool WidReview::setDataValue(const QString &value, int row, int column)
{
    if (row < 0 || row >= ui->packetTable->rowCount()
        || column < 0 || column >= ui->packetTable->columnCount()) {
        return false;
    }

    QTableWidgetItem *item = ui->packetTable->item(row, column);
    if (item == nullptr) {
        return false;
    }

    // 只修改数据浏览表的显示值，不直接改动 m_packets 或 BIN 文件。
    item->setText(value);
    return true;
}

QString WidReview::formatDisplayByte(quint8 value) const
{
    // 10 进制不带前导零，方便直接阅读；
    // 16 进制固定两位，便于按列对齐。
    if (m_numberBase == NumberBase::Decimal) {
        return QString::number(value);
    }

    return QStringLiteral("%1")
        .arg(value, 2, 16, QLatin1Char('0'))
        .toUpper();
}

void WidReview::refreshHexTableValues()
{
    if (m_editablePayload.isEmpty()) {
        return;
    }

    // 切换进制只重写显示文本，不修改 m_editablePayload，
    // 因此不会产生“未保存修改”状态。
    m_populatingHexTable = true;
    QSignalBlocker blocker(ui->hexTable);
    for (int row = 0; row < ui->hexTable->rowCount(); ++row) {
        for (int column = 1; column < ui->hexTable->columnCount(); ++column) {
            const int byteIndex = row * 16 + (column - 1);
            if (byteIndex >= m_editablePayload.size()) {
                continue;
            }
            if (QTableWidgetItem *item = ui->hexTable->item(row, column)) {
                item->setText(formatDisplayByte(
                    static_cast<quint8>(m_editablePayload.at(byteIndex))));
            }
        }
    }
    m_populatingHexTable = false;
}

void WidReview::changeNumberBase(int index)
{
    // combo 的第 0 项为 16 进制，第 1 项为 10 进制。
    m_numberBase = index == 1 ? NumberBase::Decimal : NumberBase::Hexadecimal;
    refreshHexTableValues();
    setReviewStatus(
        QStringLiteral("数据段已切换为%1显示。")
            .arg(m_numberBase == NumberBase::Decimal
                     ? QStringLiteral("10进制")
                     : QStringLiteral("16进制")));
}

void WidReview::handleHexItemChanged(QTableWidgetItem *item)
{
    // 批量填充或恢复文本时忽略信号，防止递归触发。
    if (m_populatingHexTable || item == nullptr || item->column() == 0) {
        return;
    }

    // 表格第 0 列是偏移，第 1-16 列对应每行 16 个字节。
    // 因此字节索引 = 行号 × 16 + (列号 - 1)。
    const int byteIndex = item->row() * 16 + (item->column() - 1);
    if (byteIndex < 0 || byteIndex >= m_editablePayload.size()) {
        return;
    }

    QString text = item->text().trimmed();
    bool ok = false;
    int value = 0;

    // 根据当前显示模式使用不同进制解析输入。
    // 10 进制范围 0-255；16 进制范围 00-FF，并兼容 0x 前缀。
    if (m_numberBase == NumberBase::Decimal) {
        value = text.toInt(&ok, 10);
        if (!ok || text.isEmpty() || text.size() > 3
            || value < 0 || value > 255) {
            ok = false;
        }
    } else {
        if (text.startsWith(QStringLiteral("0x"), Qt::CaseInsensitive)) {
            text = text.mid(2);
        }
        value = text.toInt(&ok, 16);
        if (!ok || text.isEmpty() || text.size() > 2
            || value < 0 || value > 0xFF) {
            ok = false;
        }
    }

    // 输入非法时不修改底层字节，直接把单元格恢复成原值。
    if (!ok) {
        m_populatingHexTable = true;
        {
            QSignalBlocker blocker(ui->hexTable);
            item->setText(formatDisplayByte(
                static_cast<quint8>(m_editablePayload.at(byteIndex))));
        }
        m_populatingHexTable = false;
        setReviewStatus(
            m_numberBase == NumberBase::Decimal
                ? QStringLiteral("数据格式无效，请输入 0-255 范围内的十进制数值。")
                : QStringLiteral("数据格式无效，请输入 00-FF 范围内的十六进制字节。"));
        return;
    }

    m_populatingHexTable = true;
    {
        QSignalBlocker blocker(ui->hexTable);
        item->setText(formatDisplayByte(static_cast<quint8>(value)));
        if (value == static_cast<quint8>(m_originalPayload.at(byteIndex))) {
            item->setBackground(QBrush());
        } else {
            item->setBackground(QBrush(QColor(255, 242, 196)));
        }
    }
    m_populatingHexTable = false;

    // 先更新内存缓冲区，再与文件原始内容比较来确定脏状态。
    // 即使用户改回原值，保存按钮也会重新禁用。
    m_editablePayload[byteIndex] = static_cast<char>(value);
    m_hexDirty = m_editablePayload != m_originalPayload;
    ui->savePacketButton->setEnabled(m_hexDirty && !m_isImporting);
    ui->fileStateLabel->setText(
        m_hexDirty ? QStringLiteral("有未保存的数据修改")
                   : QStringLiteral("数据修改已撤销"));
    setReviewStatus(
        m_hexDirty ? QStringLiteral("数据已修改，点击“保存到文件”写回 BIN 文件。")
                   : QStringLiteral("数据已恢复为文件中的原始内容。"));
}

void WidReview::saveCurrentPacket()
{
    // 只有当前数据段存在未保存修改时才允许写入。
    if (!m_hexDirty || m_isImporting || m_editablePacketIndex < 0
        || m_editablePacketIndex >= m_packets.size() || m_currentFilePath.isEmpty()) {
        return;
    }

    // 保存采用原位覆盖，不重写整个文件：
    //   seek 到该包 offset + 6（跳过包头），
    //   只写回当前表格已加载的数据段前缀。
    const BinPacket &packet = m_packets.at(m_editablePacketIndex);
    QFile file(m_currentFilePath);
    if (!file.open(QIODevice::ReadWrite)) {
        QMessageBox::warning(
            this,
            QStringLiteral("保存失败"),
            QStringLiteral("无法以读写方式打开 BIN 文件：%1").arg(file.errorString()));
        return;
    }

    // 检查 seek、完整写入和 flush 三个步骤，任一失败都保留脏状态。
    if (!file.seek(packet.offset + BinFormat::HeaderSize)
        || file.write(m_editablePayload) != m_editablePayload.size()
        || !file.flush()) {
        QMessageBox::warning(
            this,
            QStringLiteral("保存失败"),
            QStringLiteral("写入数据段失败：%1").arg(file.errorString()));
        return;
    }

    // 写盘成功后更新“原始内容”基准，并清除所有黄色修改标记。
    m_originalPayload = m_editablePayload;
    m_hexDirty = false;
    ui->savePacketButton->setEnabled(false);

    m_populatingHexTable = true;
    QSignalBlocker blocker(ui->hexTable);
    for (int row = 0; row < ui->hexTable->rowCount(); ++row) {
        for (int column = 1; column < ui->hexTable->columnCount(); ++column) {
            if (QTableWidgetItem *item = ui->hexTable->item(row, column)) {
                item->setBackground(QBrush());
            }
        }
    }
    m_populatingHexTable = false;

    updatePacketPreviewRow();
    setFileState(QStringLiteral("修改已保存到 BIN 文件"), SuccessState);
    setReviewStatus(
        QStringLiteral("第 %1 包数据段已保存  |  %2")
            .arg(packet.index + 1)
            .arg(QDir::toNativeSeparators(m_currentFilePath)));
}

void WidReview::loadHexPayload(int packetIndex)
{
    // 取消上一次可能仍在进行的分块填充，避免旧任务继续写表格。
    if (m_hexFiller != nullptr) {
        m_hexFiller->cancel();
    }

    // 重建表格期间阻止 itemChanged，避免每个单元格触发一次业务逻辑。
    m_populatingHexTable = true;
    QSignalBlocker blocker(ui->hexTable);

    // 删除旧行前先关闭重绘：当 hexTable 中已有数千行（Aurora 大包约
    // 4096 行 × 17 列）时，setRowCount(0) 释放单元格期间若保持重绘，
    // Qt 会反复重绘空表，取消导入、切换数据包时会出现明显卡顿。
    ui->hexTable->setUpdatesEnabled(false);
    // 直接 setRowCount(0) 让 Qt 批量释放旧行及其单元格，
    // 比 clearContents() + setRowCount(0) 更快，翻页时可减少明显停顿。
    ui->hexTable->setRowCount(0);
    ui->displayModeCombo->setEnabled(false);
    ui->savePacketButton->setEnabled(false);
    m_originalPayload.clear();
    m_editablePayload.clear();
    m_editablePacketIndex = packetIndex;
    m_hexDirty = false;

    // 无选中包时保持空表格，并禁用进制切换和保存按钮。
    if (packetIndex < 0 || packetIndex >= m_packets.size()) {
        m_populatingHexTable = false;
        ui->hexTable->setUpdatesEnabled(true);
        return;
    }

    // 按需读取当前包的数据段，单次上限 64 KB。
    // 1 GB 文件不会因为选中某个包而整体载入内存。
    const BinPacket &packet = m_packets.at(packetIndex);
    const qint64 previewSize = qMin(packet.payloadSize(), MaxHexPreviewBytes);
    if (previewSize > 0) {
        QFile file(m_currentFilePath);
        if (file.open(QIODevice::ReadOnly)
            && file.seek(packet.offset + BinFormat::HeaderSize)) {
            m_editablePayload = file.read(previewSize);
        }
    }

    // 保存一份原始副本，用于判断用户是否真正修改了数据。
    m_originalPayload = m_editablePayload;
    const int rowCount = (m_editablePayload.size() + 15) / 16;
    m_populatingHexTable = false;

    ui->displayModeCombo->setEnabled(!m_editablePayload.isEmpty()
                                     && !m_isImporting);

    if (rowCount > 0 && m_hexFiller != nullptr) {
        // 数据段字节已经完整读入内存（编辑/保存都基于 m_editablePayload），
        // 表格单元格的创建交给分块填充器异步完成，
        // Aurora 单包 64 KB 也不会再阻塞界面。
        m_hexFiller->start(
            rowCount,
            [this](int row) -> QList<QTableWidgetItem *> {
                QList<QTableWidgetItem *> items;

                auto *offsetItem = new QTableWidgetItem(
                    QStringLiteral("%1")
                        .arg(row * 16, 8, 16, QLatin1Char('0'))
                        .toUpper());
                offsetItem->setFlags(Qt::ItemIsEnabled
                                     | Qt::ItemIsSelectable);
                offsetItem->setTextAlignment(Qt::AlignCenter);
                items.append(offsetItem);

                for (int column = 0; column < 16; ++column) {
                    const int byteIndex = row * 16 + column;
                    auto *item = new QTableWidgetItem;
                    item->setTextAlignment(Qt::AlignCenter);
                    // 最后一行不足 16 字节时，多余单元格设为不可编辑。
                    if (byteIndex < m_editablePayload.size()) {
                        item->setText(formatDisplayByte(
                            static_cast<quint8>(
                                m_editablePayload.at(byteIndex))));
                        item->setFlags(Qt::ItemIsEnabled
                                       | Qt::ItemIsSelectable
                                       | Qt::ItemIsEditable);
                    } else {
                        item->setFlags(Qt::NoItemFlags);
                    }
                    items.append(item);
                }
                return items;
            });
    } else {
        // 没有可填充的行（例如清空数据/取消导入）时，立即恢复重绘，
        // 让空表格马上显示出来。
        ui->hexTable->setUpdatesEnabled(true);
    }
}

void WidReview::updatePacketPreviewRow()
{
    // 保存后只刷新当前页中对应数据包的前 12 字节预览，
    // 不重新构建整个分页，避免破坏用户当前的滚动和选择状态。
    const int row = m_editablePacketIndex - m_currentPage * BinFormat::PacketsPerPage;
    if (row < 0 || row >= ui->packetTable->rowCount()
        || m_editablePacketIndex < 0
        || m_editablePacketIndex >= m_packets.size()) {
        return;
    }

    // 预览列的位置由当前总线的列定义决定，不再写死列号；
    // 显示文本统一由 packetCellText() 生成，保证与翻页填充一致。
    const int busIndex = ui->busTypeCombo->currentIndex() - 1;
    const QList<ReviewColumn> columns = packetTableColumns(busIndex);
    const BinPacket &packet = m_packets.at(m_editablePacketIndex);

    for (int column = 0; column < columns.size(); ++column) {
        if (columns.at(column).field != ReviewColumnField::PayloadPreview) {
            continue;
        }
        if (QTableWidgetItem *item = ui->packetTable->item(row, column)) {
            item->setText(packetCellText(busIndex, column, row, packet));
        }
        break;
    }
}
//设置包列表的值
void WidReview::populateCurrentPage()
{
    // 重建当前页期间屏蔽选择信号：
    // clearSelection() + selectRow() 会连续触发 selectionChanged，
    // 导致同一页重复调用 updatePacketDetails()/loadHexPayload()。
    // 这里等整页数据准备好后，由调用方选中第一行触发一次即可。
    QSignalBlocker selectionBlocker(ui->packetTable->selectionModel());

    // 只创建当前页所需的 10 行，数据包总数很大时仍保持 UI 流畅。
    auto *table = ui->packetTable;
    table->clearContents();
    table->clearSelection();

    if (m_packets.isEmpty()) {
        table->setRowCount(0);
        updatePagination();
        // 没有数据时也要清空十六进制表格。
        updatePacketDetails();
        return;
    }

    // 将当前页转换为全局包索引范围；最后一页可能不足 10 包。
    const int firstPacket = m_currentPage * BinFormat::PacketsPerPage;
    const int endPacket =
        qMin(firstPacket + BinFormat::PacketsPerPage, m_packets.size());
    const int rowsOnPage = endPacket - firstPacket;

    // ---------------------------------------------------------------------
    // 数据浏览表格填充（调用链第 ⑥ 步）
    //
    // 每行每一列显示什么，全部由 packetCellText(总线, 列, 行, 数据) 决定：
    //   该函数内部按当前总线的列定义（m_packetColumns）把 BinPacket
    //   转换成单元格文本，是唯一的“列 → 显示内容”映射点。
    // 因此本函数只负责：
    //   1) 取出当前总线的列定义（列名与列数量）；
    //   2) 逐行逐列调用 packetCellText() 取值并填入表格。
    // 修改列的显示格式 → 改 packetCellText()；
    // 修改列名/列数量/列顺序 → 改对应总线的列定义接口
    // （setFcPacketTableColumns / setAuroraPacketTableColumns /
    //   setTcpPacketTableColumns）或 initializePacketTableColumns()。
    // ---------------------------------------------------------------------
    const int busColumnIndex = ui->busTypeCombo->currentIndex() - 1;
    const QList<ReviewColumn> columns =
        (busColumnIndex >= 0 && busColumnIndex < 3)
            ? m_packetColumns[busColumnIndex]
            : QList<ReviewColumn>();

    table->setRowCount(rowsOnPage);
    for (int row = 0; row < rowsOnPage; ++row) {
        // 表格行号是页内索引，需要加上 firstPacket 才能访问真实包。
        const BinPacket &packet = m_packets.at(firstPacket + row);

        for (int column = 0; column < columns.size(); ++column) {
            const ReviewColumnField field = columns.at(column).field;

            auto *item = new QTableWidgetItem(
                packetCellText(busColumnIndex, column, row, packet));
            // 预览列左对齐便于阅读，其余列居中。
            item->setTextAlignment(
                field == ReviewColumnField::PayloadPreview
                    ? (Qt::AlignVCenter | Qt::AlignLeft)
                    : Qt::AlignCenter);
            table->setItem(row, column, item);
        }
        table->setRowHeight(row, 30);
    }

    updatePagination();
}

void WidReview::selectFirstPacketOnPage()
{
    if (ui->packetTable->rowCount() <= 0) {
        // 空页也要清空详情区。
        updatePacketDetails();
        return;
    }

    // setCurrentCell 和 selectRow 可能各触发一次 selectionChanged，
    // 屏蔽信号后只手动调用一次 updatePacketDetails，
    // 避免翻页时重复清空/重建十六进制表格。
    {
        QSignalBlocker blocker(ui->packetTable->selectionModel());
        ui->packetTable->setCurrentCell(0, 0);
        ui->packetTable->selectRow(0);
    }
    updatePacketDetails();
}

void WidReview::updatePagination()
{
    // 计算总页数采用向上取整：
    // (总包数 + 每页包数 - 1) / 每页包数。
    // 例如 21 包会得到 3 页，最后一页只有 1 包。
    const int pageCount = m_packets.isEmpty()
                              ? 0
                              : (m_packets.size() + BinFormat::PacketsPerPage - 1)
                                    / BinFormat::PacketsPerPage;

    if (pageCount == 0) {
        m_currentPage = 0;
        ui->pageRangeLabel->setText(QStringLiteral("—"));
        ui->pageLabel->setText(QStringLiteral("— / —"));
        ui->pageSummaryValue->setText(QStringLiteral("— / —"));
        ui->pageJumpSpinBox->setRange(1, 1);
        ui->pageJumpSpinBox->setValue(1);
        ui->pageJumpSpinBox->setEnabled(false);
        ui->jumpPageButton->setEnabled(false);
        ui->prevPageButton->setEnabled(false);
        ui->nextPageButton->setEnabled(false);
    } else {
        // 防御性限制当前页，避免数据变化后页码越界。
        m_currentPage = qBound(0, m_currentPage, pageCount - 1);
        const int firstPacket = m_currentPage * BinFormat::PacketsPerPage;
        // lastPacket 是当前页最后一个包的全局索引加 1，
        // 可直接用于 "1-10 / 100 包" 的范围显示。
        const int lastPacket =
            qMin(firstPacket + BinFormat::PacketsPerPage, m_packets.size());

        ui->pageRangeLabel->setText(
            QStringLiteral("%1-%2 / %3 包")
                .arg(firstPacket + 1)
                .arg(lastPacket)
                .arg(m_packets.size()));
        ui->pageLabel->setText(
            QStringLiteral("第 %1 / %2 页").arg(m_currentPage + 1).arg(pageCount));
        ui->pageSummaryValue->setText(
            QStringLiteral("第 %1 / %2 页").arg(m_currentPage + 1).arg(pageCount));
        ui->pageJumpSpinBox->setRange(1, pageCount);
        ui->pageJumpSpinBox->setValue(m_currentPage + 1);
        ui->pageJumpSpinBox->setEnabled(!m_isImporting);
        ui->jumpPageButton->setEnabled(!m_isImporting);
        ui->prevPageButton->setEnabled(m_currentPage > 0);
        ui->nextPageButton->setEnabled(m_currentPage + 1 < pageCount);
    }
}

void WidReview::updatePacketDetails()
{
    // packetTable 使用整行选择；selectedRows() 为空通常表示当前页尚未选择数据。
    const QModelIndexList selectedRows = ui->packetTable->selectionModel()->selectedRows();
    const int row = selectedRows.isEmpty() ? -1 : selectedRows.first().row();

    // 当前表行是页内行号（0-9），需要换算成全部数据包中的全局索引。
    const int packetIndex =
        row < 0 ? -1 : m_currentPage * BinFormat::PacketsPerPage + row;

    // 越界通常发生在翻页清空选择、最后一页不足 10 行或文件被重置时。
    if (packetIndex < 0 || packetIndex >= m_packets.size()) {
        ui->detailTitle->setText(QStringLiteral("数据段十六进制"));
        ui->detailMeta->setText(QStringLiteral("选择本页数据包以查看完整内容"));
        loadHexPayload(-1);
        return;
    }

    const BinPacket &packet = m_packets.at(packetIndex);

    // 标题和元信息可以直接从 BinPacket 得到，不需要读取数据段。
    ui->detailTitle->setText(
        QStringLiteral("第 %1 包 · 数据段（可编辑）")
            .arg(packet.index + 1, 2, 10, QLatin1Char('0')));

    const qint64 payloadSize = packet.payloadSize();
    const qint64 previewSize = qMin(payloadSize, MaxHexPreviewBytes);
    QString meta =
        QStringLiteral("包头 %1   ·   n=%2   ·   %3 B   ·   文件偏移 %4")
            .arg(BinFormat::toHex(packet.header))
            .arg(packet.dataCount)
            .arg(payloadSize)
            .arg(formatHexOffset(packet.offset));
    // 单包超过 64 KB 时明确提示只加载了前缀，避免用户误以为可编辑全部数据。
    if (previewSize < payloadSize) {
        meta.append(QStringLiteral("   ·   仅显示前 64 KB"));
    }
    ui->detailMeta->setText(meta);
    loadHexPayload(packetIndex);
}

void WidReview::resetView()
{
    // 重置顺序：
    //   1. 清空文件路径、分页和包元数据；
    //   2. 清空摘要、列表和数据段编辑状态；
    //   3. 重置状态标签和窗口标题。
    m_packets.clear();
    m_currentFilePath.clear();
    m_selectedFilePath.clear();
    m_selectedFileSize = 0;
    m_busType = BusType::None;
    m_currentPage = 0;

    ui->fileNameValue->setText(QStringLiteral("—"));
    ui->fileNameValue->setToolTip(QString());
    ui->fileSizeValue->setText(QStringLiteral("—"));
    ui->packetCountValue->setText(QStringLiteral("0"));
    ui->packetCountBadge->setText(QStringLiteral("0"));
    ui->fileSizeSummaryValue->setText(QStringLiteral("—"));
    ui->filePathValue->setText(QStringLiteral("—"));
    ui->filePathValue->setToolTip(QString());
    ui->currentFileLabel->setText(QStringLiteral("未选择文件"));
    ui->currentFileLabel->setToolTip(QString());
    {
        QSignalBlocker blocker(ui->busTypeCombo);
        ui->busTypeCombo->setCurrentIndex(0);
        ui->busTypeCombo->setEnabled(false);
    }
    ui->clearButton->setEnabled(false);
    ui->dropTitle->setText(QStringLiteral("拖放 .bin 文件到此处"));
    ui->dropHint->setText(QStringLiteral("或从本地选择文件"));

    // 空状态不创建占位数据行，避免被误认为真实数据包。
    auto *table = ui->packetTable;
    table->clearContents();
    table->setRowCount(0);
    table->clearSelection();
    setPacketTableSchema(PacketTableSchema::Empty);
    updatePagination();

    ui->detailTitle->setText(QStringLiteral("数据段十六进制"));
    ui->detailMeta->setText(QStringLiteral("选择本页数据包以查看完整内容"));
    // 传入 -1 会清空原始/编辑缓冲、保存按钮和可编辑单元格。
    loadHexPayload(-1);

    setStatus(QStringLiteral("等待导入"), IdleState);
    setFileState(QStringLiteral("等待选择 .bin 文件"), IdleState);
    setDropHighlighted(false);
    setReviewStatus(QStringLiteral("就绪"));

    // 导入期间 resetView() 可能被失败/取消流程调用，
    // 此时不能提前恢复“选择文件”等按钮，需等待线程最终结束。
    if (!m_isImporting) {
        setImportingUi(false);
    }
}

void WidReview::showLoadError(const QString &message)
{
    // 顶栏显示简短错误状态，完整原因通过对话框展示。
    setStatus(QStringLiteral("导入失败"), ErrorState);
    setFileState(QStringLiteral("导入失败 · 请检查文件格式"), ErrorState);
    ui->fileStateLabel->setToolTip(message);
    setReviewStatus(QStringLiteral("导入失败"));
    QMessageBox::warning(this, QStringLiteral("无法导入文件"), message);
}

void WidReview::setReviewStatus(const QString &text)
{
    if (m_reviewStatusLabel != nullptr) {
        m_reviewStatusLabel->setText(text);
    }
}

void WidReview::setDropHighlighted(bool highlighted)
{
    // 拖入文件时通过动态属性切换上传区的虚线边框样式。
    ui->uploadFrame->setProperty("dragActive", highlighted);
    refreshStyle(ui->uploadFrame);
}

void WidReview::setStatus(const QString &text, const QString &state)
{
    // 顶部状态标签和文件状态标签共用 idle/success/error 三套样式。
    ui->statusPill->setText(text);
    ui->statusPill->setProperty("state", state);
    refreshStyle(ui->statusPill);
}

void WidReview::setFileState(const QString &text, const QString &state)
{
    // 文件状态标签的错误文本可能很长，因此同时写入 tooltip。
    ui->fileStateLabel->setText(text);
    ui->fileStateLabel->setProperty("state", state);
    ui->fileStateLabel->setToolTip(text);
    refreshStyle(ui->fileStateLabel);
}

void WidReview::dragEnterEvent(QDragEnterEvent *event)
{
    // 后台导入期间拒绝新的拖放任务，避免同时运行多个扫描线程。
    if (m_isImporting) {
        event->ignore();
        return;
    }

    // 只要拖动内容中至少有一个合法 .bin 文件，就允许放置。
    if (event->mimeData()->hasUrls()) {
        const QList<QUrl> urls = event->mimeData()->urls();
        for (const QUrl &url : urls) {
            if (isBinUrl(url)) {
                event->acceptProposedAction();
                setDropHighlighted(true);
                return;
            }
        }
    }

    event->ignore();
}

void WidReview::dragLeaveEvent(QDragLeaveEvent *event)
{
    // 鼠标离开窗口后取消上传区高亮。
    setDropHighlighted(false);
    QWidget::dragLeaveEvent(event);
}

void WidReview::dropEvent(QDropEvent *event)
{
    if (m_isImporting) {
        event->ignore();
        return;
    }

    // 多文件拖入时只处理列表中的第一个合法 .bin 文件。
    setDropHighlighted(false);

    if (!event->mimeData()->hasUrls()) {
        event->ignore();
        return;
    }

    const QList<QUrl> urls = event->mimeData()->urls();
    for (const QUrl &url : urls) {
        if (isBinUrl(url)) {
            event->acceptProposedAction();
            loadFile(url.toLocalFile());
            return;
        }
    }

    event->acceptProposedAction();
    resetView();
    showLoadError(QStringLiteral("拖入的内容中没有可读取的 .bin 文件。"));
}
