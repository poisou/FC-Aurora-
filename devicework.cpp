#include "devicework.h"
#include "aurorarecvpro.h"
#include "aurorasendpro.h"
#include "binformat.h"
#include "fcrecvpro.h"
#include "fcsendpro.h"
#include "tcprecvpro.h"
#include "tcpsendpro.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QStringList>
#include <QThread>

namespace {

// 三条总线的默认单文件上限，直接取自各自的宏（默认均为 1 GB）：
// [0]=FC，[1]=Aurora，[2]=以太网。
// 运行时可用 setReceiveSaveMaxBytes() 覆盖，传 0 时回到这里的宏默认值。
const qint64 kRecvSaveMaxBytes[3] = {
    FC_RECV_SAVE_MAX_BYTES,
    AURORA_RECV_SAVE_MAX_BYTES,
    TCP_RECV_SAVE_MAX_BYTES
};

// 三条总线的落盘开关（编译期常量，索引与 setReceiveSaveBus() 一致）。
// [0]=FC，[1]=Aurora，[2]=以太网；对应宏为 0 时该总线不写文件。
const bool kRecvSaveSwitches[3] = {
#if FC_RECV_SAVE_ENABLED
    true,
#else
    false,
#endif
#if AURORA_RECV_SAVE_ENABLED
    true,
#else
    false,
#endif
#if TCP_RECV_SAVE_ENABLED
    true,
#else
    false,
#endif
};

// 文件名中的总线类型后缀，顺序与 m_recvSaveBus 一一对应。
const char *const kBusNameSuffixes[3] = { "FC", "Aurora", "TCP" };

} // namespace

DeviceWorkTask::DeviceWorkTask(int ch,
                               DeviceWork::Direction direction,
                               const QString &sourcePath)
    : m_ch(ch)
    , m_sourcePath(sourcePath)
    , m_direction(direction)
    , m_running(1)
    , m_paused(0)
{
}

DeviceWorkTask::~DeviceWorkTask()
{
    if (m_file != nullptr) {
        m_file->close();
        delete m_file;
        m_file = nullptr;
    }

    // 关闭落盘文件，确保线程结束前缓冲区数据已经写出。
    if (m_recvSaveFile != nullptr) {
        m_recvSaveFile->flush();
        m_recvSaveFile->close();
        delete m_recvSaveFile;
        m_recvSaveFile = nullptr;
    }
}

void DeviceWorkTask::requestStop()
{
    m_running.storeRelease(0);
}

void DeviceWorkTask::setPaused(bool paused)
{
    m_paused.storeRelease(paused ? 1 : 0);
}

void DeviceWorkTask::setReceiveSaveBus(int busIndex)
{
    // 超出范围时按以太网（文件名后缀 _TCP）处理，避免数组越界。
    m_recvSaveBus = (busIndex >= 0 && busIndex < 3) ? busIndex : 2;

    // 单文件上限随之切换到该总线宏的默认值；
    // 若需要自定义，请在设置总线之后再调用 setReceiveSaveMaxBytes()。
    m_recvSaveMaxBytes = kRecvSaveMaxBytes[m_recvSaveBus];
}

// 设置单文件上限；传 <= 0 时恢复本总线宏的默认值。
// 优先级：setReceiveSaveMaxBytes() 的运行时设置 > 本总线宏 > 默认 1 GB。
// 注意：调用 setReceiveSaveBus() 会重新用宏默认值覆盖本设置，
//       因此自定义上限应在创建对象（构造函数已设置总线）之后再调用。
void DeviceWorkTask::setReceiveSaveMaxBytes(qint64 bytes)
{
    m_recvSaveMaxBytes =
        bytes > 0 ? bytes : kRecvSaveMaxBytes[m_recvSaveBus];
}

// 返回当前生效的单文件上限（字节）：
// 若调用过 setReceiveSaveMaxBytes() 则为自定义值，否则是该总线宏的值。
qint64 DeviceWorkTask::receiveSaveMaxBytes() const
{
    return m_recvSaveMaxBytes;
}

// 接收数据落盘：QByteArray 重载，转发到 char* + 长度实现。
bool DeviceWorkTask::saveReceivedData(const QByteArray &data)
{
    return saveReceivedData(data.constData(),
                            static_cast<qint64>(data.size()));
}

// 接收数据落盘完整流程（三个总线的 recvpro 共用）：
//   1) 空指针或长度 <= 0 直接返回 false；
//   2) 按 m_recvSaveBus 检查该总线开关宏，关闭时返回 false；
//   3) 还没有文件，或“现有大小 + 本次长度”超过 m_recvSaveMaxBytes
//      （默认 1 GB）时，调用 openReceiveSaveFile() 关闭旧文件并按时间新建；
//   4) 追加写入；失败则关闭并释放句柄，等下一笔数据重新建文件；
//   5) flush 落盘，避免异常退出丢数据。
// 保存位置：可执行文件目录/SAVADATA/<年月日时分>_<总线类型>.bin
bool DeviceWorkTask::saveReceivedData(const char *data, qint64 length)
{
    // 1) 参数校验：二进制数据必须显式给长度，不能用 strlen()。
    if (data == nullptr || length <= 0) {
        return false;
    }

    // 2) 该总线的开关宏关闭时不写文件。
    if (!kRecvSaveSwitches[m_recvSaveBus]) {
        return false;
    }

    // 3) 首次写入或即将超过单文件上限：新建下一个文件。
    const qint64 currentSize =
        m_recvSaveFile != nullptr ? m_recvSaveFile->size() : 0;
    if (m_recvSaveFile == nullptr
        || currentSize + length > m_recvSaveMaxBytes) {
        if (!openReceiveSaveFile()) {
            return false;
        }
    }

    // 4) 追加写入；失败时释放句柄，下次数据到来重新建文件。
    if (m_recvSaveFile->write(data, length) != length) {
        m_recvSaveFile->close();
        delete m_recvSaveFile;
        m_recvSaveFile = nullptr;
        return false;
    }

    // 5) 及时刷盘。
    m_recvSaveFile->flush();
    return true;
}

// 新建落盘文件：
//   1) 关闭并释放上一个文件；
//   2) 确保 可执行文件目录/SAVADATA 存在；
//   3) 以“年月日时分_总线类型.bin”命名，例：202611111530_Aurora.bin；
//      同一分钟内分卷时追加"_序号"避免覆盖；
//   4) 以只写方式打开并保存句柄。
bool DeviceWorkTask::openReceiveSaveFile()
{
    // 1) 关闭上一个文件。
    if (m_recvSaveFile != nullptr) {
        m_recvSaveFile->flush();
        m_recvSaveFile->close();
        delete m_recvSaveFile;
        m_recvSaveFile = nullptr;
    }

    // 2) 保存目录：可执行文件目录/SAVADATA（三条总线共用）。
    const QString directoryPath =
        QDir(QCoreApplication::applicationDirPath())
            .filePath(QStringLiteral("SAVADATA"));
    if (!QDir().mkpath(directoryPath)) {
        return false;
    }

    // 3) 文件名：年月日时分_总线类型.bin；同分钟分卷追加序号。
    const QString baseName =
        QDateTime::currentDateTime()
            .toString(QStringLiteral("yyyyMMddHHmm"))
        + QLatin1Char('_')
        + QString::fromLatin1(kBusNameSuffixes[m_recvSaveBus]);
    QString filePath =
        QDir(directoryPath).filePath(baseName + QStringLiteral(".bin"));
    int suffix = 1;
    while (QFile::exists(filePath)) {
        filePath = QDir(directoryPath)
                       .filePath(QStringLiteral("%1_%2.bin")
                                     .arg(baseName)
                                     .arg(suffix++));
    }

    // 4) 打开文件并保存句柄。
    auto *newFile = new QFile(filePath);
    if (!newFile->open(QIODevice::WriteOnly)) {
        delete newFile;
        return false;
    }

    m_recvSaveFile = newFile;
    return true;
}

void DeviceWorkTask::doWork()
{
    while (m_running.loadAcquire() != 0) {
        if (m_paused.loadAcquire() != 0) {
            QThread::msleep(20);
            continue;
        }

        processData();
        QThread::msleep(20);
    }
}

void DeviceWorkTask::processData()
{
    // 默认不处理数据，具体总线在子类中实现。
}

bool DeviceWorkTask::openSourceFile()
{
    if (m_file != nullptr && m_file->isOpen()) {
        return true;
    }
    if (m_sourcePath.isEmpty()) {
        return false;
    }

    if (m_file == nullptr) {
        m_file = new QFile(m_sourcePath);
    }
    if (!m_file->isOpen() && !m_file->open(QIODevice::ReadOnly)) {
        return false;
    }

    // 记录数据源总长，供 sendJD 计算发送百分比。
    m_totalBytes = m_file->size();
    return true;
}

void DeviceWorkTask::reportSendProgress()
{
    // 已发送字节 m_position / 文件总长 m_totalBytes，
    // 页面按此换算百分比并刷新进度条。
    emit sendJD(m_position, m_totalBytes);
}

bool DeviceWorkTask::readNextPacket(QByteArray &packet, bool blockPayload)
{
    if (!openSourceFile()) {
        return false;
    }

    const qint64 fileSize = m_file->size();
    if (fileSize < BinFormat::HeaderSize) {
        return false;
    }

    // 剩余数据不足一个包头时，回到文件开头重新发送。
    // 若此前已经发送过数据，说明本次是一轮发送结束，先上报 100%。
    if (m_position + BinFormat::HeaderSize > fileSize) {
        if (m_position > 0) {
            emit sendJD(fileSize, fileSize);
        }
        m_position = 0;
        return false;
    }

    if (!m_file->seek(m_position)) {
        m_position = 0;
        return false;
    }

    const QByteArray header = m_file->read(BinFormat::HeaderSize);
    if (header.size() != BinFormat::HeaderSize) {
        m_position = 0;
        return false;
    }

    // n 由包头最后两个字节解析；Aurora 为 n × 1024，FC / 以太网为 n。
    const int n = BinFormat::decodeDataCount(header);
    const qint64 payloadSize =
        blockPayload ? static_cast<qint64>(BinFormat::PayloadSize) * n : n;
    const qint64 frameSize = BinFormat::HeaderSize + payloadSize;

    // 文件剩余长度不足一帧时回到开头，避免发送残缺帧。
    // 同样先上报 100%，再从头开始下一轮。
    if (m_position + frameSize > fileSize) {
        if (m_position > 0) {
            emit sendJD(fileSize, fileSize);
        }
        m_position = 0;
        return false;
    }

    QByteArray frame = header;
    if (payloadSize > 0) {
        frame.append(m_file->read(payloadSize));
        if (frame.size() != frameSize) {
            m_position = 0;
            return false;
        }
    }

    packet = frame;
    return true;
}

bool DeviceWorkTask::send(const QByteArray &data)
{
    // 默认不实际发送，子类按总线类型重写。
    Q_UNUSED(data)
    return true;
}

DeviceWork::DeviceWork(const QString &deviceName,
                       int ch,
                       Direction direction,
                       QObject *parent)
    : QObject(parent)
    , m_deviceName(deviceName)
    , m_ch(ch)
    , m_direction(direction)
{
}

DeviceWork::~DeviceWork()
{
    stopWork();
}

void DeviceWork::setSourceFile(const QString &filePath)
{
    m_sourcePath = filePath;
}

void DeviceWork::setTcpTarget(const QString &address, quint16 port)
{
    m_tcpAddress = address;
    m_tcpPort = port;
}

void DeviceWork::setTcpLocal(const QString &address, quint16 port)
{
    m_localAddress = address;
    m_localPort = port;
}

DeviceWork::Direction DeviceWork::direction() const
{
    return m_direction;
}

bool DeviceWork::isRunning() const
{
    return m_thread != nullptr;
}

// 启动线程。
// 关键步骤：
//   1. 创建 QThread，并按“设备名_方向”命名，便于调试时识别线程；
//   2. createTask() 按设备类型创建具体工作对象；
//   3. moveToThread 把工作对象迁入子线程（对象必须无父对象）；
//   4. 线程启动信号触发 doWork()，工作对象开始循环处理数据。
// 注意：所有 setSourceFile / setTcpTarget / setTcpLocal 都应在调用本函数前完成。
void DeviceWork::startWork()
{
    if (m_thread != nullptr) {
        // 已启动（可能处于暂停状态），恢复运行即可。
        // 工作对象保留读取位置，从上次发送位置继续。
        m_task->setPaused(false);
        return;
    }

    m_thread = new QThread;
    m_thread->setObjectName(
        QStringLiteral("%1_%2")
            .arg(m_deviceName,
                 m_direction == Send ? QStringLiteral("send")
                                     : QStringLiteral("recv")));

    m_task = createTask();
    m_task->moveToThread(m_thread);

    connect(m_thread, &QThread::started, m_task, &DeviceWorkTask::doWork);

    // 任务对象在子线程中收到数据后，经本对象转发到 GUI 线程。
    connect(m_task,
            &DeviceWorkTask::dataReceived,
            this,
            &DeviceWork::dataReceived);

    // 发送进度同样经本对象转发到 GUI 线程。
    connect(m_task, &DeviceWorkTask::sendJD, this, &DeviceWork::sendJD);

    m_thread->start();
}

void DeviceWork::pauseWork()
{
    if (m_task != nullptr) {
        m_task->setPaused(true);
    }
}

// 停止线程。
// 顺序很重要：先置停止标志让 doWork() 退出循环，再 quit() 结束事件循环，
// wait() 等待线程真正结束后才能删除任务对象，避免跨线程释放造成崩溃。
void DeviceWork::stopWork()
{
    if (m_thread == nullptr) {
        return;
    }

    m_task->requestStop();
    m_thread->quit();
    m_thread->wait();

    // 线程已经完全退出，此时在工作线程外释放任务对象不会产生竞态。
    delete m_task;
    m_task = nullptr;

    delete m_thread;
    m_thread = nullptr;
}

DeviceWorkTask *DeviceWork::createTask() const
{
    if (m_direction == Send) {
        if (m_deviceName.startsWith(QStringLiteral("Aurora"))) {
            return new AuroraSendPro(m_ch, m_sourcePath);
        }
        if (m_deviceName.startsWith(QStringLiteral("FC设备"))) {
            return new FCSendPro(m_ch, m_sourcePath);
        }
        return new TcpSendPro(m_ch, m_sourcePath, m_tcpAddress, m_tcpPort);
    }

    if (m_deviceName.startsWith(QStringLiteral("Aurora"))) {
        return new AuroraRecvPro(m_ch);
    }
    if (m_deviceName.startsWith(QStringLiteral("FC设备"))) {
        return new FCRecvPro(m_ch);
    }
    if (m_deviceName == QStringLiteral("以太网设备")) {
        return new TcpRecvPro(m_ch, m_localAddress, m_localPort);
    }

    return new DeviceWorkTask(m_ch, m_direction, m_sourcePath);
}
