// 对应界面：wid_customdatasource.ui。
// 本文件负责自定义帧生成、分页、帧编辑以及 AHEAD + 帧数据文件保存。
#include "wid_customdatasource.h"

#include "binformat.h"
#include "fcframeeditdialog.h"
#include "tcpframeeditdialog.h"
#include "ui_wid_customdatasource.h"

#include <QAbstractButton>
#include <QCheckBox>
#include <QComboBox>
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSaveFile>
#include <QSignalBlocker>
#include <QTableWidget>
#include <QTableWidgetItem>

#include <memory>

namespace {

// 帧列表每页固定显示 50 行。
constexpr int FramesPerPage = 50;

// 清理数据源名称中的 Windows 非法文件名字符，并生成 .bin 文件名。
QString safeFileName(const QString &name)
{
    QString baseName = name.trimmed();
    if (baseName.endsWith(QStringLiteral(".bin"), Qt::CaseInsensitive)) {
        baseName.chop(4);
    }

    const QString invalidCharacters =
        QStringLiteral("\\/:*?\"<>|");
    for (int index = 0; index < baseName.size(); ++index) {
        if (invalidCharacters.contains(baseName.at(index))) {
            baseName[index] = QLatin1Char('_');
        }
    }

    while (!baseName.isEmpty()
           && (baseName.endsWith(QLatin1Char(' '))
               || baseName.endsWith(QLatin1Char('.')))) {
        baseName.chop(1);
    }
    if (baseName.isEmpty()) {
        baseName = QStringLiteral("custom_data");
    }
    return baseName + QStringLiteral(".bin");
}

// 将当前帧包含的 1024 字节包数写入 AHEAD 最后两个字节。
// 字节序沿用 binformat.h 中的 BIN_HEADER_BIG_ENDIAN 配置，
// 与 Aurora 导入器解析包头 n 的规则保持一致。
void setAheadPacketCount(AHEAD &ahead, int packetCount)
{
    const int boundedCount = qBound(0, packetCount, 0xFFFF);
    const quint16 storedCount = static_cast<quint16>(boundedCount);

#if BIN_HEADER_BIG_ENDIAN
    // 大端：payload[4] 为高字节，payload[5] 为低字节。
    ahead.payload[4] = static_cast<quint8>(storedCount >> 8);
    ahead.payload[5] = static_cast<quint8>(storedCount & 0xFF);
#else
    // 小端：payload[4] 为低字节，payload[5] 为高字节。
    ahead.payload[4] = static_cast<quint8>(storedCount & 0xFF);
    ahead.payload[5] = static_cast<quint8>(storedCount >> 8);
#endif
}

// 将完整字节数据转换为空格分隔的两位大写 HEX 文本。
QString formatHexData(const QByteArray &data)
{
    QStringList values;
    values.reserve(data.size());
    for (char rawByte : data) {
        values.append(
            QStringLiteral("%1")
                .arg(static_cast<quint8>(rawByte), 2, 16, QLatin1Char('0'))
                .toUpper());
    }
    return values.join(QLatin1Char(' '));
}

// 将完整字节数据转换为逐项 HEX 列表，供帧编辑对话框使用。
QStringList hexValues(const QByteArray &data)
{
    QStringList values;
    values.reserve(data.size());
    for (char rawByte : data) {
        values.append(
            QStringLiteral("%1")
                .arg(static_cast<quint8>(rawByte), 2, 16, QLatin1Char('0'))
                .toUpper());
    }
    return values;
}

} // namespace

wid_customdatasource::wid_customdatasource(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::wid_customdatasource)
{
    ui->setupUi(this);
    configureFrameTable();

    connect(ui->backButton,
            &QPushButton::clicked,
            this,
            &wid_customdatasource::handleBack);
    connect(ui->saveButton,
            &QPushButton::clicked,
            this,
            &wid_customdatasource::saveDataSource);
    connect(ui->busTypeCombo,
            static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
            this,
            &wid_customdatasource::updateBusTypePage);

    // FC / Aurora / 以太网三个页面控件结构完全一致，
    // 统一把“生成、清空、翻页、单元格编辑”连接到同一组槽函数，
    // 槽函数内部通过 currentFramePageWidgets() 操作当前显示的页面。
    const QList<FramePageWidgets> pages = allFramePageWidgets();
    for (const FramePageWidgets &page : pages) {
        connect(page.generate,
                &QPushButton::clicked,
                this,
                &wid_customdatasource::generateFrames);
        connect(page.clear,
                &QPushButton::clicked,
                this,
                &wid_customdatasource::clearFrames);
        connect(page.prev,
                &QPushButton::clicked,
                this,
                &wid_customdatasource::showPreviousFramePage);
        connect(page.next,
                &QPushButton::clicked,
                this,
                &wid_customdatasource::showNextFramePage);
        connect(page.jump,
                &QPushButton::clicked,
                this,
                &wid_customdatasource::jumpToFramePage);
        connect(page.table,
                &QTableWidget::itemChanged,
                this,
                &wid_customdatasource::handleFrameItemChanged);
    }

    populateFramePage();
    updateBusTypePage(ui->busTypeCombo->currentIndex());
}

wid_customdatasource::~wid_customdatasource()
{
    delete ui;
}

void wid_customdatasource::handleBack()
{
    emit backRequested();
}

void wid_customdatasource::updateBusTypePage(int index)
{
    // 总线类型：0=FC，1=Aurora，2=以太网（TCP）。
    // 三个页面控件结构完全一致，切换总线时显示对应页面；
    // 唯一区别是各页面“编辑”弹窗显示的数据帧参数不同。
    switch (index) {
    case 1:
        ui->typePageStack->setCurrentWidget(ui->auroraPage);
        break;
    case 2:
        ui->typePageStack->setCurrentWidget(ui->tcpPage);
        break;
    default:
        ui->typePageStack->setCurrentWidget(ui->fcPage);
        break;
    }

    // 切换总线类型后，各总线的帧格式/参数不同，清空当前页面的帧列表，
    // 避免把上一总线的帧数据带到新类型下误保存。
    clearFrames();
}

QList<wid_customdatasource::FramePageWidgets>
wid_customdatasource::allFramePageWidgets() const
{
    QList<FramePageWidgets> pages;

    // FC 页面。
    {
        FramePageWidgets page;
        page.page = ui->fcPage;
        page.settingsFrame = ui->settingsFrame;
        page.frameCount = ui->frameCountSpinBox;
        page.generate = ui->generateButton;
        page.clear = ui->clearFramesButton;
        page.table = ui->frameTable;
        page.pageSpin = ui->framePageSpinBox;
        page.pageCount = ui->framePageCountLabel;
        page.prev = ui->framePrevButton;
        page.next = ui->frameNextButton;
        page.jump = ui->frameJumpButton;
        pages.append(page);
    }

    // Aurora 页面（控件结构与 FC 完全一致，仅对象名前缀不同）。
    {
        FramePageWidgets page;
        page.page = ui->auroraPage;
        page.settingsFrame = ui->auroraSettingsFrame;
        page.frameCount = ui->auroraFrameCountSpinBox;
        page.generate = ui->auroraGenerateButton;
        page.clear = ui->auroraClearFramesButton;
        page.table = ui->auroraFrameTable;
        page.pageSpin = ui->auroraFramePageSpinBox;
        page.pageCount = ui->auroraFramePageCountLabel;
        page.prev = ui->auroraFramePrevButton;
        page.next = ui->auroraFrameNextButton;
        page.jump = ui->auroraFrameJumpButton;
        pages.append(page);
    }

    // 以太网（TCP）页面。
    {
        FramePageWidgets page;
        page.page = ui->tcpPage;
        page.settingsFrame = ui->tcpSettingsFrame;
        page.frameCount = ui->tcpFrameCountSpinBox;
        page.generate = ui->tcpGenerateButton;
        page.clear = ui->tcpClearFramesButton;
        page.table = ui->tcpFrameTable;
        page.pageSpin = ui->tcpFramePageSpinBox;
        page.pageCount = ui->tcpFramePageCountLabel;
        page.prev = ui->tcpFramePrevButton;
        page.next = ui->tcpFrameNextButton;
        page.jump = ui->tcpFrameJumpButton;
        pages.append(page);
    }

    return pages;
}

wid_customdatasource::FramePageWidgets
wid_customdatasource::currentFramePageWidgets() const
{
    const QList<FramePageWidgets> pages = allFramePageWidgets();
    return pages.value(qBound(0, ui->busTypeCombo->currentIndex(),
                              pages.size() - 1));
}

void wid_customdatasource::generateFrames()
{
    // 帧个数取自当前总线页面（FC / Aurora / 以太网结构一致）。
    const FramePageWidgets page = currentFramePageWidgets();
    const int frameCount = page.frameCount->value();

    // “一键生成”只创建帧记录，PW/PRI/TOA 的序列参数在每帧的
    // “数据帧信息”窗口中设置，并在该窗口点击“生成”时应用。
    // 每帧底层使用 1024 字节紧凑数组。
    m_frames.clear();
    m_frames.reserve(frameCount);
    for (int index = 0; index < frameCount; ++index) {
        QByteArray frameData(AuroraFieldLayout::DataLength, '\0');
        FrameRecord frame;
        // 新建帧的默认赋值：
        // length     = 当前帧数据总字节数，显示到“帧长”列；
        // dataBytes  = 实际保存的原始字节；
        // auroraData = 编辑窗口中的 Aurora 参数快照。
        frame.length = QString::number(frameData.size());
        frame.dataBytes = frameData;
        frame.auroraData.baseIndex = 0;
        frame.auroraData.length = AuroraFieldLayout::DataLength;
        frame.auroraData.randomMode = false;
        m_frames.append(frame);
    }

    m_framePage = 0;
    populateFramePage();
}

void wid_customdatasource::clearFrames()
{
    // 只清除帧列表数据，不重置发送时间和帧个数。
    m_frames.clear();
    m_framePage = 0;
    populateFramePage();
}

void wid_customdatasource::saveDataSource()
{
    const QString dataSourceName = ui->dataSourceNameEdit->text().trimmed();
    if (dataSourceName.isEmpty()) {
        QMessageBox::warning(
            this,
            QStringLiteral("保存失败"),
            QStringLiteral("请输入数据源名称。"));
        return;
    }
    if (m_frames.isEmpty()) {
        QMessageBox::warning(
            this,
            QStringLiteral("保存失败"),
            QStringLiteral("帧列表为空，请先点击“一键生成”创建帧数据。"));
        return;
    }

    const QString dataDirectory =
        QDir(QCoreApplication::applicationDirPath())
            .filePath(QStringLiteral("MYDATA"));
    if (!QDir().mkpath(dataDirectory)) {
        QMessageBox::warning(
            this,
            QStringLiteral("保存失败"),
            QStringLiteral("无法创建 MYDATA 目录。"));
        return;
    }

    const QString filePath =
        QDir(dataDirectory).filePath(safeFileName(dataSourceName));
    QSaveFile file(filePath);
    if (!file.open(QIODevice::WriteOnly)) {
        QMessageBox::warning(
            this,
            QStringLiteral("保存失败"),
            QStringLiteral("无法创建 BIN 文件：%1").arg(file.errorString()));
        return;
    }

    // 每一帧都使用独立的 AHEAD，因为最后两个字节要写入该帧的包数。
    for (const FrameRecord &frame : m_frames) {
        // 未经编辑的帧也按 1024 个 0 字节保存，确保每行都有完整数据段。
        const QByteArray frameData =
            frame.dataBytes.isEmpty()
                ? QByteArray(AuroraFieldLayout::DataLength, '\0')
                : frame.dataBytes;

        // 包数 = 当前行帧数据总字节数 / 1024。
        // 例如帧长为 3072 时，AHEAD 最后两字节写入 3。
        AHEAD ahead;
        const int packetCount =
            frameData.size() / AuroraFieldLayout::DataLength;
        setAheadPacketCount(ahead, packetCount);

        // 写入顺序固定为：6 字节 AHEAD + 当前帧完整数据。
        if (file.write(reinterpret_cast<const char *>(&ahead),
                       sizeof(AHEAD))
            != sizeof(AHEAD)) {
            file.cancelWriting();
            QMessageBox::warning(
                this,
                QStringLiteral("保存失败"),
                QStringLiteral("写入 AHEAD 失败：%1").arg(file.errorString()));
            return;
        }

        if (file.write(frameData) != frameData.size()) {
            file.cancelWriting();
            QMessageBox::warning(
                this,
                QStringLiteral("保存失败"),
                QStringLiteral("写入帧数据失败：%1").arg(file.errorString()));
            return;
        }
    }

    if (!file.commit()) {
        QMessageBox::warning(
            this,
            QStringLiteral("保存失败"),
            QStringLiteral("BIN 文件提交失败：%1").arg(file.errorString()));
        return;
    }

    // 文件成功后通知外层页面写入对应总线类型的数据表。
    emit saveRequested(
        dataSourceName,
        ui->busTypeCombo->currentText(),
        QFileInfo(filePath).absoluteFilePath(),
        QDateTime::currentDateTime());
}

void wid_customdatasource::configureFrameTable()
{
    // FC / Aurora / 以太网三个页面的帧列表结构完全一致，
    // 统一设置列标题、列宽和交互方式。
    const QList<FramePageWidgets> pages = allFramePageWidgets();
    for (const FramePageWidgets &page : pages) {
        QTableWidget *table = page.table;
        table->setColumnCount(4);
        table->setHorizontalHeaderLabels(
            {QStringLiteral("帧序号"),
             QStringLiteral("帧长"),
             QStringLiteral("值"),
             QStringLiteral("操作")});
        table->verticalHeader()->setVisible(false);
        table->setSelectionBehavior(QAbstractItemView::SelectRows);
        table->setSelectionMode(QAbstractItemView::SingleSelection);
        table->setEditTriggers(
            QAbstractItemView::DoubleClicked
            | QAbstractItemView::EditKeyPressed);
        table->horizontalHeader()->setSectionResizeMode(
            0, QHeaderView::Fixed);
        table->horizontalHeader()->setSectionResizeMode(
            1, QHeaderView::Fixed);
        table->horizontalHeader()->setSectionResizeMode(
            2, QHeaderView::Stretch);
        table->horizontalHeader()->setSectionResizeMode(
            3, QHeaderView::Fixed);
        table->setColumnWidth(0, 120);
        table->setColumnWidth(1, 220);
        table->setColumnWidth(3, 170);
    }
}

void wid_customdatasource::createOperationWidget(int row)
{
    const int globalIndex = m_framePage * FramesPerPage + row;

    auto *container = new QWidget;
    auto *layout = new QHBoxLayout(container);
    layout->setContentsMargins(2, 2, 2, 2);
    layout->setSpacing(5);

    auto *editButton = new QPushButton(QStringLiteral("编辑"));
    editButton->setCursor(Qt::PointingHandCursor);
    editButton->setMinimumSize(54, 26);

    auto *removeButton = new QPushButton(QStringLiteral("删除"));
    removeButton->setCursor(Qt::PointingHandCursor);
    removeButton->setMinimumSize(54, 26);
    removeButton->setStyleSheet(
        QStringLiteral(
            "QPushButton { background:#FF7070; color:white; border:1px solid #E05252;"
            " border-radius:3px; }"
            "QPushButton:hover { background:#FF5C5C; }"));

    // “编辑”按钮打开独立的数据帧信息对话框：
    // FC 模式显示 FC Header set（D_ID、R_CTL … MSG_L 共 18 项）；
    // Aurora / 以太网模式仍显示 Aurora 参数（PW/PRI/TOA）和数据值。
    connect(editButton, &QPushButton::clicked, this, [this, globalIndex]() {
        if (globalIndex < 0 || globalIndex >= m_frames.size()) {
            return;
        }

        FrameRecord &frame = m_frames[globalIndex];
        // 下拉框索引：0=FC，1=Aurora，2=以太网。
        // 三条总线各自使用独立的对话框类：
        //   FC     → AuroraFrameEditDialog（FC Header set 模式）
        //   Aurora → AuroraFrameEditDialog（Aurora 参数模式）
        //   以太网 → TcpFrameEditDialog（以太网自己的窗口类）
        const int busIndex = ui->busTypeCombo->currentIndex();
        const bool fcMode = (busIndex == 0);

        AuroraFrameEditData editData = frame.auroraData;
        editData.length = AuroraFieldLayout::DataLength;
        if (frame.dataBytes.isEmpty()) {
            frame.dataBytes.fill('\0', AuroraFieldLayout::DataLength);
        }
        editData.gridValues = hexValues(frame.dataBytes);

        std::unique_ptr<AuroraFrameEditDialog> dialog;
        if (busIndex == 0) {
            // FC：FC Header set 模式，由 FcFrameEditDialog 独立实现。
            dialog.reset(new FcFrameEditDialog(this));
        } else if (busIndex == 2) {
            // 以太网：由 TcpFrameEditDialog 独立实现。
            dialog.reset(new TcpFrameEditDialog(this));
        } else {
            // Aurora：Aurora 参数模式。
            dialog.reset(new AuroraFrameEditDialog(this));
            dialog->setBusMode(AuroraFrameEditDialog::BusMode::Aurora);
        }
        if (fcMode) {
            dialog->setFcHeaderValues(frame.fcHeaderValues);
        }
        dialog->setFrameData(editData);
        if (dialog->exec() != QDialog::Accepted) {
            return;
        }

        frame.auroraData = dialog->frameData();
        if (fcMode) {
            // 保存 FC Header set 的 18 项参数，下次编辑同一帧时回填。
            frame.fcHeaderValues = dialog->fcHeaderValues();
        }

        // 对话框保存后，将完整 HEX 数据还原成紧凑的字节数组。
        const QStringList savedHexValues = dialog->hexadecimalValues();
        // 包数可能大于 1，因此按对话框返回的实际数据量分配，
        // 总长度通常为“包数 × 1024”。
        frame.dataBytes = QByteArray(savedHexValues.size(), '\0');
        for (int index = 0;
             index < savedHexValues.size() && index < frame.dataBytes.size();
             ++index) {
            bool valueOk = false;
            const uint value =
                savedHexValues.at(index).toUInt(&valueOk, 16);
            if (valueOk && value <= 0xFF) {
                frame.dataBytes[index] = static_cast<char>(value);
            }
        }

        // 保存后赋值关系：
        // 帧长 = dataBytes.size()；
        // 值列 = populateFramePage() 中的 formatHexData(dataBytes)。
        frame.length = QString::number(frame.dataBytes.size());
        populateFramePage();
    });

    connect(removeButton, &QPushButton::clicked, this, [this, globalIndex]() {
        removeFrame(globalIndex);
    });

    layout->addWidget(editButton);
    layout->addWidget(removeButton);
    layout->addStretch();
    // 把操作按钮放入当前总线页面的帧列表。
    currentFramePageWidgets().table->setCellWidget(row, 3, container);
}

void wid_customdatasource::populateFramePage()
{
    const FramePageWidgets page = currentFramePageWidgets();
    QTableWidget *table = page.table;
    {
        QSignalBlocker blocker(table);
        table->clearContents();
        table->setRowCount(0);

        const int pageCount =
            qMax(1, (m_frames.size() + FramesPerPage - 1) / FramesPerPage);
        m_framePage = qBound(0, m_framePage, pageCount - 1);
        const int firstFrame = m_framePage * FramesPerPage;
        const int endFrame = qMin(firstFrame + FramesPerPage, m_frames.size());
        const int rowCount = endFrame - firstFrame;
        table->setRowCount(rowCount);

        for (int row = 0; row < rowCount; ++row) {
            const int globalIndex = firstFrame + row;
            const FrameRecord &frame = m_frames.at(globalIndex);

            auto *numberItem =
                new QTableWidgetItem(QString::number(globalIndex + 1));
            numberItem->setTextAlignment(Qt::AlignCenter);
            numberItem->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);

            const QString lengthText =
                frame.dataBytes.isEmpty()
                    ? frame.length
                    : QString::number(frame.dataBytes.size());
            const QString valueText =
                frame.dataBytes.isEmpty()
                    ? frame.value
                    : formatHexData(frame.dataBytes);

            auto *lengthItem = new QTableWidgetItem(lengthText);
            lengthItem->setTextAlignment(Qt::AlignCenter);
            auto *valueItem = new QTableWidgetItem(valueText);
            // 帧列表列索引：0=帧序号，1=帧长，2=值，3=操作。
            // lengthText 来自 dataBytes.size()，valueText 来自完整 HEX 数据。
            const Qt::ItemFlags readOnlyFlags =
                Qt::ItemIsEnabled | Qt::ItemIsSelectable;
            lengthItem->setFlags(readOnlyFlags);
            valueItem->setFlags(readOnlyFlags);

            table->setItem(row, 0, numberItem);
            table->setItem(row, 1, lengthItem);
            table->setItem(row, 2, valueItem);
            createOperationWidget(row);
        }
    }

    const int pageCount =
        qMax(1, (m_frames.size() + FramesPerPage - 1) / FramesPerPage);
    page.pageSpin->setRange(1, pageCount);
    page.pageSpin->setValue(m_framePage + 1);
    page.pageSpin->setEnabled(!m_frames.isEmpty());
    page.pageCount->setText(QStringLiteral("/ %1 页").arg(pageCount));
    page.prev->setEnabled(m_framePage > 0);
    page.next->setEnabled(m_framePage + 1 < pageCount);
    page.jump->setEnabled(pageCount > 1);
}

void wid_customdatasource::showPreviousFramePage()
{
    if (m_framePage <= 0) {
        return;
    }
    --m_framePage;
    populateFramePage();
}

void wid_customdatasource::showNextFramePage()
{
    const int pageCount =
        qMax(1, (m_frames.size() + FramesPerPage - 1) / FramesPerPage);
    if (m_framePage + 1 >= pageCount) {
        return;
    }
    ++m_framePage;
    populateFramePage();
}

void wid_customdatasource::jumpToFramePage()
{
    const FramePageWidgets page = currentFramePageWidgets();
    const int pageCount =
        qMax(1, (m_frames.size() + FramesPerPage - 1) / FramesPerPage);
    const int targetPage =
        qBound(1, page.pageSpin->value(), pageCount) - 1;
    if (targetPage == m_framePage) {
        page.pageSpin->setValue(m_framePage + 1);
        return;
    }
    m_framePage = targetPage;
    populateFramePage();
}

void wid_customdatasource::handleFrameItemChanged(QTableWidgetItem *item)
{
    if (item == nullptr || (item->column() != 1 && item->column() != 2)) {
        return;
    }

    const int globalIndex = m_framePage * FramesPerPage + item->row();
    if (globalIndex < 0 || globalIndex >= m_frames.size()) {
        return;
    }

    FrameRecord &frame = m_frames[globalIndex];
    if (item->column() == 1) {
        frame.length = item->text();
    } else {
        frame.value = item->text();
    }
}

void wid_customdatasource::removeFrame(int globalIndex)
{
    if (globalIndex < 0 || globalIndex >= m_frames.size()) {
        return;
    }

    // 删除当前页中对应的可见行。即使后续重新分页，用户点击“删除”后
    // 目标行也会立即从表格中消失，而不是只修改底层数据。
    QTableWidget *table = currentFramePageWidgets().table;
    const int visibleRow =
        globalIndex - m_framePage * FramesPerPage;
    if (visibleRow >= 0 && visibleRow < table->rowCount()) {
        QSignalBlocker blocker(table);
        table->removeRow(visibleRow);
    }

    m_frames.removeAt(globalIndex);
    const int pageCount =
        qMax(1, (m_frames.size() + FramesPerPage - 1) / FramesPerPage);
    m_framePage = qBound(0, m_framePage, pageCount - 1);

    // 重新生成当前页，使剩余行的帧序号、操作按钮和分页状态保持一致。
    populateFramePage();
}
