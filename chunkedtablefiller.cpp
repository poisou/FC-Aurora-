#include "chunkedtablefiller.h"

#include <QSignalBlocker>
#include <QTableWidget>
#include <QTimer>

ChunkedTableFiller::ChunkedTableFiller(QTableWidget *table, QObject *parent)
    : QObject(parent)
    , m_table(table)
    , m_timer(new QTimer(this))
{
    // 单次触发 + 0ms：每批完成后立即排队下一批，同时让出事件循环，
    // 保证填充期间界面仍可响应点击、滚动和翻页。
    m_timer->setSingleShot(true);
    m_timer->setInterval(0);
    connect(m_timer, &QTimer::timeout, this, &ChunkedTableFiller::fillNextChunk);
}

ChunkedTableFiller::~ChunkedTableFiller()
{
    cancel();
}

void ChunkedTableFiller::setRowsPerChunk(int rows)
{
    m_rowsPerChunk = qMax(1, rows);
}

void ChunkedTableFiller::start(int rowCount, RowBuilder builder)
{
    cancel();
    if (m_table == nullptr) {
        return;
    }

    m_builder = std::move(builder);
    m_rowCount = qMax(0, rowCount);
    m_nextRow = 0;
    m_running = (m_rowCount > 0 && m_builder);

    {
        // 重建行期间屏蔽 itemChanged，避免每个单元格触发业务逻辑。
        QSignalBlocker blocker(m_table);
        m_table->setRowCount(0);
        if (!m_running) {
            return;
        }

        // 填充期间关闭重绘，完成后统一刷新一次，避免反复整表重绘。
        m_table->setUpdatesEnabled(false);
        m_table->setRowCount(m_rowCount);
    }

    m_timer->start();
}

void ChunkedTableFiller::cancel()
{
    if (m_timer != nullptr) {
        m_timer->stop();
    }
    // 注意：这里不主动恢复表格重绘。
    // 调用方（例如数据回看页清空表格）通常会在 cancel() 之后立即
    // setRowCount(0) 删除大量单元格，如果此时重绘已开启，Qt 会在逐行
    // 删除过程中反复触发重绘，造成明显卡顿。
    // 因此恢复重绘的时机交给调用方统一处理。
    m_running = false;
    m_nextRow = 0;
    m_rowCount = 0;
    m_builder = nullptr;
}

bool ChunkedTableFiller::isRunning() const
{
    return m_running;
}

int ChunkedTableFiller::filledRows() const
{
    return m_nextRow;
}

void ChunkedTableFiller::fillNextChunk()
{
    if (!m_running || !m_builder || m_table == nullptr) {
        return;
    }

    const int endRow = qMin(m_nextRow + m_rowsPerChunk, m_rowCount);
    {
        QSignalBlocker blocker(m_table);
        for (int row = m_nextRow; row < endRow; ++row) {
            const QList<QTableWidgetItem *> items = m_builder(row);
            const int columnCount =
                qMin(items.size(), m_table->columnCount());
            for (int column = 0; column < columnCount; ++column) {
                m_table->setItem(row, column, items.at(column));
            }
        }
    }

    m_nextRow = endRow;

    if (m_nextRow < m_rowCount) {
        m_timer->start();
        return;
    }

    // 全部完成：恢复重绘并通知调用方（例如重算列宽）。
    m_running = false;
    m_table->setUpdatesEnabled(true);
    m_table->viewport()->update();
    emit finished();
}
