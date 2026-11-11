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
// =============================================================================

#include "aurorafileimporter.h"
#include "mytools/binfileimporterbase.h"
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
const QString IdleState = QString("idle");
const QString SuccessState = QString("success");
const QString ErrorState = QString("error");

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
        return QString("%1 B").arg(bytes);
    }

    double value = static_cast<double>(bytes) / 1024.0;
    QString unit = QString("KB");
    if (bytes >= 1024LL * 1024 * 1024) {
        value = static_cast<double>(bytes) / (1024.0 * 1024.0 * 1024.0);
        unit = QString("GB");
    } else if (bytes >= 1024LL * 1024) {
        value = static_cast<double>(bytes) / (1024.0 * 1024.0);
        unit = QString("MB");
    }

    return QString("%1 %2  (%3 B)")
        .arg(QString::number(value, 'f', 2))
        .arg(unit)
        .arg(bytes);
}

// 将文件偏移格式化为固定八位大写 HEX。
// @param offset 文件绝对字节偏移。
// @return 例如 966 返回 "0x000003C6"。
QString formatHexOffset(qint64 offset)
{
    return QString("0x")
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
    return info.isFile() && info.suffix().compare(QString("bin"), Qt::CaseInsensitive) == 0;
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
    ui->chooseButton->setIcon(QIcon(QString(":/icons/folder-open.svg")));
    ui->chooseButton->setIconSize(QSize(16, 16));
    ui->clearButton->setIcon(QIcon(QString(":/icons/trash-2.svg")));
    ui->clearButton->setIconSize(QSize(15, 15));
    // 页面内部状态栏，用于显示扫描、保存和错误等状态。
    auto *statusFrame = new QFrame;
    statusFrame->setObjectName(QString("reviewStatusBar"));
    statusFrame->setFixedHeight(22);
    auto *statusLayout = new QHBoxLayout(statusFrame);
    statusLayout->setContentsMargins(10, 0, 10, 0);
    statusLayout->setSpacing(8);
    m_reviewStatusLabel = new QLabel(QString("就绪"));
    m_reviewStatusLabel->setObjectName(QString("reviewStatusText"));
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
    // 数据包列表列定义：
    //   0 包序号、1 文件偏移、2 包头、3 n、4 数据长度、5 数据预览。
    // 表格只填充当前页，避免为 65,536 个包创建 65,536 行控件。
    auto *table = ui->packetTable;
    table->setColumnCount(6);
    table->setHorizontalHeaderLabels(
        {QString(), QString(), QString(), QString(), QString(), QString()});

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
    header->setSectionResizeMode(0, QHeaderView::Fixed);
    header->setSectionResizeMode(1, QHeaderView::Fixed);
    header->setSectionResizeMode(2, QHeaderView::Fixed);
    header->setSectionResizeMode(3, QHeaderView::Fixed);
    header->setSectionResizeMode(4, QHeaderView::Fixed);
    header->setSectionResizeMode(5, QHeaderView::Stretch);
    table->setColumnWidth(0, 66);
    table->setColumnWidth(1, 106);
    table->setColumnWidth(2, 154);
    table->setColumnWidth(3, 76);
    table->setColumnWidth(4, 84);
    // 固定高度保证每页 10 包始终完整显示，不随窗口或最后一页包数伸缩。
    table->horizontalHeader()->setFixedHeight(30);
    table->setFixedHeight(332);
}

void WidReview::setPacketTableSchema(PacketTableSchema schema)
{
    auto *table = ui->packetTable;
    table->setColumnHidden(2, false);
    table->setColumnHidden(3, false);

    if (schema == PacketTableSchema::Aurora) {
        ui->packetTable->setHorizontalHeaderLabels(
            {QString("包序号"),
             QString("文件偏移"),
             QString("6 字节包头"),
             QString("数据块 n"),
             QString("数据长度"),
             QString("数据预览")});
    } else if (schema == PacketTableSchema::FC
               || schema == PacketTableSchema::Ethernet) {
        table->setColumnHidden(2, true);
        table->setColumnHidden(3, true);
        table->setHorizontalHeaderLabels(
            {QString("包序号"),
             QString("文件偏移"),
             QString(),
             QString(),
             QString("数据长度"),
             QString("数据预览")});
    } else {
        ui->packetTable->setHorizontalHeaderLabels(
            {QString(), QString(), QString(), QString(), QString(), QString()});
    }
}

void WidReview::configureHexTable()
{
    // 列 0 显示行偏移，列 1-16 分别对应 00-0F 字节。
    // 表头保持为字节位置，不随 16/10 进制切换而变化。
    auto *table = ui->hexTable;
    QStringList headers;
    headers << QString("偏移");
    for (int column = 0; column < 16; ++column) {
        headers << QString("%1")
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
        QString("选择 BIN 文件"),
        m_lastDirectory,
        QString("BIN 文件 (*.bin);;所有文件 (*.*)"));

    if (!filePath.isEmpty()) {
        loadFile(filePath);
    }
}

bool WidReview::loadFile(const QString &filePath)
{
    // 同一时间只允许一个后台导入任务。
    // 导入期间的“清空当前文件”按钮会临时变为“取消导入”。
    if (m_isImporting) {
        setReviewStatus(QString("当前文件仍在导入，请等待完成或取消导入。"));
        return false;
    }

    const QFileInfo info(filePath);

    // 这里只做 O(1) 的快速校验；真正的包头格式检查由后台线程完成。
    if (!info.exists() || !info.isFile()) {
        resetView();
        showLoadError(QString("所选文件不存在，或无法作为本地文件读取。"));
        return false;
    }

    if (info.suffix().compare(QString("bin"), Qt::CaseInsensitive) != 0) {
        resetView();
        showLoadError(QString("文件类型不正确，请选择扩展名为 .bin 的文件。"));
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

    setFileState(QString("文件已选择 · 请选择总线类型"), IdleState);
    setReviewStatus(
        QString("已选择文件：%1").arg(QDir::toNativeSeparators(info.absoluteFilePath())));
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
    ui->currentFileLabel->setText(QString("当前文件：%1").arg(info.fileName()));
    ui->currentFileLabel->setToolTip(QDir::toNativeSeparators(info.absoluteFilePath()));
    ui->dropTitle->setText(info.fileName());
    ui->dropHint->setText(QString("已选择文件，请选择总线类型"));
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

    setFileState(QString("文件已选择 · 请选择总线类型"), IdleState);
    setReviewStatus(QString("请选择总线类型后再执行数据解析。"));
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
    ui->dropTitle->setText(QString("正在导入文件"));
    ui->dropHint->setText(info.fileName());

    setImportingUi(true);
    m_importProgress->setValue(0);
    setFileState(QString("正在后台解析包头…"), IdleState);
    setReviewStatus(QString("正在导入：%1").arg(info.fileName()));

    // ---------------------------------------------------------------------
    // 创建后台线程
    //
    // 根据协议创建独立导入器，并 moveToThread() 转移线程亲和性。
    // 所有文件读取和包头解析都在工作线程中执行，主线程保持响应。
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
        importing ? QString("取消导入") : QString("清空当前文件"));

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
        QString("正在后台解析 · 已识别 %1 包").arg(packetCount));
    setReviewStatus(
        QString("正在解析 %1% · 已识别 %2 包 · %3 / %4")
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
    // 工作线程只返回元数据。这里把结果移交给主窗口并更新所有摘要信息。
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
    ui->dropTitle->setText(QString("文件已载入"));
    ui->dropHint->setText(QString("可将新的 .bin 文件拖到窗口中替换"));
    m_importProgress->setValue(100);

    // 导入完成后默认展示第一页，并自动选中第一包。
    populateCurrentPage();
    setFileState(
        QString("格式校验通过 · 共 %1 个数据包").arg(m_packets.size()),
        SuccessState);
    setReviewStatus(
        QString("已载入 %1 个数据包  |  %2")
            .arg(m_packets.size())
            .arg(QDir::toNativeSeparators(filePath)));

    if (ui->packetTable->rowCount() > 0) {
        ui->packetTable->setCurrentCell(0, 0);
        ui->packetTable->selectRow(0);
    }
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
    setFileState(QString("导入已取消"), IdleState);
    setReviewStatus(QString("导入已取消"));
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
        setReviewStatus(QString("正在取消导入…"));
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

    if (ui->packetTable->rowCount() > 0) {
        ui->packetTable->setCurrentCell(0, 0);
        ui->packetTable->selectRow(0);
    }
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

    if (ui->packetTable->rowCount() > 0) {
        ui->packetTable->setCurrentCell(0, 0);
        ui->packetTable->selectRow(0);
    }
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

    if (ui->packetTable->rowCount() > 0) {
        ui->packetTable->setCurrentCell(0, 0);
        ui->packetTable->selectRow(0);
    }
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

    return QString("%1")
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
        QString("数据段已切换为%1显示。")
            .arg(m_numberBase == NumberBase::Decimal
                     ? QString("10进制")
                     : QString("16进制")));
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
        if (text.startsWith(QString("0x"), Qt::CaseInsensitive)) {
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
                ? QString("数据格式无效，请输入 0-255 范围内的十进制数值。")
                : QString("数据格式无效，请输入 00-FF 范围内的十六进制字节。"));
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
        m_hexDirty ? QString("有未保存的数据修改")
                   : QString("数据修改已撤销"));
    setReviewStatus(
        m_hexDirty ? QString("数据已修改，点击“保存到文件”写回 BIN 文件。")
                   : QString("数据已恢复为文件中的原始内容。"));
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
            QString("保存失败"),
            QString("无法以读写方式打开 BIN 文件：%1").arg(file.errorString()));
        return;
    }

    // 检查 seek、完整写入和 flush 三个步骤，任一失败都保留脏状态。
    if (!file.seek(packet.offset + BinFormat::HeaderSize)
        || file.write(m_editablePayload) != m_editablePayload.size()
        || !file.flush()) {
        QMessageBox::warning(
            this,
            QString("保存失败"),
            QString("写入数据段失败：%1").arg(file.errorString()));
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
    setFileState(QString("修改已保存到 BIN 文件"), SuccessState);
    setReviewStatus(
        QString("第 %1 包数据段已保存  |  %2")
            .arg(packet.index + 1)
            .arg(QDir::toNativeSeparators(m_currentFilePath)));
}

void WidReview::loadHexPayload(int packetIndex)
{
    // 重建表格期间阻止 itemChanged，避免每个单元格触发一次业务逻辑。
    m_populatingHexTable = true;
    QSignalBlocker blocker(ui->hexTable);
    ui->hexTable->clearContents();
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
    const int rowCount =
        (m_editablePayload.size() + 15) / 16;
    ui->hexTable->setRowCount(rowCount);

    // 每行显示一个偏移列和 16 个可编辑字节列。
    for (int row = 0; row < rowCount; ++row) {
        auto *offsetItem = new QTableWidgetItem(
            QString("%1")
                .arg(row * 16, 8, 16, QLatin1Char('0'))
                .toUpper());
        offsetItem->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
        offsetItem->setTextAlignment(Qt::AlignCenter);
        ui->hexTable->setItem(row, 0, offsetItem);

        for (int column = 0; column < 16; ++column) {
            const int byteIndex = row * 16 + column;
            auto *item = new QTableWidgetItem;
            item->setTextAlignment(Qt::AlignCenter);
            // 最后一行不足 16 字节时，多余单元格设为不可编辑。
            if (byteIndex < m_editablePayload.size()) {
                item->setText(formatDisplayByte(
                    static_cast<quint8>(m_editablePayload.at(byteIndex))));
                item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable
                               | Qt::ItemIsEditable);
            } else {
                item->setFlags(Qt::NoItemFlags);
            }
            ui->hexTable->setItem(row, column + 1, item);
        }
        ui->hexTable->setRowHeight(row, 24);
    }

    m_populatingHexTable = false;
    ui->displayModeCombo->setEnabled(!m_editablePayload.isEmpty() && !m_isImporting);
    QTimer::singleShot(0, this, &WidReview::updateHexColumnWidths);
}

void WidReview::updatePacketPreviewRow()
{
    // 保存后只刷新当前页中对应数据包的前 12 字节预览，
    // 不重新构建整个分页，避免破坏用户当前的滚动和选择状态。
    const int row = m_editablePacketIndex - m_currentPage * BinFormat::PacketsPerPage;
    if (row < 0 || row >= ui->packetTable->rowCount() || m_editablePayload.isEmpty()) {
        return;
    }

    const BinPacket &packet = m_packets.at(m_editablePacketIndex);
    QString preview = BinFormat::toHex(m_editablePayload.left(12));
    if (packet.payloadSize() > 12) {
        preview.append(QString(" ..."));
    }
    if (QTableWidgetItem *item = ui->packetTable->item(row, 5)) {
        item->setText(preview);
    }
}

//设置包列表的值
void WidReview::populateCurrentPage()
{
    // 只创建当前页所需的 10 行，数据包总数很大时仍保持 UI 流畅。
    auto *table = ui->packetTable;
    table->clearContents();
    table->clearSelection();

    if (m_packets.isEmpty()) {
        table->setRowCount(0);
        updatePagination();
        return;
    }

    // 将当前页转换为全局包索引范围；最后一页可能不足 10 包。
    const int firstPacket = m_currentPage * BinFormat::PacketsPerPage;
    const int endPacket =
        qMin(firstPacket + BinFormat::PacketsPerPage, m_packets.size());
    const int rowsOnPage = endPacket - firstPacket;

    // 打开一次文件，为当前页最多 10 行读取各包前 12 字节预览。
    QFile previewFile(m_currentFilePath);
    const bool canReadPreview = previewFile.open(QIODevice::ReadOnly);

    table->setRowCount(rowsOnPage);
    for (int row = 0; row < rowsOnPage; ++row) {
        // 表格行号是页内索引，需要加上 firstPacket 才能访问真实包。
        const BinPacket &packet = m_packets.at(firstPacket + row);

        // 第 0 列：包序号，内部索引从 0 开始，界面显示从 01 开始。
        auto *indexItem = new QTableWidgetItem(
            QString("%1").arg(packet.index + 1, 2, 10, QLatin1Char('0')));
        indexItem->setTextAlignment(Qt::AlignCenter);

        // 第 1 列：该包在 BIN 文件中的绝对起始偏移，显示为 8 位 HEX。
        auto *offsetItem = new QTableWidgetItem(formatHexOffset(packet.offset));
        offsetItem->setTextAlignment(Qt::AlignCenter);

        // 第 2 列：6 字节包头原始内容，转换为连续 HEX 文本。
        auto *headerItem = new QTableWidgetItem(BinFormat::toHex(packet.header));
        headerItem->setTextAlignment(Qt::AlignCenter);

        // 第 3 列：包头最后两个字节按 BIN_HEADER_BIG_ENDIAN 解析出的数据块数量 n。
        auto *dataCountItem = new QTableWidgetItem(
            QString::number(packet.dataCount));
        dataCountItem->setTextAlignment(Qt::AlignCenter);

        // 第 4 列：该包数据段实际字节数，计算方式为 n × 1024。
        auto *lengthItem = new QTableWidgetItem(
            QString("%1 B").arg(packet.payloadSize()));
        lengthItem->setTextAlignment(Qt::AlignCenter);

        // 只读取当前页可见行的少量预览字节，导入时无需加载完整数据段。
        QByteArray previewBytes;
        if (canReadPreview
            && previewFile.seek(packet.offset + BinFormat::HeaderSize)) {
            previewBytes =
                previewFile.read(qMin<qint64>(12, packet.payloadSize()));
        }

        QString preview = BinFormat::toHex(previewBytes);
        if (packet.payloadSize() > 12) {
            preview.append(QString(" ..."));
        } else if (preview.isEmpty()) {
            preview = QString("—");
        }
        // 第 5 列：数据段前 12 字节的 HEX 预览；超过 12 字节时追加省略号。
        auto *previewItem = new QTableWidgetItem(preview);
        previewItem->setTextAlignment(Qt::AlignVCenter | Qt::AlignLeft);

        // 元数据全部来自 m_packets；只有预览列需要读取原始文件。
        table->setItem(row, 0, indexItem);
        table->setItem(row, 1, offsetItem);
        table->setItem(row, 2, headerItem);
        table->setItem(row, 3, dataCountItem);
        table->setItem(row, 4, lengthItem);
        table->setItem(row, 5, previewItem);
        table->setRowHeight(row, 30);
    }

    updatePagination();
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
        ui->pageRangeLabel->setText(QString("—"));
        ui->pageLabel->setText(QString("— / —"));
        ui->pageSummaryValue->setText(QString("— / —"));
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
            QString("%1-%2 / %3 包")
                .arg(firstPacket + 1)
                .arg(lastPacket)
                .arg(m_packets.size()));
        ui->pageLabel->setText(
            QString("第 %1 / %2 页").arg(m_currentPage + 1).arg(pageCount));
        ui->pageSummaryValue->setText(
            QString("第 %1 / %2 页").arg(m_currentPage + 1).arg(pageCount));
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
        ui->detailTitle->setText(QString("数据段十六进制"));
        ui->detailMeta->setText(QString("选择本页数据包以查看完整内容"));
        loadHexPayload(-1);
        return;
    }

    const BinPacket &packet = m_packets.at(packetIndex);

    // 标题和元信息可以直接从 BinPacket 得到，不需要读取数据段。
    ui->detailTitle->setText(
        QString("第 %1 包 · 数据段（可编辑）")
            .arg(packet.index + 1, 2, 10, QLatin1Char('0')));

    const qint64 payloadSize = packet.payloadSize();
    const qint64 previewSize = qMin(payloadSize, MaxHexPreviewBytes);
    QString meta =
        QString("包头 %1   ·   n=%2   ·   %3 B   ·   文件偏移 %4")
            .arg(BinFormat::toHex(packet.header))
            .arg(packet.dataCount)
            .arg(payloadSize)
            .arg(formatHexOffset(packet.offset));
    // 单包超过 64 KB 时明确提示只加载了前缀，避免用户误以为可编辑全部数据。
    if (previewSize < payloadSize) {
        meta.append(QString("   ·   仅显示前 64 KB"));
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

    ui->fileNameValue->setText(QString("—"));
    ui->fileNameValue->setToolTip(QString());
    ui->fileSizeValue->setText(QString("—"));
    ui->packetCountValue->setText(QString("0"));
    ui->packetCountBadge->setText(QString("0"));
    ui->fileSizeSummaryValue->setText(QString("—"));
    ui->filePathValue->setText(QString("—"));
    ui->filePathValue->setToolTip(QString());
    ui->currentFileLabel->setText(QString("未选择文件"));
    ui->currentFileLabel->setToolTip(QString());
    {
        QSignalBlocker blocker(ui->busTypeCombo);
        ui->busTypeCombo->setCurrentIndex(0);
        ui->busTypeCombo->setEnabled(false);
    }
    ui->clearButton->setEnabled(false);
    ui->dropTitle->setText(QString("拖放 .bin 文件到此处"));
    ui->dropHint->setText(QString("或从本地选择文件"));

    // 空状态不创建占位数据行，避免被误认为真实数据包。
    auto *table = ui->packetTable;
    table->clearContents();
    table->setRowCount(0);
    table->clearSelection();
    setPacketTableSchema(PacketTableSchema::Empty);
    updatePagination();

    ui->detailTitle->setText(QString("数据段十六进制"));
    ui->detailMeta->setText(QString("选择本页数据包以查看完整内容"));
    // 传入 -1 会清空原始/编辑缓冲、保存按钮和可编辑单元格。
    loadHexPayload(-1);

    setFileState(QString("等待选择 .bin 文件"), IdleState);
    setDropHighlighted(false);
    setReviewStatus(QString("就绪"));

    // 导入期间 resetView() 可能被失败/取消流程调用，
    // 此时不能提前恢复“选择文件”等按钮，需等待线程最终结束。
    if (!m_isImporting) {
        setImportingUi(false);
    }
}

void WidReview::showLoadError(const QString &message)
{
    // 顶栏显示简短错误状态，完整原因通过对话框展示。
    setFileState(QString("导入失败 · 请检查文件格式"), ErrorState);
    ui->fileStateLabel->setToolTip(message);
    setReviewStatus(QString("导入失败"));
    QMessageBox::warning(this, QString("无法导入文件"), message);
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
    showLoadError(QString("拖入的内容中没有可读取的 .bin 文件。"));
}
