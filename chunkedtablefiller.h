#pragma once

#include <QList>
#include <QObject>

#include <functional>

class QTableWidget;
class QTableWidgetItem;
class QTimer;

// 表格分块填充器。
//
// 作用：把“创建大量 QTableWidgetItem”的工作拆成多个事件循环批次，
// 避免一次性创建数万个单元格导致界面卡死（典型场景：数据回看页
// Aurora 单包 64 KB ≈ 4096 行 × 17 列）。
//
// 使用方式（数据回看页 loadHexPayload() 即此用法）：
//   m_filler = new ChunkedTableFiller(ui->hexTable, this);
//   connect(m_filler, &ChunkedTableFiller::finished, this, ...);
//   m_filler->start(rowCount, [this](int row) {
//       QList<QTableWidgetItem *> items;
//       items << new QTableWidgetItem(...)   // 第 0 列
//             << new QTableWidgetItem(...);  // 第 1 列 ...
//       return items;
//   });
//
// 行为约定：
//   - start() 会先取消旧任务，再设置新行数；
//   - 填充期间自动关闭表格重绘并屏蔽 itemChanged 信号；
//   - 每批默认 128 行，批与批之间把控制权交回事件循环；
//   - 全部完成后恢复重绘并发出 finished()；
//   - 调用方随时可以 cancel() 中止（例如切换到别的数据包）。
class ChunkedTableFiller : public QObject
{
    Q_OBJECT

public:
    // 行构建回调：为指定行创建并返回按列顺序排列的单元格。
    // 参数 row 从 0 开始；返回列表的第 i 项会放入表格第 i 列。
    using RowBuilder = std::function<QList<QTableWidgetItem *>(int row)>;

    // 构造填充器。
    // @param table 需要填充的表格控件。
    // @param parent Qt 父对象。
    explicit ChunkedTableFiller(QTableWidget *table, QObject *parent = nullptr);

    // 析构时自动取消未完成的任务。
    ~ChunkedTableFiller() override;

    // 设置每批填充的行数，默认 128。
    // 数值越大总耗时略短但单次停顿变长；建议 64~256。
    void setRowsPerChunk(int rows);

    // 开始填充。
    // @param rowCount 需要填充的总行数。
    // @param builder 行构建回调；rowCount 为 0 时不创建任何行。
    void start(int rowCount, RowBuilder builder);

    // 取消当前填充并恢复表格重绘。
    // 取消后表格保留已创建的行与单元格。
    void cancel();

    // 是否有未完成的填充任务。
    bool isRunning() const;

    // 已填充完成的行数，便于调试或进度显示。
    int filledRows() const;

signals:
    // 全部行填充完成。
    void finished();

private:
    // 定时器回调：填充下一批行。
    void fillNextChunk();

    QTableWidget *m_table = nullptr;
    QTimer *m_timer = nullptr;
    RowBuilder m_builder;
    int m_rowsPerChunk = 128; // 每批行数。
    int m_rowCount = 0;       // 总行数。
    int m_nextRow = 0;        // 下一批起始行。
    bool m_running = false;   // 是否正在填充。
};
