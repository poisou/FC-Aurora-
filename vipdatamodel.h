#pragma once

#include "binformat.h"

#include <QByteArray>
#include <QHash>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QVector>

// VIP 数据浏览表数据模型。
//
// 对应页面：VIP 数据回看页（表格分页展示 BIN 文件内容）。
// 本类作为 VIP 数据浏览表格的唯一数据源，职责如下：
//   1. 传入一个 BIN 文件后，按总线协议扫描并提取全部数据包；
//   2. 为表格提供分页信息、列名和单元格文本；
//   3. 接收界面改动的字节，先保存在内存覆盖表中；
//   4. 保存时把修改原位写回文件，写成功后清空覆盖表；
//   5. 为 VipRecvPro 的发送/接收逻辑提供按包读取数据段的接口。
//
// 设计要点：
//   - 只保存包头元数据（BinPacket），不把整个文件读入内存，
//     因此 1 GB 级文件也不会占用大量内存；
//   - 数据段按需读取，界面预览只读需要的那一小段；
//   - 所有“包序号/偏移/n/长度/预览”的显示规则集中在 cellText()，
//     表格填充与分页逻辑不需要了解数据格式。
class VipDataModel : public QObject
{
    Q_OBJECT

public:
    // 解包协议：
    //   Aurora：6 字节包头 + n × 1024 字节数据段；
    //   Tcp   ：6 字节包头 + n 字节数据段（FC 与以太网均使用本协议）。
    enum class Protocol {
        Aurora,
        Tcp
    };

    // @param parent Qt 父对象；本类只在 GUI 线程使用。
    explicit VipDataModel(QObject *parent = nullptr);

    // ── 加载与状态 ────────────────────────────────────────────────
    // 传入 BIN 文件并提取全部数据包。
    //
    // 执行流程：
    //   1. 清空上一次的数据包与未保存修改；
    //   2. 打开文件，记录文件总字节数；
    //   3. 从偏移 0 开始逐包扫描：
    //        a) 读 6 字节包头；
    //        b) BinFormat::decodeDataCount() 解析 n
    //           （字节序由 binformat.h 的 BIN_HEADER_BIG_ENDIAN 统一控制）；
    //        c) 按协议计算数据段长度（Aurora = n × 1024，Tcp = n）；
    //        d) 只记录 offset / dataCount / payloadLength / header，
    //           不读取数据段内容；
    //        e) 偏移前移到下一包，直到文件末尾或剩余数据不足一包。
    //   4. 至少解析出 1 个完整包时返回 true。
    //
    // 注意：本函数是同步扫描，大文件会阻塞调用线程；
    //       界面调用时建议放到后台线程执行，完成后再回到主线程刷新表格。
    //
    // @param filePath BIN 文件绝对路径。
    // @param protocol 解包协议：Aurora / Tcp（FC、以太网都用 Tcp）。
    // @return 至少解析出 1 个完整包时返回 true。
    bool loadFile(const QString &filePath, Protocol protocol);

    // 清空已加载的数据包和未保存修改（不会删除或改动磁盘文件）。
    void clear();

    // 是否已成功加载文件。
    bool isLoaded() const;

    // 当前文件绝对路径；未加载时为空字符串。
    QString filePath() const;

    // 文件总字节数（磁盘上的真实大小）。
    qint64 fileSize() const;

    // 当前解包协议。
    Protocol protocol() const;

    // ── 数据包访问 ────────────────────────────────────────────────
    // 已解析出的数据包数量。
    int packetCount() const;

    // 取指定全局包序号的元数据。
    // @param index 全局包序号，从 0 开始。
    // @return 包元数据；索引越界时返回一个空的静态对象。
    const BinPacket &packetAt(int index) const;

    // 读取指定包的完整数据段（不含 6 字节包头）。
    // 会叠加内存中尚未保存的字节修改，供 VipRecvPro 发送使用。
    // @param index 全局包序号。
    // @return 数据段内容；索引越界或读取失败时返回空数组。
    QByteArray packetPayload(int index) const;

    // ── 分页 ─────────────────────────────────────────────────────
    // 设置每页显示的数据包数量。
    // @param pageSize 每页包数，<= 0 时恢复默认值。
    void setPageSize(int pageSize);

    // 每页显示的数据包数量（默认 BinFormat::PacketsPerPage）。
    int pageSize() const;

    // 总页数；未加载文件时为 0。
    int pageCount() const;

    // 指定页实际显示的行数（最后一页可能不足 pageSize）。
    // @param page 页索引，从 0 开始。
    int rowsOnPage(int page) const;

    // 指定页第 row 行对应的全局包索引。
    // @param page 页索引，从 0 开始。
    // @param row 页内行号，从 0 开始。
    // @return 全局包索引；越界返回 -1。
    int packetIndexAt(int page, int row) const;

    // ── 表格列 ───────────────────────────────────────────────────
    // 默认列名，顺序与 cellText() 的 column 参数一致：
    //   0=包序号，1=文件偏移，2=数据块 n，3=数据长度，4=数据预览。
    QStringList columnTitles() const;

    // 计算表格“第 page 页、第 row 行、第 column 列”的显示文本。
    // 所有“列 → 内容”的映射都集中在这里，后期改显示格式只改本函数。
    // @param page 页索引，从 0 开始。
    // @param row 页内行号，从 0 开始。
    // @param column 列索引，从 0 开始。
    // @return 单元格文本；参数越界时返回空字符串。
    QString cellText(int page, int row, int column) const;

    // ── 数据修改（先改模型，再写文件）────────────────────────────
    // 修改文件中的某一个字节。
    //
    // 只更新内存覆盖表并发出 dataChanged()，不会立即写盘；
    // 调用方在界面上刷新对应单元格即可看到新值。
    // 真正落盘由 save() 统一完成。
    //
    // @param fileOffset 文件绝对偏移（0 表示文件第 1 个字节）。
    // @param value 新的字节值（0~255）。
    // @return 偏移在文件范围内返回 true；越界或未加载返回 false。
    bool setByte(qint64 fileOffset, quint8 value);

    // 读取某个字节的“当前值”：优先返回覆盖表中的新值，
    // 否则从文件读取。
    // @param fileOffset 文件绝对偏移。
    // @return 字节值；越界或读取失败返回 0。
    quint8 byteValue(qint64 fileOffset) const;

    // 尚未保存的修改字节数量。
    int modifiedByteCount() const;

    // 是否存在尚未写回文件的修改。
    bool hasUnsavedChanges() const;

    // 把覆盖表中的所有修改原位写回文件。
    //
    // 流程：
    //   1. 以读写方式打开文件；
    //   2. 逐个修改点 seek 到文件绝对偏移并写入 1 个字节；
    //   3. flush 后关闭，确保数据真正落盘；
    //   4. 清空覆盖表，重新读取文件大小；
    //   5. 发出 saved() 通知界面刷新。
    //
    // 说明：只做原位覆盖，不截断、不扩展文件。
    //       如果修改了包头（例如最后两个字节 n），包结构与包数量会变化，
    //       保存后需要重新调用 loadFile() 才能刷新包列表。
    //
    // @return 全部修改写入成功时返回 true。
    bool save();

    // 放弃所有未保存的修改（重新从文件读取）。
    void discardChanges();

signals:
    // 内存覆盖表发生变化（setByte() 或 discardChanges() 之后发出）。
    // 界面可据此刷新当前页中受影响的单元格。
    void dataChanged();

    // 修改已成功写回文件。
    void saved();

private:
    // 按需要读取文件的一段内容。
    // @param fileOffset 文件绝对偏移。
    // @param length 读取字节数。
    // @return 读到的内容；失败时返回空数组。
    QByteArray readFileBytes(qint64 fileOffset, qint64 length) const;

    // 把内存覆盖表应用到一段数据上（用于预览显示与数据段读取）。
    // @param fileOffset 该段数据在文件中的绝对起始偏移。
    // @param data 需要就地修改的数据。
    void applyModifiedBytes(qint64 fileOffset, QByteArray *data) const;

    QString m_filePath;                    // 当前文件路径。
    QVector<BinPacket> m_packets;          // 解包得到的包元数据。
    qint64 m_fileSize = 0;                 // 文件总字节数。
    int m_pageSize = BinFormat::PacketsPerPage; // 每页包数。
    Protocol m_protocol = Protocol::Tcp;   // 当前解包协议。
    // 未保存的字节修改：键为文件绝对偏移，值为新字节。
    QHash<qint64, quint8> m_modifiedBytes;
};
