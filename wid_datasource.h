#pragma once

#include <QByteArray>
#include <QDateTime>
#include <QHash>
#include <QPointer>
#include <QSet>
#include <QSqlDatabase>
#include <QString>
#include <QVector>
#include <QWidget>

namespace Ui {
class wid_datasource;
}

class BinFileImporterBase;
class DataSourceFileCopyWorker;
class QResizeEvent;
class QTableWidgetItem;
class QThread;

// 数据源管理页面。
//
// 对应界面文件：wid_datasource.ui
// 负责展示已选择的数据源文件、基础统计信息，并在下方按 16/10 进制
// 预览选中数据源的前 64 KB 字节。下方偏移列表中的字节单元格可直接编辑，
// 修改先写入内存缓存，点击“保存修改”后统一写回对应的 BIN 文件。
// 该页面作为 QWidget 嵌入 wid_stack。
// 数据源列表每页显示 10 条记录，下方数据段表格每页显示 50 行。
class wid_datasource : public QWidget
{
    Q_OBJECT

public:
    // 构造数据源管理页面。
    // @param parent Qt 父控件。
    explicit wid_datasource(QWidget *parent = nullptr);

    // 销毁 Designer UI。
    ~wid_datasource() override;

    // 将自定义页面生成的 BIN 文件加入数据源列表和数据库。
    // 同一路径的旧数据库记录会先删除，避免重复保存产生重复行。
    // @param name 数据源名称。
    // @param busType 总线类型：FC、Aurora 或 以太网。
    // @param filePath MYDATA 中 BIN 文件的绝对路径。
    // @param createdAt 数据源创建时间。
    // @return 数据库写入和列表刷新成功时返回 true。
    bool addGeneratedDataSource(const QString &name,
                                const QString &busType,
                                const QString &filePath,
                                const QDateTime &createdAt);

signals:
    // 用户点击“自定义数据”后请求打开自定义数据源编辑页面。
    // 页面切换由 wid_stack 统一处理。
    void customDataSourceRequested();

protected:
    // 页面尺寸变化后重新计算字节列宽度。
    // @param event Qt 尺寸变化事件。
    void resizeEvent(QResizeEvent *event) override;

private slots:
    // 选择一个或多个 BIN 文件并加入数据源列表。
    void chooseFiles();

    // 请求打开自定义数据源编辑页面。
    void openCustomDataSource();

    // 删除数据源表中整行高亮的记录。
    // 删除成功后刷新当前页，但不会自动选中其它记录；
    // 下方数据段列表会同时清空，等待用户重新选择数据源行。
    void deleteSelectedData();

    // 刷新当前选中数据源的十六进制表格。
    void refreshHexPreview();

    // 处理数据浏览列表“值”列的手工修改。
    // 输入内容必须是空格分隔或连续的十六进制字节，最多修改文件前 16 字节。
    // @param item 发生内容变化的表格单元格。
    void handleDataSourceValueChanged(QTableWidgetItem *item);

    // 处理下方“偏移”列表（数据段表格）中单个字节单元格的修改。
    // 单元格列号 1~16 对应本行 16 个字节，第 0 列为偏移列（只读）。
    // 修改只写入内存中的 previewData，并标记该记录待保存；
    // 真正的文件写入在点击“保存修改”后由 saveDataSourceChanges() 完成。
    // 输入非法时恢复原值并提示，不会污染缓存。
    // @param item 发生内容变化的表格单元格。
    void handleHexItemChanged(QTableWidgetItem *item);

    // 将所有已修改的数据源内容写回各自 BIN 文件的原位置。
    // 修改来源包括“值”列（前 16 字节）和下方偏移列表（前 64 KB 内任意字节），
    // 保存时按 previewData 逐字节原位覆盖，不截断也不扩展文件。
    void saveDataSourceChanges();

    // 切换十六进制或十进制显示。
    // @param index 0 表示 16 进制，1 表示 10 进制。
    void changeDisplayBase(int index);

    // 数据源列表切换到上一页。
    // 每页固定显示 10 条记录；已在第一页时忽略本次操作。
    void showPreviousSourcePage();

    // 数据源列表切换到下一页。
    // 每页固定显示 10 条记录；已在最后一页时忽略本次操作。
    void showNextSourcePage();

    // 跳转到数据源列表的指定页。
    // 页码来自 sourceJumpSpinBox，取值范围为 1 到当前总页数。
    void jumpToSourcePage();

    // 数据段表格切换到上一页。
    // 每页固定显示 50 行，每行包含 16 个字节。
    void showPreviousHexPage();

    // 数据段表格切换到下一页。
    // 每页固定显示 50 行，每行包含 16 个字节。
    void showNextHexPage();

    // 跳转到数据段的指定页。
    // 页码来自 hexJumpSpinBox，取值范围为 1 到当前总页数。
    void jumpToHexPage();

private:
    // 数据长度列的通用状态。FC、Aurora、TCP 共用，不再把 Aurora
    // 的解析过程硬编码为某个布尔字段。
    enum class LengthState {
        Ready,   // 长度已计算完成，可直接显示。
        Parsing, // 后台正在计算 Aurora 累计数据长度。
        Failed   // 长度计算失败。
    };

    // 数据源列表中的一条记录。
    struct DataSourceRecord {
        quint64 id = 0;              // 记录唯一编号，用于异步解析完成后更新对应行。
        QString name;                // 用户设置的数据源名称。
        QString busType;             // 总线类型：FC、Aurora 或 以太网。
        QDateTime createdAt;         // 文件系统中该文件的创建时间。
        qint64 dataLength = 0;       // 文件总字节数，或 Aurora 所有数据段的累计字节数。
        QString filePath;            // 上传文件的绝对路径。
        QByteArray previewData;      // 文件开头最多 64 KB 的预览内容，偏移列表可编辑此范围。
        bool hasFile = false;        // 是否为上传文件记录；自定义记录不参与时间范围。
        bool valueDirty = false;      // 内容（值列或偏移列表）已修改，尚未写回文件。
        LengthState lengthState = LengthState::Ready; // 数据长度显示状态。
        QString lengthError;         // 长度计算失败原因；为空表示成功。
    };

    // 向表格追加一条记录。
    // @param record 待追加的数据源记录。
    void appendRecord(const DataSourceRecord &record);

    // 初始化可执行文件目录下的 SQLite 数据库和三张数据源表。
    // 数据库路径：<可执行文件目录>/qsqlite/WT_FCData.db。
    // @return 数据库和所有数据表均准备成功时返回 true。
    bool initializeDatabase();

    // 创建指定数据源表。字段固定为 name、date、filePath。
    // @param tableName 已校验的表名。
    // @return 表已存在或创建成功时返回 true。
    bool createDataSourceTable(const QString &tableName);

    // 从三张数据源表加载已有记录到界面列表。
    void loadPersistedData();

    // 根据总线类型返回对应的 SQLite 表名。
    // @param busType FC、Aurora 或 以太网。
    // @return table_data_FCsource、table_data_Aurorasource 或
    //         table_data_TCPsource；未知类型返回空字符串。
    QString tableNameForBusType(const QString &busType) const;

    // 生成 MYDATA 目录中的唯一目标文件路径。
    // @param sourceFileName 原文件名，例如 capture.bin。
    // @return 不覆盖已有文件的绝对路径。
    QString uniqueDataFilePath(const QString &sourceFileName) const;

    // 启动后台文件复制任务。
    // @param record 已完成元数据填充、尚待复制的数据源记录。
    // @param sourcePath 用户选择的原始文件绝对路径。
    void startDataSourceFileCopy(const DataSourceRecord &record,
                                 const QString &sourcePath);

    // 向对应数据源表插入 name、date、filePath 三个字段。
    // @param record 已复制到 MYDATA 的数据源记录。
    // @return 数据库插入成功时返回 true。
    bool insertDataSourceRecord(const DataSourceRecord &record);

    // 从对应数据源表中删除指定记录。
    // @param record 需要删除的数据源记录。
    // @return 数据库删除成功时返回 true；非文件记录直接返回 true。
    bool deleteDataSourceRecord(const DataSourceRecord &record);

    // 删除 MYDATA 目录中对应的数据文件。
    // 删除前会校验目标路径确实位于 MYDATA 内，防止误删其它文件。
    // @param record 需要删除物理文件的数据源记录。
    // @return 文件不存在或删除成功时返回 true。
    bool deleteMyDataFile(const DataSourceRecord &record);

    // 根据总线类型启动统一的数据长度计算流程。
    // FC、TCP/以太网：直接使用文件总字节数，结果立即可用；
    // Aurora：进入后台解析，累计每个包的 n × 1024 数据段长度。
    // @param recordId 对应的数据源记录编号。
    // @param busType 记录的总线类型。
    // @param filePath 数据文件绝对路径。
    // @param fileSize FC/TCP 可直接使用的文件总字节数。
    void startDataLengthParse(quint64 recordId,
                              const QString &busType,
                              const QString &filePath,
                              qint64 fileSize);

    // Aurora 专用后台长度计算实现。
    // 从统一入口 startDataLengthParse() 调用，不直接处理其它总线类型。
    // @param recordId 对应的数据源记录编号。
    // @param filePath Aurora 文件绝对路径。
    void startAuroraLengthParse(quint64 recordId, const QString &filePath);

    // 将任意总线类型的数据长度计算结果写回对应记录。
    // 成功时状态改为 Ready，失败时改为 Failed。
    // @param recordId 对应的数据源记录编号。
    // @param dataLength FC/TCP 文件总字节数或 Aurora 累计数据段长度。
    // @param errorMessage 计算失败原因；为空表示成功。
    // @return 找到并更新了对应记录时返回 true；记录已被删除时返回 false。
    bool applyDataLengthResult(quint64 recordId,
                               qint64 dataLength,
                               const QString &errorMessage = QString());

    // 如果指定记录位于当前页，只刷新该记录的“数据长度”单元格。
    // @param recordId 需要刷新显示的记录编号。
    void refreshDataLengthCell(quint64 recordId);

    // 生成数据长度列显示文本。
    // @param record 待显示的数据源记录。
    // @return Ready 返回长度数值，Parsing 返回“解析中…”，
    //         Failed 返回“解析失败”。
    QString dataLengthText(const DataSourceRecord &record) const;

    // 请求取消指定记录的通用长度计算任务。
    // FC/TCP 通常没有后台任务，调用时直接安全返回。
    // @param recordId 对应的数据源记录编号。
    void cancelDataLengthParse(quint64 recordId);

    // 根据当前页码刷新数据源列表。
    // 数据源列表每页最多显示 10 条记录。
    // @param selectFirstRow true 表示刷新后默认选中当前页第一行，
    //        并同步刷新下方数据段；false 表示不选中任何行，
    //        下方数据段保持为空。删除记录时使用 false。
    void populateDataSourcePage(bool selectFirstRow = true);

    // 同步数据源列表的分页文本和按钮可用状态。
    void updateSourcePagination();

    // 重新计算总条数和总线类型。
    // 时间范围取数据浏览列表中所有上传文件创建时间的最小值和最大值，
    // 不含“自定义数据”记录。
    void updateSummary();

    // 按当前进制刷新十六进制表格。
    // 仅创建当前页最多 50 行的表格项，避免一次为整个预览数据创建控件。
    void populateHexTable();

    // 刷新上方数据源列表中指定记录的“值”列显示（previewData 前 16 字节 HEX）。
    // 偏移列表修改到前 16 字节时调用，保证两处显示同步。
    // @param globalRow 记录在 m_records 中的全局索引，不是页内行号。
    void refreshDataSourceValueCell(int globalRow);

    // 同步数据段表格的分页文本和按钮可用状态。
    void updateHexPagination();

    // 根据表格可视宽度重新分配 16 个字节列。
    void updateHexColumnWidths();

    // 格式化单个字节。
    // @param value 字节值。
    // @return 当前进制下的文本。
    QString formatByte(quint8 value) const;

    Ui::wid_datasource *ui;
    QVector<DataSourceRecord> m_records;
    bool m_decimalMode = false;
    int m_sourcePage = 0;
    int m_hexPage = 0;
    quint64 m_nextRecordId = 1;
    QHash<quint64, QThread *> m_lengthParseThreads;
    // 使用 QPointer 跟踪工作线程中的解析器；当线程释放解析器后，
    // 指针会自动变为空，避免窗口关闭时访问已销毁对象。
    QHash<quint64, QPointer<BinFileImporterBase>> m_lengthParsers;
    QHash<QThread *, QPointer<DataSourceFileCopyWorker>> m_copyWorkers;
    QSet<QString> m_reservedDataPaths;
    QString m_dataDirectoryPath;
    QString m_databasePath;
    QString m_databaseConnectionName;
    QSqlDatabase m_database;
};
