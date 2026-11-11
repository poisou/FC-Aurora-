// 对应界面：wid_customdatasource.ui。
// 本文件负责自定义帧生成、分页、帧编辑以及 AHEAD + 帧数据文件保存。
#include "wid_customdatasource.h"

#include "mytools/binformat.h"
#include "ui_wid_customdatasource.h"

#include <QCheckBox>
#include <QComboBox>
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSaveFile>
#include <QSignalBlocker>
#include <QTableWidget>
#include <QTableWidgetItem>

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
            QString("%1")
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
            QString("%1").arg(static_cast<quint8>(rawByte), 2, 16, QLatin1Char('0')).toUpper());
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
    configureFCTable();

    connect(ui->backButton,
            &QPushButton::clicked,
            this,
            &wid_customdatasource::handleBack);
    connect(ui->generateButton,
            &QPushButton::clicked,
            this,
            &wid_customdatasource::generateFrames);
    connect(ui->clearFramesButton,
            &QPushButton::clicked,
            this,
            &wid_customdatasource::clearFrames);
    connect(ui->saveButton,
            &QPushButton::clicked,
            this,
            &wid_customdatasource::saveDataSource);
    connect(ui->framePrevButton,
            &QPushButton::clicked,
            this,
            &wid_customdatasource::showPreviousFramePage);
    connect(ui->frameNextButton,
            &QPushButton::clicked,
            this,
            &wid_customdatasource::showNextFramePage);
    connect(ui->frameJumpButton,
            &QPushButton::clicked,
            this,
            &wid_customdatasource::jumpToFramePage);
    connect(ui->frameTable,
            &QTableWidget::itemChanged,
            this,
            &wid_customdatasource::handleFrameItemChanged);
    connect(ui->busTypeCombo,
            static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
            this,
            &wid_customdatasource::updateBusTypePage);

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
    Q_UNUSED(index)

    // FC、Aurora 和以太网统一使用相同的帧列表编辑界面。
    // 总线类型只作为数据源属性保存，不再切换不同的参数布局。
    if(index == 0)
    {
        ui->typePageStack->setCurrentWidget(ui->FCPage);
    }
    if(index == 1)
    {
        ui->typePageStack->setCurrentWidget(ui->AuroraPage);
    }

}

void wid_customdatasource::generateFrames()
{
    const int frameCount = ui->frameCountSpinBox->value();

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
        //给ahead赋值
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
    ui->frameTable->setColumnCount(4);
    ui->frameTable->setHorizontalHeaderLabels(
        {QStringLiteral("帧序号"),
         QStringLiteral("帧长"),
         QStringLiteral("值"),
         QStringLiteral("操作")});
    ui->frameTable->verticalHeader()->setVisible(false);
    ui->frameTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->frameTable->setSelectionMode(QAbstractItemView::SingleSelection);
    ui->frameTable->setEditTriggers(
        QAbstractItemView::DoubleClicked | QAbstractItemView::EditKeyPressed);
    ui->frameTable->horizontalHeader()->setSectionResizeMode(
        0, QHeaderView::Fixed);
    ui->frameTable->horizontalHeader()->setSectionResizeMode(
        1, QHeaderView::Fixed);
    ui->frameTable->horizontalHeader()->setSectionResizeMode(
        2, QHeaderView::Stretch);
    ui->frameTable->horizontalHeader()->setSectionResizeMode(
        3, QHeaderView::Fixed);
    ui->frameTable->setColumnWidth(0, 120);
    ui->frameTable->setColumnWidth(1, 220);
    ui->frameTable->setColumnWidth(3, 170);
}

void wid_customdatasource::configureFCTable()
{
    wid_FCData* mfc = new wid_FCData();
    ui->FCPageLayout->addWidget(mfc);
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
    editButton->setStyleSheet(
                QStringLiteral(
                "QPushButton{border:1px solid rgb(70,90,120);color: rgb(146, 182, 202);border-radius:3px;font: 75 11pt '微软雅黑';background-color: rgb(255,255,255);}"
                "QPushButton:hover{color: rgb(25, 118, 210);background-color: rgb(227, 236, 255);};"
               ));
    auto *removeButton = new QPushButton(QStringLiteral("删除"));
    removeButton->setCursor(Qt::PointingHandCursor);
    removeButton->setMinimumSize(54, 26);
    removeButton->setStyleSheet(
        QStringLiteral(
            "QPushButton { background:#FF7070; color:white; border:1px solid #E05252;"
            " border-radius:3px; }"
            "QPushButton:hover { background:#FF5C5C; }"));

    // “编辑”按钮打开独立的数据帧信息对话框，填写 参数和数据值。
    connect(editButton, &QPushButton::clicked, this, [this, globalIndex]() {
        if (globalIndex < 0 || globalIndex >= m_frames.size()) {
            return;
        }
        FrameRecord &frame = m_frames[globalIndex];
        int mtype = ui->busTypeCombo->currentIndex();
        if(mtype == 0)
        {
            //FC
//            wid_FCData
        }
        if(mtype == 1)
        {
            //Aurora格式数据
            AuroraFrameEditData editData = frame.auroraData;
            editData.length = AuroraFieldLayout::DataLength;
            if (frame.dataBytes.isEmpty()) {
                frame.dataBytes.fill('\0', AuroraFieldLayout::DataLength);
            }
            editData.gridValues = hexValues(frame.dataBytes);

            AuroraFrameEditDialog dialog(this);
            dialog.setFrameData(editData);
            if (dialog.exec() != QDialog::Accepted) {
                return;
            }

            frame.auroraData = dialog.frameData();

            // 对话框保存后，将完整 HEX 数据还原成紧凑的字节数组。
            const QStringList savedHexValues = dialog.hexadecimalValues();
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
        }
        if(mtype == 2)
        {
            //TCP
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
    ui->frameTable->setCellWidget(row, 3, container);
}

void wid_customdatasource::populateFramePage()
{
    QTableWidget *table = ui->frameTable;
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
    ui->framePageSpinBox->setRange(1, pageCount);
    ui->framePageSpinBox->setValue(m_framePage + 1);
    ui->framePageSpinBox->setEnabled(!m_frames.isEmpty());
    ui->framePageCountLabel->setText(
        QStringLiteral("/ %1 页").arg(pageCount));
    ui->framePrevButton->setEnabled(m_framePage > 0);
    ui->frameNextButton->setEnabled(m_framePage + 1 < pageCount);
    ui->frameJumpButton->setEnabled(pageCount > 1);
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
    const int pageCount =
        qMax(1, (m_frames.size() + FramesPerPage - 1) / FramesPerPage);
    const int targetPage =
        qBound(1, ui->framePageSpinBox->value(), pageCount) - 1;
    if (targetPage == m_framePage) {
        ui->framePageSpinBox->setValue(m_framePage + 1);
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
    const int visibleRow =
        globalIndex - m_framePage * FramesPerPage;
    if (visibleRow >= 0 && visibleRow < ui->frameTable->rowCount()) {
        QSignalBlocker blocker(ui->frameTable);
        ui->frameTable->removeRow(visibleRow);
    }

    m_frames.removeAt(globalIndex);
    const int pageCount =
        qMax(1, (m_frames.size() + FramesPerPage - 1) / FramesPerPage);
    m_framePage = qBound(0, m_framePage, pageCount - 1);

    // 重新生成当前页，使剩余行的帧序号、操作按钮和分页状态保持一致。
    populateFramePage();
}
