// 对应界面：wid_datasource.ui。
// 本文件负责数据源列表、数据库、MYDATA 文件及下方数据段表格赋值。
#include "wid_datasource.h"

#include "aurorafileimporter.h"
#include "myTools/binformat.h"
#include "datasourcefilecopyworker.h"
#include "datasourceuploaddialog.h"
#include "ui_wid_datasource.h"

#include <QComboBox>
#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QHeaderView>
#include <QItemSelectionModel>
#include <QMessageBox>
#include <QResizeEvent>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QSqlError>
#include <QSqlQuery>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QThread>
#include <QTimer>
#include <QVariant>

namespace {

// 数据源页只缓存文件开头 64 KB 用于快速预览，不把大文件整体读入内存。
constexpr qint64 MaxDataSourcePreview = 64 * 1024;

// 上方数据源记录列表每页固定显示 10 条。
constexpr int DataSourceRowsPerPage = 10;

// 下方字节表格每页固定显示 50 行，每行包含 16 个字节。
constexpr int HexRowsPerPage = 50;

// 三张持久化数据源表的名称。UI 显示的“以太网”对应 TCP 表。
const QString FcSourceTable = QString("table_data_FCsource");
const QString AuroraSourceTable = QString("table_data_Aurorasource");
const QString TcpSourceTable = QString("table_data_TCPsource");

// 将界面中输入的 HEX 文本解析为字节数组。
// 输入支持空格分隔的 "01 AF 7C" 或连续格式 "01AF7C"。
// @param text 用户输入的 HEX 文本。
// @param ok 输出参数；解析成功时设为 true，否则设为 false。
// @return 解析后的原始字节；失败时返回空数组。
QByteArray parseHexBytes(const QString &text, bool *ok)
{
    QString compact;
    compact.reserve(text.size());

    // 忽略用户从其它工具粘贴时夹带的空白字符。
    for (const QChar character : text) {
        if (!character.isSpace()) {
            compact.append(character);
        }
    }

    if (compact.isEmpty() || compact.size() % 2 != 0
            || compact.size() > 16 * 2) {
        *ok = false;
        return QByteArray();
    }

    QByteArray bytes;
    bytes.reserve(compact.size() / 2);
    for (int index = 0; index < compact.size(); index += 2) {
        bool byteOk = false;
        const int value = compact.mid(index, 2).toInt(&byteOk, 16);
        if (!byteOk || value < 0 || value > 0xFF) {
            *ok = false;
            return QByteArray();
        }
        bytes.append(static_cast<char>(value));
    }

    *ok = true;
    return bytes;
}

} // namespace

wid_datasource::wid_datasource(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::wid_datasource)
{
    // 独立创建数据源管理页时也必须注册 Aurora 跨线程结果类型。
    qRegisterMetaType<BinPacket>("BinPacket");
    qRegisterMetaType<QVector<BinPacket>>("QVector<BinPacket>");

    ui->setupUi(this);
    m_cd = new wid_customdatasource();
    connect(m_cd,&wid_customdatasource::backRequested,this,&wid_datasource::backRequested);
    connect(m_cd,&wid_customdatasource::saveRequested,this,&wid_datasource::saveDataSource);

    ui->gridLayout_3->addWidget(m_cd);
    ui->dataSourceTable->setColumnCount(5);
    ui->dataSourceTable->setHorizontalHeaderLabels(
    {QString("数据源名"),
     QString("总线类型"),
     QString("创建时间"),
     QString("数据长度"),
     QString("值")});
    ui->dataSourceTable->verticalHeader()->setVisible(false);
    ui->dataSourceTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->dataSourceTable->setSelectionMode(QAbstractItemView::SingleSelection);
    // 仅“值”列会被设置为可编辑，其它列在填充表格时显式设为只读。
    ui->dataSourceTable->setEditTriggers(
                QAbstractItemView::DoubleClicked
                | QAbstractItemView::SelectedClicked
                | QAbstractItemView::EditKeyPressed);
    ui->dataSourceTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    ui->dataSourceTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Fixed);
    ui->dataSourceTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Fixed);
    ui->dataSourceTable->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Fixed);
    ui->dataSourceTable->horizontalHeader()->setSectionResizeMode(4, QHeaderView::Stretch);
    ui->dataSourceTable->setColumnWidth(1, 100);
    ui->dataSourceTable->setColumnWidth(2, 180);
    ui->dataSourceTable->setColumnWidth(3, 110);

    ui->dataSourceHexTable->setColumnCount(17);
    QStringList headers;
    headers << QString("偏移");
    for (int column = 0; column < 16; ++column) {
        headers << QString("%1").arg(column, 2, 16, QLatin1Char('0')).toUpper();
    }
    ui->dataSourceHexTable->setHorizontalHeaderLabels(headers);
    ui->dataSourceHexTable->verticalHeader()->setVisible(false);
    ui->dataSourceHexTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    ui->dataSourceHexTable->setSelectionMode(QAbstractItemView::SingleSelection);
    ui->dataSourceHexTable->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
    ui->dataSourceHexTable->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    ui->dataSourceHexTable->horizontalHeader()->setStretchLastSection(false);
    ui->dataSourceHexTable->horizontalHeader()->setSectionResizeMode(
                0, QHeaderView::Fixed);
    ui->dataSourceHexTable->setColumnWidth(0, 82);
    for (int column = 1; column < 17; ++column) {
        ui->dataSourceHexTable->horizontalHeader()->setSectionResizeMode(
                    column, QHeaderView::Fixed);
    }

    connect(ui->uploadButton, &QPushButton::clicked, this, &wid_datasource::chooseFiles);
    connect(ui->customButton,
            &QPushButton::clicked,
            this,
            &wid_datasource::openCustomDataSource);
    connect(ui->deleteButton, &QPushButton::clicked, this, &wid_datasource::deleteSelectedData);
    connect(ui->dataSourceTable,
            &QTableWidget::itemSelectionChanged,
            this,
            &wid_datasource::refreshHexPreview);
    connect(ui->dataSourceTable,
            &QTableWidget::itemChanged,
            this,
            &wid_datasource::handleDataSourceValueChanged);
    connect(ui->saveDataButton,
            &QPushButton::clicked,
            this,
            &wid_datasource::saveDataSourceChanges);
    connect(ui->displayBaseCombo,
            static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
            this,
            &wid_datasource::changeDisplayBase);
    connect(ui->sourcePrevButton,
            &QPushButton::clicked,
            this,
            &wid_datasource::showPreviousSourcePage);
    connect(ui->sourceNextButton,
            &QPushButton::clicked,
            this,
            &wid_datasource::showNextSourcePage);
    connect(ui->sourceJumpButton,
            &QPushButton::clicked,
            this,
            &wid_datasource::jumpToSourcePage);
    connect(ui->hexPrevButton,
            &QPushButton::clicked,
            this,
            &wid_datasource::showPreviousHexPage);
    connect(ui->hexNextButton,
            &QPushButton::clicked,
            this,
            &wid_datasource::showNextHexPage);
    connect(ui->hexJumpButton,
            &QPushButton::clicked,
            this,
            &wid_datasource::jumpToHexPage);

    // 先建立数据库结构，再读取历史记录，最后一次性刷新列表。
    initializeDatabase();
    loadPersistedData();
    updateSummary();
    populateDataSourcePage();
}

wid_datasource::~wid_datasource()
{
    // 复制任务支持取消。关闭窗口时先请求取消并等待线程退出，
    // 防止 QThread 在仍然运行时被销毁。
    for (const QPointer<DataSourceFileCopyWorker> &worker : m_copyWorkers) {
        if (worker) {
            worker->requestCancel();
        }
    }
    for (QThread *thread : m_copyWorkers.keys()) {
        thread->quit();
        thread->wait();
    }

    // 窗口销毁时先请求取消所有 Aurora 后台扫描，避免关闭程序后
    // 工作线程仍在读取文件。quit() 是线程安全的，即使扫描正在执行，
    // 也会在解析器返回后立即退出事件循环。
    for (const QPointer<BinFileImporterBase> &importer : m_lengthParsers) {
        if (importer) {
            importer->requestCancel();
        }
    }
    for (QThread *thread : m_lengthParseThreads) {
        thread->quit();
        thread->wait();
    }

    // QSqlDatabase 对象必须先释放，之后才能安全移除连接。
    m_database.close();
    m_database = QSqlDatabase();
    if (!m_databaseConnectionName.isEmpty()) {
        QSqlDatabase::removeDatabase(m_databaseConnectionName);
    }

    delete ui;
}

//保存自定义数据
void wid_datasource::saveDataSource(const QString &name,
                                    const QString &busType,
                                    const QString &filePath,
                                    const QDateTime &createdAt)
{
    if(!addGeneratedDataSource(name, busType, filePath, createdAt))
    {
        return;
    }
    Dlog_info* mlog = new Dlog_info(2,"保存完成!");
    mlog->exec();
    ui->stackedWidget->setCurrentIndex(0);
}

bool wid_datasource::addGeneratedDataSource(const QString &name,
                                            const QString &busType,
                                            const QString &filePath,
                                            const QDateTime &createdAt)
{
    const QFileInfo fileInfo(filePath);
    if (!fileInfo.exists() || !fileInfo.isFile()) {
        QMessageBox::warning(
                    this,
                    QString("保存失败"),
                    QString("生成的数据文件不存在：%1")
                    .arg(QDir::toNativeSeparators(filePath)));
        return false;
    }
    if (!m_database.isValid() || !m_database.isOpen()) {
        QFile::remove(filePath);
        QMessageBox::warning(
                    this,
                    QString("保存失败"),
                    QString("数据库未就绪，生成文件已删除。"));
        return false;
    }

    DataSourceRecord record;
    // 自定义页面生成的数据源记录字段说明：
    // id         : 仅用于本次运行期间定位异步任务和表格行，不写入数据库；
    // name       : 列表“数据源名”；
    // busType    : 决定写入哪张数据库表；
    // createdAt  : 列表“创建时间”，同时作为数据库 date；
    // filePath   : 数据库 filePath，也是 MYDATA 中 BIN 的真实路径；
    // dataLength : 列表“数据长度”，当前显示文件总字节数。
    record.id = m_nextRecordId++;
    record.name = name;
    record.busType = busType;
    record.createdAt = createdAt.isValid()
            ? createdAt
            : QDateTime::currentDateTime();
    record.filePath = fileInfo.absoluteFilePath();
    record.hasFile = true;
    record.dataLength = fileInfo.size();

    QFile file(record.filePath);
    if (file.open(QIODevice::ReadOnly)) {
        record.previewData = file.read(MaxDataSourcePreview);
    }

    if (!m_database.transaction()) {
        QFile::remove(filePath);
        QMessageBox::warning(
                    this,
                    QString("保存失败"),
                    QString("无法启动数据库事务：%1")
                    .arg(m_database.lastError().text()));
        return false;
    }

    // 同名数据源再次保存时会覆盖 BIN 文件，因此先删除相同路径的旧行，
    // 再插入最新记录，避免列表中反复出现同一文件。
    if (!deleteDataSourceRecord(record)) {
        m_database.rollback();
        QFile::remove(filePath);
        return false;
    }
    if (!insertDataSourceRecord(record)) {
        m_database.rollback();
        QFile::remove(filePath);
        return false;
    }
    if (!m_database.commit()) {
        QMessageBox::warning(
                    this,
                    QString("保存失败"),
                    QString("数据库事务提交失败：%1")
                    .arg(m_database.lastError().text()));
        m_database.rollback();
        QFile::remove(filePath);
        return false;
    }

    appendRecord(record);
    updateSummary();
    return true;
}

void wid_datasource::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    QTimer::singleShot(0, this, &wid_datasource::updateHexColumnWidths);
}

void wid_datasource::updateHexColumnWidths()
{
    if (ui->dataSourceHexTable->columnCount() != 17) {
        return;
    }

    auto *header = ui->dataSourceHexTable->horizontalHeader();
    const int dataWidth =
            ui->dataSourceHexTable->viewport()->width() - header->sectionSize(0);
    const int columnWidth = qMax(28, dataWidth / 16);
    for (int column = 1; column < 17; ++column) {
        header->resizeSection(column, columnWidth);
    }
}

bool wid_datasource::initializeDatabase()
{
    const QString executableDirectory = QCoreApplication::applicationDirPath();
    const QString databaseDirectory =QDir(executableDirectory).filePath(QString("mydb"));
    m_dataDirectoryPath =
            QDir(executableDirectory).filePath(QString("MYDATA"));
    m_databasePath =
            QDir(databaseDirectory).filePath(QString("WT_FCData.db"));

    // mydb 和 MYDATA 都位于可执行文件同级目录。
    // mkpath() 在目录已存在时不会报错。
    if (!QDir().mkpath(databaseDirectory)
            || !QDir().mkpath(m_dataDirectoryPath)) {
        QMessageBox::warning(
                    this,
                    QString("目录创建失败"),
                    QString("无法创建 mydb 或 MYDATA 目录。"));
        return false;
    }

    // 每个 wid_datasource 实例使用独立连接名，避免与其它页面冲突。
    m_databaseConnectionName =
            QString("wid_datasource_%1")
            .arg(reinterpret_cast<quintptr>(this), 0, 16);
    m_database = QSqlDatabase::addDatabase(
                QString("QSQLITE"), m_databaseConnectionName);
    m_database.setDatabaseName(m_databasePath);

    if (!m_database.open()) {
        QMessageBox::warning(
                    this,
                    QString("数据库打开失败"),
                    QString("无法打开数据库：%1\n%2")
                    .arg(QDir::toNativeSeparators(m_databasePath),
                         m_database.lastError().text()));
        return false;
    }

    // 三张表按需求使用固定字段：name、date、filePath。
    // 如果数据库文件或任意一张表不存在，这里会自动创建。
    const QStringList tables = {FcSourceTable, AuroraSourceTable, TcpSourceTable};
    for (const QString &tableName : tables) {
        if (!createDataSourceTable(tableName)) {
            QMessageBox::warning(
                        this,
                        QString("数据表创建失败"),
                        QString("无法创建数据表 %1：%2")
                        .arg(tableName, m_database.lastError().text()));
            m_database.close();
            return false;
        }
    }

    return true;
}

bool wid_datasource::createDataSourceTable(const QString &tableName)
{
    if (m_database.isValid() == false || !m_database.isOpen()) {
        return false;
    }

    QSqlQuery query(m_database);
    const QString sql =
            QString(
                "CREATE TABLE IF NOT EXISTS %1 ("
                "name TEXT NOT NULL,"
                "date TEXT,"
                "filePath TEXT NOT NULL"
                ")")
            .arg(tableName);
    return query.exec(sql);
}

void wid_datasource::loadPersistedData()
{
    if (!m_database.isValid() || !m_database.isOpen()) {
        return;
    }

    // 按固定顺序读取三张表。“以太网”数据来自 TCP 表，
    // 但界面仍按新的总线类型名称显示为“以太网”。
    const QStringList tables = {
        FcSourceTable, AuroraSourceTable, TcpSourceTable};
    const QStringList busTypes = {
        QString("FC"),
        QString("Aurora"),
        QString("以太网")};
    QVector<quint64> pendingLengthRecordIds;

    for (int tableIndex = 0; tableIndex < tables.size(); ++tableIndex) {
        const QString &tableName = tables.at(tableIndex);
        const QString &busType = busTypes.at(tableIndex);
        QSqlQuery query(m_database);
        if (!query.exec(
                    QString(
                        "SELECT name, date, filePath FROM %1 ORDER BY rowid")
                    .arg(tableName))) {
            QMessageBox::warning(
                        this,
                        QString("数据读取失败"),
                        QString("无法读取数据表 %1：%2")
                        .arg(tableName, query.lastError().text()));
            continue;
        }

        while (query.next()) {
            DataSourceRecord record;
            record.id = m_nextRecordId++;
            record.name = query.value(0).toString();
            record.busType = busType;
            record.filePath =QDir::cleanPath(query.value(2).toString());
            record.hasFile = true;

            // date 以 ISO 文本存储；同时兼容手工写入的显示格式。
            const QString dateText = query.value(1).toString();
            record.createdAt =
                    QDateTime::fromString(dateText, Qt::ISODate);
            if (!record.createdAt.isValid()) {
                record.createdAt = QDateTime::fromString(
                            dateText, QString("yyyy-MM-dd HH:mm:ss"));
            }

            // 数据库只保存名称、日期和路径。预览内容与数据长度在启动时
            // 根据 MYDATA 中的实际文件重新读取，避免数据库保存冗余大字段。
            const QFileInfo dataInfo(record.filePath);
            if (dataInfo.exists() && dataInfo.isFile()) {
                QFile dataFile(record.filePath);
                if (dataFile.open(QIODevice::ReadOnly)) {
                    record.previewData = dataFile.read(MaxDataSourcePreview);
                }
            }
            const bool isAurora = busType == QString("Aurora");
            record.lengthState =
                    isAurora && dataInfo.exists() && dataInfo.isFile()
                    ? LengthState::Parsing : LengthState::Ready;
            record.dataLength =
                    isAurora ? 0 : (dataInfo.exists() ? dataInfo.size() : 0);

            m_records.append(record);
            if (record.hasFile && dataInfo.exists() && dataInfo.isFile()) {
                pendingLengthRecordIds.append(record.id);
            }
        }
    }

    // 历史 Aurora 记录与本次运行上传的记录使用同一后台解析流程。
    for (quint64 recordId : pendingLengthRecordIds) {
        for (const DataSourceRecord &record : m_records) {
            if (record.id == recordId) {
                startDataLengthParse(record.id,
                                     record.busType,
                                     record.filePath,
                                     QFileInfo(record.filePath).size());
                break;
            }
        }
    }
}

QString wid_datasource::tableNameForBusType(const QString &busType) const
{
    if (busType == QString("FC")) {
        return FcSourceTable;
    }
    if (busType == QString("Aurora")) {
        return AuroraSourceTable;
    }
    if (busType == QString("以太网")) {
        return TcpSourceTable;
    }
    return QString();
}

QString wid_datasource::uniqueDataFilePath(
        const QString &sourceFileName) const
{
    const QFileInfo sourceInfo(sourceFileName);
    const QString baseName = sourceInfo.completeBaseName();
    const QString suffix = sourceInfo.suffix();

    QString candidateName = sourceInfo.fileName();
    int sequence = 1;
    auto isReservedOrExisting = [this](const QString &fileName) {
        const QString absolutePath =
                QDir(m_dataDirectoryPath).filePath(fileName);
        return QFileInfo::exists(absolutePath)
                || m_reservedDataPaths.contains(absolutePath.toCaseFolded());
    };

    while (isReservedOrExisting(candidateName)) {
        const QString suffixText =
                suffix.isEmpty() ? QString() : QString(".%1").arg(suffix);
        candidateName =
                QString("%1_%2%3").arg(baseName).arg(sequence).arg(suffixText);
        ++sequence;
    }

    return QDir(m_dataDirectoryPath).filePath(candidateName);
}

void wid_datasource::startDataSourceFileCopy(const DataSourceRecord &record,
                                             const QString &sourcePath)
{
    auto *thread = new QThread(this);
    auto *worker = new DataSourceFileCopyWorker;
    worker->moveToThread(thread);
    m_copyWorkers.insert(thread, worker);
    // 在后台任务完成前预留目标路径，防止连续上传同名文件时生成相同目标名。
    const QString reservedPath =
            QFileInfo(record.filePath).absoluteFilePath().toCaseFolded();
    m_reservedDataPaths.insert(reservedPath);
    const QString destinationPath = record.filePath;

    connect(thread,
            &QThread::started,
            worker,
            [worker, sourcePath, destinationPath]() {
        worker->copyFile(sourcePath, destinationPath);
    });

    connect(worker,
            &DataSourceFileCopyWorker::finished,
            this,
            [this, record](const QString &destinationPath) {
        DataSourceRecord completedRecord = record;
        completedRecord.filePath = destinationPath;


        const bool isAurora =completedRecord.busType == QString("Aurora");
        const bool isFc =completedRecord.busType == QString("Fc");
        const bool isTcp =completedRecord.busType == QString("Tcp");
        const QFileInfo copiedInfo(destinationPath);
        completedRecord.lengthState =
                isAurora ? LengthState::Parsing : LengthState::Ready;
        completedRecord.dataLength =
                isAurora ? 0 : copiedInfo.size();

        // 只有数据库写入成功后才显示记录，保证“界面里有记录”
        // 与“数据库中有记录”保持一致。
        if (!insertDataSourceRecord(completedRecord)) {
            QFile::remove(destinationPath);
            return;
        }
        appendRecord(completedRecord);
        startDataLengthParse(completedRecord.id,
                             completedRecord.busType,
                             completedRecord.filePath,
                             copiedInfo.size());
        if(isFc)
        {

        }
        if(isTcp)
        {

        }
    });

    connect(worker,
            &DataSourceFileCopyWorker::failed,
            this,
            [this, sourcePath](const QString &message) {
        QMessageBox::warning(
                    this,
                    QString("文件保存失败"),
                    QString("复制文件失败。\n源文件：%1\n%2")
                    .arg(QDir::toNativeSeparators(sourcePath), message));
    });

    // 任意终态都结束工作线程；worker 在线程退出时释放。
    connect(worker, &DataSourceFileCopyWorker::finished, thread, &QThread::quit);
    connect(worker, &DataSourceFileCopyWorker::failed, thread, &QThread::quit);
    connect(worker, &DataSourceFileCopyWorker::cancelled, thread, &QThread::quit);
    connect(thread, &QThread::finished, worker, &QObject::deleteLater);
    connect(thread, &QThread::finished, this, [this, thread, reservedPath]() {
        m_copyWorkers.remove(thread);
        m_reservedDataPaths.remove(reservedPath);
        thread->deleteLater();
    });

    thread->start();
}

bool wid_datasource::insertDataSourceRecord(const DataSourceRecord &record)
{
    const QString tableName = tableNameForBusType(record.busType);
    if (tableName.isEmpty() || !m_database.isValid() || !m_database.isOpen()) {
        QMessageBox::warning(
                    this,
                    QString("数据库写入失败"),
                    QString("数据库未就绪或总线类型无效。"));
        return false;
    }

    QSqlQuery query(m_database);
    query.prepare(
                QString(
                    "INSERT INTO %1 (name, date, filePath) VALUES (?, ?, ?)")
                .arg(tableName));
    query.addBindValue(record.name);
    query.addBindValue(
                record.createdAt.isValid()
                ? record.createdAt.toString(Qt::ISODate)
                : QString());
    query.addBindValue(QDir::toNativeSeparators(record.filePath));

    if (!query.exec()) {
        QMessageBox::warning(
                    this,
                    QString("数据库写入失败"),
                    QString("无法写入数据表 %1：%2")
                    .arg(tableName, query.lastError().text()));
        return false;
    }

    return true;
}

bool wid_datasource::deleteDataSourceRecord(const DataSourceRecord &record)
{
    if (!record.hasFile) {
        return true;
    }

    const QString tableName = tableNameForBusType(record.busType);
    if (tableName.isEmpty() || !m_database.isValid() || !m_database.isOpen()) {
        QMessageBox::warning(
                    this,
                    QString("数据库删除失败"),
                    QString("数据库未就绪或总线类型无效。"));
        return false;
    }

    QSqlQuery query(m_database);
    query.prepare(
                QString("DELETE FROM %1 WHERE filePath = ?").arg(tableName));
    query.addBindValue(QDir::toNativeSeparators(record.filePath));
    if (!query.exec()) {
        QMessageBox::warning(
                    this,
                    QString("数据库删除失败"),
                    QString("无法从数据表 %1 删除记录：%2")
                    .arg(tableName, query.lastError().text()));
        return false;
    }

    return true;
}

bool wid_datasource::deleteMyDataFile(const DataSourceRecord &record)
{
    if (!record.hasFile || record.filePath.isEmpty()) {
        return true;
    }

    const QFileInfo targetInfo(record.filePath);
    if (!targetInfo.exists()) {
        // 数据库中存在但物理文件已缺失时，仍允许清除对应列表和数据库记录。
        return true;
    }

    const QFileInfo dataDirectoryInfo(m_dataDirectoryPath);
    QString dataRoot = dataDirectoryInfo.canonicalFilePath();
    QString targetPath = targetInfo.canonicalFilePath();
    if (dataRoot.isEmpty()) {
        dataRoot = QDir::cleanPath(dataDirectoryInfo.absoluteFilePath());
    }
    if (targetPath.isEmpty()) {
        targetPath = QDir::cleanPath(targetInfo.absoluteFilePath());
    }
    dataRoot = QDir::cleanPath(dataRoot);
    targetPath = QDir::cleanPath(targetPath);

    // Windows 路径大小写不敏感，因此统一使用不区分大小写的前缀比较。
    // 末尾分隔符可防止 MYDATA_OTHER 被误判为 MYDATA 的子目录。
    const QString expectedPrefix =
            dataRoot.endsWith(QLatin1Char('/'))
            ? dataRoot
            : dataRoot + QLatin1Char('/');
#ifdef Q_OS_WIN
    const Qt::CaseSensitivity pathCase = Qt::CaseInsensitive;
#else
    const Qt::CaseSensitivity pathCase = Qt::CaseSensitive;
#endif
    if (!targetPath.startsWith(expectedPrefix, pathCase)) {
        QMessageBox::warning(
                    this,
                    QString("拒绝删除文件"),
                    QString("目标文件不在 MYDATA 目录中，出于安全考虑已取消删除：\n%1")
                    .arg(QDir::toNativeSeparators(targetPath)));
        return false;
    }

    if (!QFile::remove(targetPath)) {
        QMessageBox::warning(
                    this,
                    QString("文件删除失败"),
                    QString("无法删除 MYDATA 中的文件：\n%1")
                    .arg(QDir::toNativeSeparators(targetPath)));
        return false;
    }

    return true;
}

void wid_datasource::chooseFiles()
{
    DataSourceUploadDialog dialog(this);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    const QFileInfo sourceInfo(dialog.filePath());
    const QString sourcePath = sourceInfo.absoluteFilePath();
    QFile sourceFile(sourcePath);
    if (!sourceFile.open(QIODevice::ReadOnly)) {
        QMessageBox::warning(
                    this,
                    QString("读取失败"),
                    QString("无法读取文件内容：%1").arg(sourceFile.errorString()));
        return;
    }

    DataSourceRecord record;
    // 上传文件记录的赋值来源：
    // name      <- 上传对话框；
    // busType   <- 上传对话框总线类型；
    // createdAt <- 原文件的文件系统创建时间；
    // filePath  <- 复制到 MYDATA 后的目标路径；
    // preview   <- 原文件前 64 KB，仅用于列表预览。
    record.id = m_nextRecordId++;
    record.name = dialog.dataSourceName();
    record.busType = dialog.fileType();
    // 创建时间取文件系统记录的原始创建时间，而不是文件加入列表的时间。
    // 如果文件系统无法提供创建时间，createdAt 保持无效，界面显示“—”，
    // 该文件也不会参与最小值/最大值时间范围计算。
    record.createdAt = sourceInfo.created();
    record.hasFile = true;

    // 数据库保存的是复制后文件路径，因此记录从一开始就指向 MYDATA。
    record.filePath = uniqueDataFilePath(sourceInfo.fileName());
    record.dataLength = sourceInfo.size();
    record.previewData = sourceFile.read(MaxDataSourcePreview);
    sourceFile.close();

    // 校验总线类型能映射到数据库表，避免复制完成后再发现类型无效。
    if (tableNameForBusType(record.busType).isEmpty()) {
        QMessageBox::warning(
                    this,
                    QString("类型无效"),
                    QString("无法识别选择的总线类型：%1").arg(record.busType));
        return;
    }

    // 大数据文件复制在后台线程执行。复制成功后才会写入数据库并加入列表。
    startDataSourceFileCopy(record, sourcePath);
}

void wid_datasource::openCustomDataSource()
{
    // 数据源页面本身不创建自定义记录，只通知外层页面栈切换编辑界面。
    //    emit customDataSourceRequested();
    ui->stackedWidget->setCurrentIndex(1);
}

void wid_datasource::deleteSelectedData()
{
    const int pageRow = ui->dataSourceTable->currentRow();
    if (pageRow < 0) {
        return;
    }

    const int globalRow = m_sourcePage * DataSourceRowsPerPage + pageRow;
    if (globalRow < 0 || globalRow >= m_records.size()) {
        return;
    }

    const DataSourceRecord record = m_records.at(globalRow);

    // 如果 Aurora 解析线程仍占用该文件，先取消并等待线程结束，
    // 否则 Windows 上可能因为文件句柄未释放而无法删除物理文件。
    cancelDataLengthParse(record.id);
    QThread *parseThread = m_lengthParseThreads.value(record.id, nullptr);
    if (parseThread != nullptr && parseThread->isRunning()) {
        parseThread->quit();
        parseThread->wait();
    }

    // 文件记录使用数据库事务包住“删数据库行 + 删 MYDATA 文件”。
    // 任一步骤失败就回滚数据库，列表记录保持不变，避免数据状态不一致。
    const bool needsTransaction =
            record.hasFile && m_database.isValid() && m_database.isOpen();
    if (needsTransaction && !m_database.transaction()) {
        QMessageBox::warning(
                    this,
                    QString("删除失败"),
                    QString("无法启动数据库删除事务：%1")
                    .arg(m_database.lastError().text()));
        return;
    }

    if (!deleteDataSourceRecord(record)) {
        if (needsTransaction) {
            m_database.rollback();
        }
        return;
    }

    if (!deleteMyDataFile(record)) {
        if (needsTransaction) {
            m_database.rollback();
        }
        return;
    }

    if (needsTransaction && !m_database.commit()) {
        QMessageBox::warning(
                    this,
                    QString("删除失败"),
                    QString("数据库事务提交失败：%1")
                    .arg(m_database.lastError().text()));
        return;
    }

    m_records.remove(globalRow);
    const int pageCount = qMax(
                1,
                (m_records.size() + DataSourceRowsPerPage - 1) / DataSourceRowsPerPage);
    m_sourcePage = qBound(0, m_sourcePage, pageCount - 1);

    // 删除后只刷新上方数据源列表，不自动选中其它行。
    // populateDataSourcePage(false) 会同步清空下方数据段列表，
    // 避免用户误以为被删除记录的数据仍然处于选中状态。
    populateDataSourcePage(false);
    updateSummary();
}

void wid_datasource::appendRecord(const DataSourceRecord &record)
{
    m_records.append(record);
    m_sourcePage = (m_records.size() - 1) / DataSourceRowsPerPage;
    populateDataSourcePage();
    updateSummary();
}

void wid_datasource::startDataLengthParse(quint64 recordId,
                                          const QString &busType,
                                          const QString &filePath,
                                          qint64 fileSize)
{
    // 三种类型共用入口：
    // - Aurora 使用后台解析器累计 n × 1024；
    // - FC/TCP 直接采用文件总字节数。
    if (busType == QString("Aurora")) {
        startAuroraLengthParse(recordId, filePath);
        return;
    }

    applyDataLengthResult(recordId, fileSize);
}

void wid_datasource::startAuroraLengthParse(quint64 recordId,
                                            const QString &filePath)
{
    // 每条 Aurora 记录拥有独立的 QThread 和 AuroraFileImporter。
    // 文件读取、包头解析及数据段跳过都在工作线程中完成，
    // 主线程只负责接收最终累计长度并刷新对应单元格。
    auto *thread = new QThread(this);
    auto *importer = new AuroraFileImporter;
    importer->moveToThread(thread);

    m_lengthParseThreads.insert(recordId, thread);
    m_lengthParsers.insert(recordId, importer);

    // 线程事件循环启动后再执行阻塞式扫描，确保该对象完整属于工作线程。
    connect(thread, &QThread::started, importer, [importer, filePath]() {
        importer->importFile(filePath);
    });

    // AuroraFileImporter 已完成逐包格式校验。这里累加每包的真实数据段长度：
    // payloadLength = n × 1024，不包含每包固定 6 字节包头。
    connect(importer,
            &BinFileImporterBase::finished,
            this,
            [this, recordId](const QVector<BinPacket> &packets,
            const QString &,
            qint64) {
        qint64 totalDataLength = 0;
        for (const BinPacket &packet : packets) {
            totalDataLength += packet.payloadLength;
        }
        applyDataLengthResult(recordId, totalDataLength);
    });

    connect(importer,
            &BinFileImporterBase::failed,
            this,
            [this, recordId, filePath](const QString &message) {
        // 如果记录已经先被删除，applyDataLengthResult() 返回 false，
        // 此时不再向用户弹出与已删除记录相关的错误提示。
        if (applyDataLengthResult(recordId, 0, message)) {
            QMessageBox::warning(
                        this,
                        QString("Aurora 解析失败"),
                        QString("文件：%1\n%2")
                        .arg(QFileInfo(filePath).fileName(), message));
        }
    });

    connect(importer,
            &BinFileImporterBase::cancelled,
            this,
            [this, recordId]() {
        // 取消通常发生在删除记录或关闭窗口时。
        // 记录若仍存在，保留“解析失败”状态以便用户识别未完成项。
        applyDataLengthResult(
                    recordId, 0, QString("Aurora 数据长度解析已取消。"));
    });

    // 任意终态都结束线程事件循环。工作对象在对应线程结束时释放。
    connect(importer, &BinFileImporterBase::finished, thread, &QThread::quit);
    connect(importer, &BinFileImporterBase::failed, thread, &QThread::quit);
    connect(importer, &BinFileImporterBase::cancelled, thread, &QThread::quit);
    connect(thread, &QThread::finished, importer, &QObject::deleteLater);

    // 清理该记录对应的线程和解析器索引。
    // QThread 使用 deleteLater()，避免在 finished 信号栈内直接销毁自身。
    connect(thread, &QThread::finished, this, [this, recordId, thread]() {
        m_lengthParseThreads.remove(recordId);
        m_lengthParsers.remove(recordId);
        thread->deleteLater();
    });

    thread->start();
}

bool wid_datasource::applyDataLengthResult(quint64 recordId,
                                           qint64 dataLength,
                                           const QString &errorMessage)
{
    for (DataSourceRecord &record : m_records) {
        if (record.id != recordId) {
            continue;
        }

        record.lengthState =
                errorMessage.isEmpty() ? LengthState::Ready : LengthState::Failed;
        record.lengthError = errorMessage;
        record.dataLength = errorMessage.isEmpty() ? dataLength : 0;
        refreshDataLengthCell(recordId);
        return true;
    }

    // 记录可能已被用户删除，此时后台结果不再需要显示。
    return false;
}

void wid_datasource::refreshDataLengthCell(quint64 recordId)
{
    int globalRow = -1;
    for (int index = 0; index < m_records.size(); ++index) {
        if (m_records.at(index).id == recordId) {
            globalRow = index;
            break;
        }
    }
    if (globalRow < 0) {
        return;
    }

    // 解析完成的记录不一定位于当前分页，只有当前页才需要更新控件。
    const int firstRecord = m_sourcePage * DataSourceRowsPerPage;
    const int pageRow = globalRow - firstRecord;
    QTableWidget *table = ui->dataSourceTable;
    if (pageRow < 0 || pageRow >= table->rowCount()) {
        return;
    }

    QTableWidgetItem *lengthItem = table->item(pageRow, 3);
    if (!lengthItem) {
        lengthItem = new QTableWidgetItem;
        lengthItem->setTextAlignment(Qt::AlignCenter);
        table->setItem(pageRow, 3, lengthItem);
    }
    const DataSourceRecord &record = m_records.at(globalRow);
    lengthItem->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
    lengthItem->setText(dataLengthText(record));

    // Aurora 解析完成后，允许用户再次编辑该记录的“值”列。
    QTableWidgetItem *valueItem = table->item(pageRow, 4);
    if (valueItem) {
        Qt::ItemFlags flags = Qt::ItemIsEnabled | Qt::ItemIsSelectable;
        if (record.hasFile && !record.previewData.isEmpty()
                && record.lengthState != LengthState::Parsing) {
            flags |= Qt::ItemIsEditable;
        }
        // setFlags() 也会触发 itemChanged，需要屏蔽，避免仅切换编辑状态
        // 就被误判成用户修改了“值”内容。
        QSignalBlocker blocker(table);
        valueItem->setFlags(flags);
    }
}

QString wid_datasource::dataLengthText(const DataSourceRecord &record) const
{
    if (record.lengthState == LengthState::Parsing) {
        return QString("解析中…");
    }
    if (record.lengthState == LengthState::Failed) {
        return QString("解析失败");
    }
    return QString::number(record.dataLength);
}

void wid_datasource::cancelDataLengthParse(quint64 recordId)
{
    BinFileImporterBase *importer = m_lengthParsers.value(recordId);
    if (importer) {
        importer->requestCancel();
    }
}

void wid_datasource::populateDataSourcePage(bool selectFirstRow)
{
    auto *table = ui->dataSourceTable;
    {
        QSignalBlocker blocker(table);
        table->clearContents();
        table->setRowCount(0);

        if (!m_records.isEmpty()) {
            const int pageCount =
                    (m_records.size() + DataSourceRowsPerPage - 1) / DataSourceRowsPerPage;
            m_sourcePage = qBound(0, m_sourcePage, pageCount - 1);

            const int firstRecord = m_sourcePage * DataSourceRowsPerPage;
            const int endRecord =
                    qMin(firstRecord + DataSourceRowsPerPage, m_records.size());
            const int rows = endRecord - firstRecord;
            table->setRowCount(rows);

            for (int row = 0; row < rows; ++row) {
                const DataSourceRecord &record = m_records.at(firstRecord + row);
                const QString preview = record.previewData.isEmpty()
                        ? QString("—")
                        : BinFormat::toHex(
                              record.previewData.left(16));
                auto *nameItem = new QTableWidgetItem(record.name);
                auto *busItem = new QTableWidgetItem(record.busType);
                const QString creationTimeText =
                        record.createdAt.isValid()
                        ? record.createdAt.toString(QString("yyyy-MM-dd HH:mm:ss"))
                        : QString("—");
                auto *timeItem = new QTableWidgetItem(creationTimeText);
                auto *lengthItem =new QTableWidgetItem(dataLengthText(record));
                auto *valueItem = new QTableWidgetItem(preview);
                // 数据浏览表格列赋值对应关系：
                // 0=数据源名，1=总线类型，2=创建时间，3=数据长度，4=值。
                // preview 只取 previewData 前 16 字节并转换为 HEX。
                const Qt::ItemFlags readOnlyFlags =
                        Qt::ItemIsEnabled | Qt::ItemIsSelectable;
                nameItem->setFlags(readOnlyFlags);
                busItem->setFlags(readOnlyFlags);
                timeItem->setFlags(readOnlyFlags);
                lengthItem->setFlags(readOnlyFlags);
                valueItem->setFlags(readOnlyFlags);
                if (record.hasFile && !record.previewData.isEmpty()
                        && record.lengthState != LengthState::Parsing) {
                    valueItem->setFlags(readOnlyFlags | Qt::ItemIsEditable);
                }
                busItem->setTextAlignment(Qt::AlignCenter);
                lengthItem->setTextAlignment(Qt::AlignCenter);
                table->setItem(row, 0, nameItem);
                table->setItem(row, 1, busItem);
                table->setItem(row, 2, timeItem);
                table->setItem(row, 3, lengthItem);
                table->setItem(row, 4, valueItem);
            }
        }
    }

    updateSourcePagination();
    m_hexPage = 0;
    if (selectFirstRow && table->rowCount() > 0) {
        table->selectRow(0);
    } else {
        // 删除记录或调用方明确要求不选中时，清除当前行及其选择状态。
        // 使用 QSignalBlocker 防止 clearSelection() 触发额外的数据段刷新。
        QSignalBlocker blocker(table);
        table->clearSelection();
        table->setCurrentCell(-1, -1);
    }
    populateHexTable();
}

void wid_datasource::updateSourcePagination()
{
    const int pageCount = m_records.isEmpty()
            ? 0
            : (m_records.size() + DataSourceRowsPerPage - 1)
              / DataSourceRowsPerPage;
    if (pageCount == 0) {
        ui->pageInfoLabel->setText(QString("—"));
        ui->sourcePageLabel->setText(QString("— / —"));
        ui->sourceJumpSpinBox->setRange(1, 1);
        ui->sourceJumpSpinBox->setValue(1);
        ui->sourceJumpSpinBox->setEnabled(false);
        ui->sourceJumpButton->setEnabled(false);
        ui->sourcePrevButton->setEnabled(false);
        ui->sourceNextButton->setEnabled(false);
        return;
    }

    const int firstRecord = m_sourcePage * DataSourceRowsPerPage;
    const int lastRecord =
            qMin(firstRecord + DataSourceRowsPerPage, m_records.size());
    ui->pageInfoLabel->setText(
                QString("%1-%2 / %3")
                .arg(firstRecord + 1)
                .arg(lastRecord)
                .arg(m_records.size()));
    ui->sourcePageLabel->setText(
                QString("第 %1 / %2 页").arg(m_sourcePage + 1).arg(pageCount));
    ui->sourceJumpSpinBox->setRange(1, pageCount);
    ui->sourceJumpSpinBox->setValue(m_sourcePage + 1);
    ui->sourceJumpSpinBox->setEnabled(true);
    ui->sourceJumpButton->setEnabled(true);
    ui->sourcePrevButton->setEnabled(m_sourcePage > 0);
    ui->sourceNextButton->setEnabled(m_sourcePage + 1 < pageCount);
}

void wid_datasource::updateSummary()
{
    ui->recordCountValue->setText(QString::number(m_records.size()));

    if (m_records.isEmpty()) {
        ui->timeRangeValue->setText(QString("—"));
        ui->busTypesValue->setText(QString("—"));
        return;
    }

    // 时间范围针对“数据浏览列表中的所有文件”，因此只统计 hasFile 为 true
    // 的记录，并按文件系统创建时间比较，而不是按文件加入列表的顺序比较。
    QDateTime firstFileTime;
    QDateTime lastFileTime;
    QStringList busTypes;
    for (const DataSourceRecord &record : m_records) {
        if (!busTypes.contains(record.busType)) {
            busTypes.append(record.busType);
        }

        if (!record.hasFile || !record.createdAt.isValid()) {
            continue;
        }
        if (!firstFileTime.isValid() || record.createdAt < firstFileTime) {
            firstFileTime = record.createdAt;
        }
        if (!lastFileTime.isValid() || record.createdAt > lastFileTime) {
            lastFileTime = record.createdAt;
        }
    }

    if (firstFileTime.isValid() && lastFileTime.isValid()) {
        const QString format = QString("yyyy-MM-dd HH:mm:ss");
        ui->timeRangeValue->setText(
                    QString("%1 - %2")
                    .arg(firstFileTime.toString(format), lastFileTime.toString(format)));
    } else {
        ui->timeRangeValue->setText(QString("—"));
    }
    ui->busTypesValue->setText(busTypes.join(QString(" + ")));
}

void wid_datasource::refreshHexPreview()
{
    const int pageRow = ui->dataSourceTable->currentRow();
    const int globalRow = m_sourcePage * DataSourceRowsPerPage + pageRow;
    if (pageRow >= 0 && globalRow >= 0 && globalRow < m_records.size()) {
        m_hexPage = 0;
    }
    populateHexTable();
}

void wid_datasource::handleDataSourceValueChanged(QTableWidgetItem *item)
{
    if (item == nullptr || item->column() != 4) {
        return;
    }

    const int pageRow = item->row();
    const int globalRow = m_sourcePage * DataSourceRowsPerPage + pageRow;
    if (globalRow < 0 || globalRow >= m_records.size()) {
        return;
    }

    DataSourceRecord &record = m_records[globalRow];
    if (!record.hasFile || record.previewData.isEmpty()
            || record.lengthState == LengthState::Parsing) {
        return;
    }

    bool parseOk = false;
    const QByteArray bytes = parseHexBytes(item->text(), &parseOk);
    const int editableBytes = qMin(16, record.previewData.size());

    // 输入必须是非空 HEX，且不能超过当前文件中实际可编辑的前 16 字节。
    // 校验失败时恢复原值，避免把非法文本或超长内容写入文件缓存。
    if (!parseOk || bytes.isEmpty() || bytes.size() > editableBytes) {
        const QString originalText =
                BinFormat::toHex(record.previewData.left(editableBytes));
        {
            QSignalBlocker blocker(ui->dataSourceTable);
            item->setText(originalText);
        }
        QMessageBox::warning(
                    this,
                    QString("值格式错误"),
                    QString("请输入 1-%1 个十六进制字节，例如：01 AF 7C。")
                    .arg(editableBytes));
        return;
    }

    // 只替换文件开头输入的字节，其余预览内容保持不变。
    record.previewData.replace(0, bytes.size(), bytes);
    record.valueDirty = true;

    // 将用户输入统一规范为两位大写 HEX，并阻止 setText() 递归触发 itemChanged。
    {
        QSignalBlocker blocker(ui->dataSourceTable);
        item->setText(BinFormat::toHex(bytes));
    }

    // 同步刷新下方数据段表格中已经改变的字节。
    populateHexTable();
}

void wid_datasource::saveDataSourceChanges()
{
    int dirtyRecordCount = 0;
    int savedFileCount = 0;
    QStringList errors;

    for (DataSourceRecord &record : m_records) {
        if (!record.valueDirty) {
            continue;
        }

        ++dirtyRecordCount;
        QFile file(record.filePath);
        if (!file.open(QIODevice::ReadWrite)) {
            errors.append(
                        QString("%1：%2").arg(record.name, file.errorString()));
            continue;
        }

        // “值”列只允许编辑文件前 16 字节；这里只原位覆盖该范围，
        // 不截断也不扩展原文件，超过 16 字节的其它内容保持不变。
        const qint64 bytesToWrite =
                qMin<qint64>(16, record.previewData.size());
        if (bytesToWrite <= 0) {
            errors.append(QString("%1：没有可保存的数据。").arg(record.name));
            continue;
        }
        if (file.size() < bytesToWrite || !file.seek(0)) {
            errors.append(
                        QString("%1：文件长度变化或定位失败。").arg(record.name));
            continue;
        }

        const qint64 written =
                file.write(record.previewData.constData(), bytesToWrite);
        if (written != bytesToWrite || !file.flush()) {
            errors.append(
                        QString("%1：写入失败，%2")
                        .arg(record.name, file.errorString()));
            continue;
        }

        record.valueDirty = false;
        ++savedFileCount;
    }

    if (dirtyRecordCount == 0) {
        QMessageBox::information(
                    this, QString("保存"), QString("当前没有需要保存的修改。"));
        return;
    }

    if (!errors.isEmpty()) {
        QMessageBox::warning(
                    this,
                    QString("保存未全部完成"),
                    QString("成功保存 %1 个文件。\n\n%2")
                    .arg(savedFileCount)
                    .arg(errors.join(QString("\n"))));
        return;
    }

    QMessageBox::information(
                this,
                QString("保存完成"),
                QString("已成功保存 %1 个文件。").arg(savedFileCount));
}

void wid_datasource::changeDisplayBase(int index)
{
    m_decimalMode = index == 1;
    populateHexTable();
}

QString wid_datasource::formatByte(quint8 value) const
{
    if (m_decimalMode) {
        return QString::number(value);
    }
    return QString("%1")
            .arg(value, 2, 16, QLatin1Char('0'))
            .toUpper();
}

void wid_datasource::populateHexTable()
{
    const int pageRow = ui->dataSourceTable->currentRow();
    const int selectedRecord = m_sourcePage * DataSourceRowsPerPage + pageRow;
    ui->dataSourceHexTable->clearContents();
    ui->dataSourceHexTable->setRowCount(0);
    if (pageRow < 0 || selectedRecord < 0 || selectedRecord >= m_records.size()) {
        updateHexPagination();
        return;
    }

    const QByteArray previewBytes = m_records.at(selectedRecord).previewData;
    const int totalRows = (previewBytes.size() + 15) / 16;
    const int pageCount = qMax(
                1, (totalRows + HexRowsPerPage - 1) / HexRowsPerPage);
    m_hexPage = qBound(0, m_hexPage, pageCount - 1);

    const int firstHexRow = m_hexPage * HexRowsPerPage;
    const int endHexRow = qMin(firstHexRow + HexRowsPerPage, totalRows);
    const int visibleRows = endHexRow - firstHexRow;
    ui->dataSourceHexTable->setRowCount(visibleRows);

    for (int row = 0; row < visibleRows; ++row) {
        const int globalHexRow = firstHexRow + row;
        // 第 0 列显示当前行的 8 位十六进制偏移。
        auto *offsetItem = new QTableWidgetItem(
                    QString("%1")
                    .arg(globalHexRow * 16, 8, 16, QLatin1Char('0'))
                    .toUpper());
        offsetItem->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
        offsetItem->setTextAlignment(Qt::AlignCenter);
        ui->dataSourceHexTable->setItem(row, 0, offsetItem);

        for (int column = 0; column < 16; ++column) {
            const int index = globalHexRow * 16 + column;
            auto *item = new QTableWidgetItem;
            item->setTextAlignment(Qt::AlignCenter);
            if (index < previewBytes.size()) {
                item->setText(
                            formatByte(static_cast<quint8>(previewBytes.at(index))));
                item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
            } else {
                item->setFlags(Qt::NoItemFlags);
            }
            ui->dataSourceHexTable->setItem(row, column + 1, item);
        }
    }

    updateHexPagination();
    QTimer::singleShot(0, this, &wid_datasource::updateHexColumnWidths);
}

void wid_datasource::updateHexPagination()
{
    const int pageRow = ui->dataSourceTable->currentRow();
    const int selectedRecord = m_sourcePage * DataSourceRowsPerPage + pageRow;
    if (pageRow < 0 || selectedRecord < 0 || selectedRecord >= m_records.size()) {
        ui->hexPageInfoLabel->setText(QString("—"));
        ui->hexPageLabel->setText(QString("— / —"));
        ui->hexJumpSpinBox->setRange(1, 1);
        ui->hexJumpSpinBox->setValue(1);
        ui->hexJumpSpinBox->setEnabled(false);
        ui->hexJumpButton->setEnabled(false);
        ui->hexPrevButton->setEnabled(false);
        ui->hexNextButton->setEnabled(false);
        return;
    }

    const int totalRows =
            (m_records.at(selectedRecord).previewData.size() + 15) / 16;
    const int pageCount = qMax(
                1, (totalRows + HexRowsPerPage - 1) / HexRowsPerPage);
    const int firstRow = m_hexPage * HexRowsPerPage;
    const int lastRow = qMin(firstRow + HexRowsPerPage, totalRows);

    ui->hexPageInfoLabel->setText(
                totalRows == 0
                ? QString("无数据")
                : QString("%1-%2 / %3 行")
                  .arg(firstRow + 1)
                  .arg(lastRow)
                  .arg(totalRows));
    ui->hexPageLabel->setText(
                QString("第 %1 / %2 页").arg(m_hexPage + 1).arg(pageCount));
    ui->hexJumpSpinBox->setRange(1, pageCount);
    ui->hexJumpSpinBox->setValue(m_hexPage + 1);
    ui->hexJumpSpinBox->setEnabled(totalRows > 0);
    ui->hexJumpButton->setEnabled(totalRows > 0);
    ui->hexPrevButton->setEnabled(totalRows > 0 && m_hexPage > 0);
    ui->hexNextButton->setEnabled(totalRows > 0 && m_hexPage + 1 < pageCount);
}

void wid_datasource::showPreviousSourcePage()
{
    if (m_sourcePage <= 0) {
        return;
    }
    --m_sourcePage;
    populateDataSourcePage();
}

void wid_datasource::showNextSourcePage()
{
    const int pageCount =
            (m_records.size() + DataSourceRowsPerPage - 1) / DataSourceRowsPerPage;
    if (m_sourcePage + 1 >= pageCount) {
        return;
    }
    ++m_sourcePage;
    populateDataSourcePage();
}

void wid_datasource::jumpToSourcePage()
{
    const int pageCount = m_records.isEmpty()
            ? 0
            : (m_records.size() + DataSourceRowsPerPage - 1)
              / DataSourceRowsPerPage;
    if (pageCount <= 0) {
        return;
    }

    // QSpinBox 显示的是从 1 开始的页码，内部 m_sourcePage 从 0 开始。
    const int targetPage = qBound(1, ui->sourceJumpSpinBox->value(), pageCount) - 1;
    if (targetPage == m_sourcePage) {
        // 页码没有变化时也把输入框恢复为当前真实页码，
        // 防止用户输入越界值后界面显示与内部状态不一致。
        ui->sourceJumpSpinBox->setValue(m_sourcePage + 1);
        return;
    }

    m_sourcePage = targetPage;
    populateDataSourcePage();
}

void wid_datasource::showPreviousHexPage()
{
    if (m_hexPage <= 0) {
        return;
    }
    --m_hexPage;
    populateHexTable();
}

void wid_datasource::showNextHexPage()
{
    const int pageRow = ui->dataSourceTable->currentRow();
    const int selectedRecord = m_sourcePage * DataSourceRowsPerPage + pageRow;
    if (selectedRecord < 0 || selectedRecord >= m_records.size()) {
        return;
    }

    const int totalRows =
            (m_records.at(selectedRecord).previewData.size() + 15) / 16;
    const int pageCount = qMax(
                1, (totalRows + HexRowsPerPage - 1) / HexRowsPerPage);
    if (m_hexPage + 1 >= pageCount) {
        return;
    }
    ++m_hexPage;
    populateHexTable();
}

void wid_datasource::jumpToHexPage()
{
    const int pageRow = ui->dataSourceTable->currentRow();
    const int selectedRecord = m_sourcePage * DataSourceRowsPerPage + pageRow;
    if (pageRow < 0 || selectedRecord < 0 || selectedRecord >= m_records.size()) {
        return;
    }

    const int totalRows =
            (m_records.at(selectedRecord).previewData.size() + 15) / 16;
    if (totalRows <= 0) {
        return;
    }

    const int pageCount = qMax(
                1, (totalRows + HexRowsPerPage - 1) / HexRowsPerPage);
    // QSpinBox 显示从 1 开始的页码，内部 m_hexPage 从 0 开始。
    const int targetPage = qBound(1, ui->hexJumpSpinBox->value(), pageCount) - 1;
    if (targetPage == m_hexPage) {
        ui->hexJumpSpinBox->setValue(m_hexPage + 1);
        return;
    }

    m_hexPage = targetPage;
    populateHexTable();
}

void wid_datasource::backRequested()
{
    ui->stackedWidget->setCurrentIndex(0);
}
