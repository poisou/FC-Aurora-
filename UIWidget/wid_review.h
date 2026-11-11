#pragma once

// =============================================================================
// 数据回看页面
//
// 对应界面文件：wid_review.ui
// 负责协调以下模块：
//   1. 文件选择、拖放和命令行导入；
//   2. 后台线程流式扫描 BIN 包头；
//   3. 数据包分页、页码跳转和数据摘要；
//   4. 按需加载当前包数据段；
//   5. 16/10 进制编辑以及修改原位写回 BIN 文件。
// =============================================================================

#include "mytools/binformat.h"

#include <QWidget>
#include <QString>
#include <QVector>

class QDragEnterEvent;
class QDragLeaveEvent;
class QDropEvent;
class QLabel;
class QProgressBar;
class QResizeEvent;
class QTableWidgetItem;
class QThread;

class BinFileImporterBase;

namespace Ui {
class WidReview;
}

// 数据回看页面类。
//
// 主线程只处理 UI 和轻量元数据；Aurora 与 TCP 扫描分别由独立导入器在后台线程完成。
// 当前数据段最多加载 64 KB，避免单包过大时阻塞界面或占用过多内存。
class WidReview : public QWidget
{
    Q_OBJECT

public:
    // 构造数据回看页面。
    // @param parent Qt 父控件；作为 wid_stack 子页面时由调用方传入。
    explicit WidReview(QWidget *parent = nullptr);

    // 销毁页面；若后台导入仍在运行，会先请求取消并等待线程结束。
    ~WidReview() override;

    // 启动异步文件导入。
    //
    // 可被文件选择框、拖放和命令行启动参数复用。
    // 该方法只做快速路径校验并创建工作线程，真正的包头扫描在后台完成。
    //
    // @param filePath 用户选择的 BIN 文件路径。
    // @return 成功启动导入返回 true；正在导入或路径非法返回 false。
    bool loadFile(const QString &filePath);

    // 对数据浏览表中指定的当前页行列赋值。
    // @param value 需要写入单元格的文本。
    // @param row 当前页表格行索引，从 0 开始。
    // @param column 数据浏览表列索引，范围 0-5。
    // @return 行列有效且赋值成功时返回 true。
    bool setDataValue(const QString &value, int row, int column);

protected:
    // 处理拖动进入事件。
    // @param event Qt 拖动事件对象，用于判断 MIME 数据和接受/拒绝拖动。
    void dragEnterEvent(QDragEnterEvent *event) override;

    // 处理拖动离开事件。
    // @param event Qt 拖动离开事件对象，用于结束拖入高亮状态。
    void dragLeaveEvent(QDragLeaveEvent *event) override;

    // 处理文件放下事件。
    // @param event Qt 放下事件对象，用于读取拖入的本地文件 URL。
    void dropEvent(QDropEvent *event) override;

    // 处理页面尺寸变化。
    // @param event Qt 尺寸变化事件对象，用于触发字节列等宽重算。
    void resizeEvent(QResizeEvent *event) override;

private slots:
    // 打开文件选择对话框；无参数。
    void chooseFile();

    // 清空当前文件；导入期间执行取消操作；无参数。
    void clearFile();

    // 切换到上一页；无参数。
    void showPreviousPage();

    // 切换到下一页；无参数。
    void showNextPage();

    // 根据页码输入框切换到指定页；无参数。
    void jumpToPage();

    // 响应总线类型下拉框变化。
    // @param index 当前下拉项索引：0=未选择，1=FC，2=Aurora，3=以太网。
    void handleBusTypeChanged(int index);

    // 根据数据包列表当前选中行加载数据段详情；无参数。
    void updatePacketDetails();

    // 响应可编辑字节单元格内容变化。
    // @param item 内容发生变化的表格项。
    void handleHexItemChanged(QTableWidgetItem *item);

    // 切换数据段显示和编辑进制。
    // @param index 下拉项索引：0=16进制，1=10进制。
    void changeNumberBase(int index);

    // 将当前数据段修改原位写回 BIN 文件；无参数。
    void saveCurrentPacket();

    // 更新后台导入进度。
    // @param processedBytes 已扫描字节数。
    // @param totalBytes 文件总字节数。
    // @param packetCount 已识别数据包数量。
    void handleImportProgress(qint64 processedBytes, qint64 totalBytes, int packetCount);

    // 接收后台导入成功结果。
    // @param packets 数据包元数据列表。
    // @param filePath 导入文件绝对路径。
    // @param fileSize 导入文件字节大小。
    void handleImportFinished(const QVector<BinPacket> &packets,
                              const QString &filePath,
                              qint64 fileSize);

    // 处理后台导入失败。
    // @param message 详细错误信息。
    void handleImportFailed(const QString &message);

    // 处理后台导入取消；无参数。
    void handleImportCancelled();

private:
    // 数据段显示和编辑使用的数字进制。
    enum class NumberBase {
        Hexadecimal,
        Decimal
    };

    // 当前文件所属的总线协议。
    enum class BusType {
        None,
        FC,
        Aurora,
        Ethernet
    };

    // 初始化数据包元数据列表；无参数。
    void configureTable();

    // 数据浏览表使用的列结构。
    enum class PacketTableSchema {
        Empty,
        Aurora,
        FC,
        Ethernet
    };

    // 根据总线类型切换数据浏览表列名和隐藏列。
    // @param schema Empty 清空列名，Aurora 显示 6 列，FC/Ethernet 显示 4 列。
    void setPacketTableSchema(PacketTableSchema schema);

    // 初始化可编辑字节表格；无参数。
    void configureHexTable();

    // 根据表格实际可视宽度重新分配 16 个字节列，保证每列等宽；无参数。
    void updateHexColumnWidths();

    // 将已选择的文件信息显示到界面。
    // @param filePath 已选择文件的路径。
    void applySelectedFileInfo(const QString &filePath);

    // 按当前进制格式化一个字节。
    // @param value 0-255 范围内的单字节数值。
    // @return 16进制返回两位大写 HEX，10进制返回十进制文本。
    QString formatDisplayByte(quint8 value) const;

    // 仅刷新单元格显示文本，不改变底层字节内容；无参数。
    void refreshHexTableValues();

    // 从 BIN 文件按需读取指定数据包的数据段。
    // @param packetIndex 数据包在 m_packets 中的全局索引；-1 表示清空。
    void loadHexPayload(int packetIndex);

    // 保存成功后同步刷新数据包列表中的前 12 字节预览；无参数。
    void updatePacketPreviewRow();

    // 创建并启动后台导入线程。
    // @param filePath 已确认、准备扫描的 BIN 文件路径。
    // @param protocol 数据段解析协议；FC 使用独立类但当前规则与 TCP 相同。
    // @param busType 用户选择的总线类型，用于界面列结构和总线状态。
    void beginImport(const QString &filePath,
                     BinFormat::Protocol protocol,
                     BusType busType);

    // 统一切换导入期间控件状态。
    // @param importing true 表示进入导入状态，false 表示恢复交互。
    void setImportingUi(bool importing);

    // 将界面和数据状态恢复到未载入文件状态；无参数。
    void resetView();

    // 只渲染当前页，避免一次性创建全部数据包行；无参数。
    void populateCurrentPage();

    // 同步页码、范围、跳转输入框和翻页按钮状态；无参数。
    void updatePagination();

    // 设置拖入区域高亮。
    // @param highlighted true 表示显示拖入高亮，false 表示恢复普通边框。
    void setDropHighlighted(bool highlighted);

    // 设置文件状态提示。
    // @param text 提示文本。
    // @param state QSS 状态名，通常为 idle、success 或 error。
    void setFileState(const QString &text, const QString &state);

    // 显示导入错误并将界面恢复到安全状态。
    // @param message 面向用户的详细错误信息。
    void showLoadError(const QString &message);

    // 更新数据回看页面底部状态栏文本。
    // @param text 需要显示在底部状态栏的文本。
    void setReviewStatus(const QString &text);

    // Qt Designer 生成的界面对象。
    Ui::WidReview *ui;

    // 当前文件的全部数据包元数据，不包含完整数据段。
    QVector<BinPacket> m_packets;

    // 当前文件绝对路径，用于按需读取和原位保存。
    QString m_currentFilePath;

    // 下次打开文件对话框时使用的目录。
    QString m_lastDirectory;

    // 用户已选择、等待总线类型或已解析的文件。
    QString m_selectedFilePath;
    qint64 m_selectedFileSize = 0;
    BusType m_busType = BusType::None;

    // 从 0 开始的当前分页索引，界面显示页码时加 1。
    int m_currentPage = 0;

    // 后台导入线程及其工作对象。
    QThread *m_importThread = nullptr;
    BinFileImporterBase *m_importer = nullptr;

    // 状态栏中的导入进度条。
    QProgressBar *m_importProgress = nullptr;

    // 页面内部底部状态栏中的状态文本。
    QLabel *m_reviewStatusLabel = nullptr;

    // 是否正在执行后台导入。
    bool m_isImporting = false;

    // 导入代次，用于忽略已经过期的线程信号。
    quint64 m_importGeneration = 0;

    // 当前数据段加载出来时的原始内容和编辑中的内容。
    QByteArray m_originalPayload;
    QByteArray m_editablePayload;

    // 当前可编辑数据段所属的数据包全局索引。
    int m_editablePacketIndex = -1;

    // 当前数据段是否有尚未保存的修改。
    bool m_hexDirty = false;

    // 批量填充表格时阻止 itemChanged 信号触发业务逻辑。
    bool m_populatingHexTable = false;

    // 当前数据段的显示和编辑进制。
    NumberBase m_numberBase = NumberBase::Hexadecimal;
};
