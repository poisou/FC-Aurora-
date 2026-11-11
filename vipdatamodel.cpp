#include "vipdatamodel.h"

// =============================================================================
// VipDataModel 实现
//
// 【完整调用链】传入 BIN 文件 → 提取全部包 → 分页显示 → 修改 → 保存
//
// ① 加载文件并提取全部包
//      loadFile(路径, 协议)
//        ├─ clear()                        清空上一次数据与未保存修改
//        ├─ 逐包循环（只读 6 字节包头，不读数据段）
//        │    ├─ BinFormat::decodeDataCount() 解析 n
//        │    │   （字节序由 binformat.h 的 BIN_HEADER_BIG_ENDIAN 统一控制）
//        │    ├─ 数据段长度：Aurora = n × 1024，Tcp（含 FC）= n
//        │    └─ 记录 offset / dataCount / payloadLength / header
//        └─ 至少解析出 1 个完整包才返回 true
//      要改解包算法 → 只改 loadFile() 的循环；其余逻辑不用动。
//
// ② 表格分页展示
//      pageCount() / rowsOnPage(page) / packetIndexAt(page, row)
//        └─ cellText(page, row, column)    统一的“列 → 文本”映射点
//             ├─ 列 0/1/2/3 直接取 BinPacket 元数据
//             └─ 列 4（预览）按需读数据段前 12 字节，并叠加未保存修改
//
// ③ 修改数据
//      setByte(文件绝对偏移, 新值)
//        ├─ 只写入内存覆盖表 m_modifiedBytes，不落盘
//        └─ emit dataChanged()             界面据此刷新单元格
//      byteValue(偏移) / packetPayload(包序号) 读取时会叠加覆盖表，
//      因此界面显示与 VipRecvPro 收发逻辑读到的都是修改后的最新内容。
//
// ④ 保存
//      save()
//        ├─ 以读写方式打开文件
//        ├─ 逐个修改点 seek + 写 1 字节（原位覆盖，不截断不扩展）
//        ├─ flush + close
//        ├─ 清空覆盖表，重新读取文件大小
//        └─ emit dataChanged() / saved()    表格与 VipRecvPro 同步刷新
//
// =============================================================================
// 设计约束：
//   - 只保存包头元数据，不把整个文件载入内存（1 GB 文件安全）；
//   - 修改包头（例如 n）会改变包结构，保存后需重新 loadFile()。
// =============================================================================

#include <QFile>
#include <QFileInfo>

namespace {

// 数据预览列最多显示的字节数。
constexpr int kPreviewBytes = 12;

// 空包元数据：packetAt() 越界时返回它的引用，避免悬空指针。
const BinPacket kEmptyPacket;

} // namespace

// 构造函数：只初始化 QObject 父对象，所有状态使用成员默认值
// （无文件、无包、页大小 = BinFormat::PacketsPerPage、协议 = Tcp）。
VipDataModel::VipDataModel(QObject *parent)
    : QObject(parent)
{
}

// 加载 BIN 文件并提取全部数据包。
// 步骤编号与文件头注释 ① 对应：
//   1) clear() 清空旧数据与未保存修改，避免新旧混合；
//   2) 打开文件失败直接返回 false（不改动任何已有状态之外的内容）；
//   3) 逐包扫描，只读包头不读数据段；
//   4) 文件末尾不足一包时停止（不视为错误）；
//   5) 返回是否至少解析出一个完整包。
bool VipDataModel::loadFile(const QString &filePath, Protocol protocol)
{
    // 1) 清空上一次的内容，避免新旧数据混在一起。
    clear();
    m_protocol = protocol;

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        return false;
    }

    m_filePath = filePath;
    m_fileSize = file.size();

    // 2) 从文件开头逐包扫描，只记录元数据，不读取数据段。
    qint64 offset = 0;
    int index = 0;
    while (offset + BinFormat::HeaderSize <= m_fileSize) {
        if (!file.seek(offset)) {
            break;
        }

        const QByteArray header = file.read(BinFormat::HeaderSize);
        if (header.size() != BinFormat::HeaderSize) {
            break;
        }

        BinPacket packet;
        packet.index = index;
        packet.offset = offset;
        packet.header = header;
        packet.dataCount = BinFormat::decodeDataCount(header);

        // 3) 数据段长度：Aurora = n × 1024，Tcp（含 FC）= n。
        if (m_protocol == Protocol::Aurora) {
            packet.payloadLength =
                static_cast<qint64>(packet.dataCount) * BinFormat::PayloadSize;
        } else {
            packet.payloadLength = static_cast<qint64>(packet.dataCount);
        }

        // 4) 剩余数据不足一整包时说明文件末尾不完整，停止扫描。
        if (offset + packet.packetSize() > m_fileSize) {
            break;
        }

        m_packets.append(packet);

        // 5) 前移到下一包。
        offset += packet.packetSize();
        ++index;
    }

    return !m_packets.isEmpty();
}

// 清空模型：文件路径、包列表、文件大小和未保存修改全部复位，
// 并发出 dataChanged() 通知界面清空表格（不会删除磁盘文件）。
void VipDataModel::clear()
{
    m_filePath.clear();
    m_packets.clear();
    m_fileSize = 0;
    m_modifiedBytes.clear();
    emit dataChanged();
}

// 是否已成功加载文件（以包列表非空为准）。
bool VipDataModel::isLoaded() const
{
    return !m_packets.isEmpty();
}

// 当前文件路径；未加载时为空字符串。
QString VipDataModel::filePath() const
{
    return m_filePath;
}

// 文件总字节数（加载时记录，保存后重新读取）。
qint64 VipDataModel::fileSize() const
{
    return m_fileSize;
}

// 当前解包协议（决定 payloadLength 的换算方式）。
VipDataModel::Protocol VipDataModel::protocol() const
{
    return m_protocol;
}

// 已解析出的数据包数量。
int VipDataModel::packetCount() const
{
    return m_packets.size();
}

// 取指定包的元数据；越界时返回静态空对象，调用方无需判空。
// @param index 全局包序号，从 0 开始。
const BinPacket &VipDataModel::packetAt(int index) const
{
    if (index < 0 || index >= m_packets.size()) {
        return kEmptyPacket;
    }
    return m_packets.at(index);
}

// 读取指定包的完整数据段（不含 6 字节包头）。
// 步骤：
//   1) 校验包序号；
//   2) 按 offset + HeaderSize 定位，读 payloadSize() 个字节；
//   3) 叠加内存中尚未保存的修改，供 VipRecvPro 发送使用。
QByteArray VipDataModel::packetPayload(int index) const
{
    if (index < 0 || index >= m_packets.size()) {
        return QByteArray();
    }

    const BinPacket &packet = m_packets.at(index);
    const qint64 payloadOffset = packet.offset + BinFormat::HeaderSize;
    QByteArray payload =
        readFileBytes(payloadOffset, packet.payloadSize());

    // 叠加尚未保存的修改，保证 VipRecvPro 读到的是最新内容。
    applyModifiedBytes(payloadOffset, &payload);
    return payload;
}

// 设置每页包数；传 <= 0 时恢复默认值 BinFormat::PacketsPerPage。
// 只影响分页计算，不改动已解析的包列表。
void VipDataModel::setPageSize(int pageSize)
{
    m_pageSize =
        pageSize > 0 ? pageSize : BinFormat::PacketsPerPage;
}

// 当前每页包数。
int VipDataModel::pageSize() const
{
    return m_pageSize;
}

// 总页数：向上取整；未加载或页大小非法时为 0。
int VipDataModel::pageCount() const
{
    if (m_packets.isEmpty() || m_pageSize <= 0) {
        return 0;
    }
    return (m_packets.size() + m_pageSize - 1) / m_pageSize;
}

// 指定页的行数；最后一页可能不足一页，越界页返回 0。
int VipDataModel::rowsOnPage(int page) const
{
    if (page < 0 || page >= pageCount()) {
        return 0;
    }

    const int first = page * m_pageSize;
    return qMin(m_pageSize, m_packets.size() - first);
}

// 页内行号 → 全局包索引；越界返回 -1，便于表格直接判断合法性。
int VipDataModel::packetIndexAt(int page, int row) const
{
    if (page < 0 || row < 0 || row >= rowsOnPage(page)) {
        return -1;
    }
    return page * m_pageSize + row;
}

// 默认列名，顺序与 cellText() 的 column 参数一一对应。
QStringList VipDataModel::columnTitles() const
{
    return QStringList{
        QStringLiteral("包序号"),
        QStringLiteral("文件偏移"),
        QStringLiteral("数据块 n"),
        QStringLiteral("数据长度"),
        QStringLiteral("数据预览")
    };
}

// 计算单元格文本（文件头注释 ② 的统一映射点）。
// 处理顺序：
//   1) 页/行 → 全局包索引，越界直接返回空字符串；
//   2) 按列号取对应字段（序号/偏移/n/长度/预览）；
//   3) 预览列按需读数据段前 12 字节，并叠加未保存修改，
//      保证“改完立即显示新值”，无需先保存。
QString VipDataModel::cellText(int page, int row, int column) const
{
    const int index = packetIndexAt(page, row);
    if (index < 0) {
        return QString();
    }

    // 列 → 显示内容的统一映射（改格式只改这里）。
    //   0=包序号：从 1 开始，补足两位；
    //   1=文件偏移：8 位大写十六进制；
    //   2=数据块 n：包头解析出的 n；
    //   3=数据长度：数据段字节数 + “ B”；
    //   4=数据预览：数据段前 12 字节 HEX，超出部分用 “ ...” 表示。
    const BinPacket &packet = m_packets.at(index);
    switch (column) {
    case 0:
        return QStringLiteral("%1")
            .arg(packet.index + 1, 2, 10, QLatin1Char('0'));
    case 1:
        return QStringLiteral("0x%1")
            .arg(packet.offset, 8, 16, QLatin1Char('0'))
            .toUpper();
    case 2:
        return QString::number(packet.dataCount);
    case 3:
        return QStringLiteral("%1 B").arg(packet.payloadSize());
    case 4: {
        const qint64 payloadOffset = packet.offset + BinFormat::HeaderSize;
        const qint64 length =
            qMin<qint64>(kPreviewBytes, packet.payloadSize());
        QByteArray preview = readFileBytes(payloadOffset, length);
        applyModifiedBytes(payloadOffset, &preview);

        QString text = BinFormat::toHex(preview);
        if (packet.payloadSize() > kPreviewBytes) {
            text.append(QStringLiteral(" ..."));
        } else if (text.isEmpty()) {
            text = QStringLiteral("—");
        }
        return text;
    }
    default:
        return QString();
    }
}

// 修改单个字节（文件头注释 ③）。
//   1) 偏移越界或未加载 → 返回 false；
//   2) 只写入内存覆盖表，不落盘；
//   3) 发 dataChanged()，界面据此刷新对应单元格；
// 落盘统一由 save() 完成，可多次修改后一次性写入。
bool VipDataModel::setByte(qint64 fileOffset, quint8 value)
{
    if (fileOffset < 0 || fileOffset >= m_fileSize) {
        return false;
    }

    // 先更新内存模型，界面据此立即刷新；落盘交给 save()。
    m_modifiedBytes.insert(fileOffset, value);
    emit dataChanged();
    return true;
}

// 读取某个字节的当前值：先查内存覆盖表，未命中再从文件读取。
// @param fileOffset 文件绝对偏移；越界或读取失败返回 0。
quint8 VipDataModel::byteValue(qint64 fileOffset) const
{
    const auto modified = m_modifiedBytes.constFind(fileOffset);
    if (modified != m_modifiedBytes.constEnd()) {
        return modified.value();
    }

    const QByteArray data = readFileBytes(fileOffset, 1);
    return data.isEmpty() ? 0 : static_cast<quint8>(data.at(0));
}

// 尚未写回文件的修改字节数。
int VipDataModel::modifiedByteCount() const
{
    return m_modifiedBytes.size();
}

// 是否存在未保存修改（界面可据此提示“有改动未保存”）。
bool VipDataModel::hasUnsavedChanges() const
{
    return !m_modifiedBytes.isEmpty();
}

// 保存：把内存覆盖表原位写回文件（文件头注释 ④）。
// 步骤：
//   1) 无修改直接返回 true，避免无谓打开文件；
//   2) 以读写方式打开文件，失败返回 false（修改仍保留在覆盖表）；
//   3) 逐个修改点 seek + 写 1 字节；任一步失败立即关闭并返回 false；
//   4) flush 确保真正落盘后关闭；
//   5) 清空覆盖表，重新读取文件大小；
//   6) 发 dataChanged() + saved()，界面与 VipRecvPro 同步刷新。
bool VipDataModel::save()
{
    if (m_modifiedBytes.isEmpty()) {
        return true;
    }

    QFile file(m_filePath);
    if (!file.open(QIODevice::ReadWrite)) {
        return false;
    }

    // 逐字节原位写回：不截断、不扩展文件。
    for (auto it = m_modifiedBytes.constBegin();
         it != m_modifiedBytes.constEnd();
         ++it) {
        if (!file.seek(it.key())) {
            file.close();
            return false;
        }
        const char byte = static_cast<char>(it.value());
        if (file.write(&byte, 1) != 1) {
            file.close();
            return false;
        }
    }

    if (!file.flush()) {
        file.close();
        return false;
    }
    file.close();

    // 写盘成功后清空覆盖表，并同步最新的文件大小。
    m_modifiedBytes.clear();
    m_fileSize = QFileInfo(m_filePath).size();
    emit dataChanged();
    emit saved();
    return true;
}

// 放弃未保存修改：清空覆盖表并发 dataChanged()，
// 之后读取到的字节重新以磁盘文件内容为准。
void VipDataModel::discardChanges()
{
    if (m_modifiedBytes.isEmpty()) {
        return;
    }
    m_modifiedBytes.clear();
    emit dataChanged();
}

// 按需读取文件的一段内容（每次调用打开一次文件）。
// 只用于“预览/单字节/按包读取”等小数据量场景，
// 不会把整个文件读入内存。
// @param fileOffset 文件绝对偏移。
// @param length 读取字节数，<= 0 返回空数组。
// @return 读到的内容；打开或定位失败返回空数组。
QByteArray VipDataModel::readFileBytes(qint64 fileOffset,
                                       qint64 length) const
{
    if (m_filePath.isEmpty() || length <= 0) {
        return QByteArray();
    }

    QFile file(m_filePath);
    if (!file.open(QIODevice::ReadOnly) || !file.seek(fileOffset)) {
        return QByteArray();
    }
    return file.read(length);
}

// 把内存覆盖表中的修改叠加到一段已读取的数据上。
// 用于“修改后不保存也能立即看到新值”：
//   - 预览列 cellText() 第 4 列
//   - packetPayload() 供 VipRecvPro 发送的数据
// 参数：
//   fileOffset 该段数据在文件中的绝对起始偏移；
//   data       需要就地修改的数据（可与覆盖表无交集）。
// 只有落在 [fileOffset, fileOffset + data->size()) 内的修改点会被应用。
void VipDataModel::applyModifiedBytes(qint64 fileOffset,
                                      QByteArray *data) const
{
    if (data == nullptr || data->isEmpty() || m_modifiedBytes.isEmpty()) {
        return;
    }

    const qint64 endOffset = fileOffset + data->size();
    for (auto it = m_modifiedBytes.constBegin();
         it != m_modifiedBytes.constEnd();
         ++it) {
        if (it.key() < fileOffset || it.key() >= endOffset) {
            continue;
        }
        (*data)[static_cast<int>(it.key() - fileOffset)] =
            static_cast<char>(it.value());
    }
}
