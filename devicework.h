#pragma once

#include <QAtomicInt>
#include <QByteArray>
#include <QList>
#include <QObject>
#include <QString>

class QFile;
class QThread;
class DeviceWorkTask;

// =============================================================================
// 接收数据落盘开关（三条总线各自独立控制）
//
// 1：开启——recvpro 调用 saveReceivedData() 时，将该总线收到的数据
//    以二进制形式追加保存到可执行文件目录下：
//      可执行文件目录/SAVADATA/<年月日时分>_<总线类型>.bin
//      FC_RECV_SAVE_ENABLED     → 文件名后缀 _FC
//      AURORA_RECV_SAVE_ENABLED → 文件名后缀 _Aurora
//      TCP_RECV_SAVE_ENABLED    → 文件名后缀 _TCP
//    单个文件上限见下方 FC/AURORA/TCP_RECV_SAVE_MAX_BYTES 宏（默认 1 GB），
//    超过后自动新建下一个文件。
// 0：关闭——saveReceivedData() 直接返回 false，不创建、不写入文件。
//
// 需要调整时修改下面的默认值，或在编译参数里定义同名宏覆盖。
// =============================================================================
#ifndef FC_RECV_SAVE_ENABLED
#define FC_RECV_SAVE_ENABLED 1
#endif

#ifndef AURORA_RECV_SAVE_ENABLED
#define AURORA_RECV_SAVE_ENABLED 1
#endif

#ifndef TCP_RECV_SAVE_ENABLED
#define TCP_RECV_SAVE_ENABLED 1
#endif

// =============================================================================
// 接收数据单文件大小上限（三条总线各自独立控制）
//
// 写入前判断“当前文件大小 + 本次数据长度”是否超过该值，
// 超过则关闭旧文件、新建下一个文件继续保存。
//
//   FC_RECV_SAVE_MAX_BYTES     → FC 设备单文件上限
//   AURORA_RECV_SAVE_MAX_BYTES → Aurora 设备单文件上限
//   TCP_RECV_SAVE_MAX_BYTES    → 以太网设备单文件上限
//
// 三个宏默认均为 1 GB；单位是字节，可用表达式书写，例如 500LL * 1024 * 1024。
// 修改示例（只把 Aurora 改成 300 MB）：
//   #define AURORA_RECV_SAVE_MAX_BYTES (300LL * 1024 * 1024)
// 也可在 .pro 中统一定义：DEFINES += AURORA_RECV_SAVE_MAX_BYTES=...(表达式需加括号)
//
// 运行时也可调用 setReceiveSaveMaxBytes() 覆盖（传 0 恢复本宏默认值）；
// 优先级：运行时设置 > 本宏 > 1 GB。
// =============================================================================
#ifndef FC_RECV_SAVE_MAX_BYTES
#define FC_RECV_SAVE_MAX_BYTES (1024LL * 1024 * 1024)
#endif

#ifndef AURORA_RECV_SAVE_MAX_BYTES
#define AURORA_RECV_SAVE_MAX_BYTES (1024LL * 1024 * 1024)
#endif

#ifndef TCP_RECV_SAVE_MAX_BYTES
#define TCP_RECV_SAVE_MAX_BYTES (1024LL * 1024 * 1024)
#endif

// 设备收发工作类（线程管理器）。
//
// 工作方式：
//   1. 每个设备通道创建一个 DeviceWork，方向由 Direction 指定；
//   2. 调用方（场景并行页面）在本对象上设置发送参数；
//   3. startWork() 内部 new QThread，并创建具体的任务对象
//      （FCSendPro / FCRecvPro / AuroraSendPro / AuroraRecvPro /
//        TcpSendPro / TcpRecvPro），通过 moveToThread 放入该线程；
//   4. pauseWork() 只置暂停标志，线程和读取位置都保留；
//   5. stopWork() 结束线程并释放任务对象，再次 startWork() 会重新创建线程。
//
// 发送线程参数设置示例（全部在 GUI 线程、startWork() 之前调用）：
//   DeviceWork *work = new DeviceWork(QStringLiteral("Aurora设备1通道"),
//                                     1, DeviceWork::Send, this);
//   work->setSourceFile(sourcePath);   // 数据源文件路径，必须设置
//   work->startWork();                 // 启动后才开始按帧读取并发送
//
// 以太网发送还需额外设置对端地址：
//   work->setTcpTarget(QStringLiteral("192.168.1.100"), 8080);
//
// 以太网接收需要设置本地监听地址：
//   work->setTcpLocal(QStringLiteral("0.0.0.0"), 8080);
class DeviceWork : public QObject
{
    Q_OBJECT

public:
    // 工作方向。
    enum Direction {
        Send,    // 发送线程：从数据源文件读帧并发送
        Receive  // 接收线程：从总线/网络接收数据并上报
    };

    // @param deviceName 设备名称，例如“FC设备1通道”“Aurora设备1通道”
    //                   “以太网设备”。用于线程命名，并在 createTask()
    //                   中据此选择具体的工作类。
    // @param ch 通道号，从 1 开始；接收数据时作为“通道号”列显示。
    // @param direction 工作方向，Send 或 Receive。
    // @param parent Qt 父对象；本对象始终留在 GUI 线程，只有任务对象
    //               会被 moveToThread 移到子线程。
    DeviceWork(const QString &deviceName,
               int ch,
               Direction direction,
               QObject *parent = nullptr);

    // 停止线程并释放资源；对象析构时会自动调用 stopWork()。
    ~DeviceWork() override;

    // 设置发送使用的数据源文件路径（仅发送线程需要）。
    //
    // 只保存路径，真正的文件读取在子线程中按帧进行，因此 1GB 级文件
    // 不会整体载入内存。
    //
    // 调用方式：
    //   - 在 startWork() 之前调用；已启动时修改需先 stopWork() 再设置；
    //   - 文件格式为连续的“6 字节包头 + 数据段”：
    //       Aurora：数据段长度 = n × 1024，n 取包头最后两个字节；
    //       FC / 以太网：数据段长度 = n，n 取包头最后两个字节；
    //     字节序由 binformat.h 的 BIN_HEADER_BIG_ENDIAN 统一控制。
    //
    // @param filePath 数据源文件的绝对路径；传空字符串表示不发送。
    void setSourceFile(const QString &filePath);

    // 设置以太网发送线程的对端地址和端口（仅以太网发送需要）。
    //
    // 调用方式：startWork() 之前调用。线程内部作为 TCP 客户端
    // 使用 QTcpSocket::connectToHost 连接该地址，断线后自动重连。
    //
    // @param address 对端 IP，例如 "192.168.1.100"。
    // @param port 对端端口，1~65535；传 0 表示不连接、不发送。
    void setTcpTarget(const QString &address, quint16 port);

    // 设置以太网接收线程监听的本地地址和端口（仅以太网接收需要）。
    //
    // 调用方式：startWork() 之前调用。线程内部作为 TCP 服务端
    // 使用 QTcpServer::listen 监听该地址，等待对端连接后接收数据。
    //
    // @param address 本地 IP；传空字符串表示监听所有 IPv4 地址
    //                （等价于 QHostAddress::AnyIPv4）。
    // @param port 本地监听端口，1~65535。
    void setTcpLocal(const QString &address, quint16 port);

    // 返回本对象的工作方向，用于页面区分发送/接收线程。
    Direction direction() const;

    // 启动工作线程。
    //
    // 首次调用：创建 QThread 和对应任务对象，moveToThread 后启动线程。
    // 再次调用（暂停后恢复）：仅清除暂停标志，线程与读取位置保持不变，
    // 因此会从上次发送位置继续。
    void startWork();

    // 暂停工作线程：线程保持运行，但不再处理数据。
    // 读取位置不会重置，startWork() 后继续。
    void pauseWork();

    // 停止工作线程：置停止标志 → quit() → wait() → 释放任务对象和线程。
    // 再次 startWork() 会重新创建线程，并从数据源开头重新读取。
    void stopWork();

    // 线程是否已启动（不含暂停状态）。
    bool isRunning() const;

signals:
    // 接收线程收到数据时发出，由页面写入对应总线的接收容器。
    // 任务对象在子线程 emit，经 Qt 队列连接自动回到 GUI 线程。
    // @param data 收到的原始数据。
    void dataReceived(const QByteArray &data);

    // 发送进度：任务对象每成功发送一帧后发出，经队列连接回到 GUI 线程。
    // 页面据此刷新“启动”按钮下方的进度条。
    // @param sentBytes 已发送字节数（即任务对象当前的读取位置）。
    // @param totalBytes 数据源文件总字节数；为 0 表示未知。
    void sendJD(qint64 sentBytes, qint64 totalBytes);

private:
    // 按“设备名称 + 方向”创建具体的工作对象：
    //   Send + Aurora    → AuroraSendPro
    //   Send + FC        → FCSendPro
    //   Send + 以太网    → TcpSendPro
    //   Receive + Aurora → AuroraRecvPro
    //   Receive + FC     → FCRecvPro
    //   Receive + 以太网 → TcpRecvPro
    DeviceWorkTask *createTask() const;

    QString m_deviceName;
    int m_ch;
    Direction m_direction;
    QString m_sourcePath;
    QString m_tcpAddress;
    quint16 m_tcpPort = 0;
    QString m_localAddress;
    quint16 m_localPort = 0;
    QThread *m_thread = nullptr;
    DeviceWorkTask *m_task = nullptr;
};

// 实际运行在子线程中的工作对象（所有具体收发类的基类）。
//
// 生命周期：由 DeviceWork::createTask() 创建 → moveToThread → 线程启动后
// 执行 doWork() → 线程停止后由 DeviceWork::stopWork() 删除。
//
// doWork() 是一个阻塞循环，因此本类不使用事件循环和定时器：
// 收到数据时通过信号 dataReceived 交给 GUI 线程，暂停/停止通过原子标志控制。
//
// 二次开发要点：
//   - 发送类请重写 processData()，内部调用 readNextPacket() 取一帧，
//     再调用 send(帧)；只有 send() 返回 true 才应前移 m_position。
//   - 接收类请重写 processData()，收到数据时 emit dataReceived(数据)。
class DeviceWorkTask : public QObject
{
    Q_OBJECT

public:
    // @param ch 通道号（从 1 开始），可在子类中用于区分硬件通道。
    // @param direction 工作方向，决定基类默认行为。
    // @param sourcePath 发送使用的数据源文件路径；接收类传空字符串。
    DeviceWorkTask(int ch,
                   DeviceWork::Direction direction,
                   const QString &sourcePath);

    ~DeviceWorkTask() override;

    // 请求停止：把 m_running 置 0，doWork() 下一次循环退出。
    // 由 DeviceWork::stopWork() 在 GUI 线程直接调用（原子操作，线程安全）。
    void requestStop();

    // 设置暂停标志（原子操作，可从 GUI 线程调用）。
    // @param paused true 表示暂停，false 表示恢复。
    void setPaused(bool paused);

    // ── 接收数据落盘接口（三条总线的 recvpro 都可直接调用）──────────
    // 把收到的数据以二进制形式追加保存到：
    //   可执行文件目录/SAVADATA/<年>...<分>_<总线类型>.bin
    //   例：SAVADATA/202611111530_Aurora.bin
    // 三条总线共用一个目录，靠文件名中的总线类型区分。
    // 同一分钟内分卷时自动追加序号（_1、_2 ...），避免覆盖已有文件。
    // 单个文件上限由 setReceiveSaveMaxBytes() 指定，默认 1 GB；
    // 每次写入前判断“现有大小 + 本次长度”是否超限，超过则新建下一个文件。
    //
    // 开关：见本文件顶部的 FC/AURORA/TCP_RECV_SAVE_ENABLED，
    //       对应总线为 0 时本接口直接返回 false，不创建文件。
    //
    // 使用方式（在 recvpro 的 processData() 中，收到数据后按需调用）：
    //   QByteArray frame;                    // 从总线接收接口取到的数据
    //   saveReceivedData(frame);             // ① 落盘成 .bin
    //   emit dataReceived(frame);            // ② 上报界面表格
    // 注意：场景并行页面在收到 dataReceived 时也会落盘一次；
    //       若在 recvpro 中已调用本接口，请不要再让页面重复保存，
    //       两处保留一处即可，避免同一笔数据写两遍。
    //
    // @param data 数据首地址；为空指针时直接返回 false。
    // @param length 实际字节数；<= 0 时直接返回 false。
    //               接收数据是二进制，可能含 0x00，不能用 strlen() 计算。
    // @return 成功写入返回 true；开关关闭、参数非法或打开文件失败返回 false。
    bool saveReceivedData(const char *data, qint64 length);

    // 上方接口的 QByteArray 重载，行为完全一致。
    // @param data 收到的数据；为空时返回 false。
    // @return 成功写入返回 true。
    bool saveReceivedData(const QByteArray &data);

    // 设置单个落盘文件的最大字节数。
    //
    // 默认值来自宏 FC/AURORA/TCP_RECV_SAVE_MAX_BYTES（均默认 1 GB），
    // 由 setReceiveSaveBus() 按总线选取。写入前判断
    // “当前文件大小 + 本次数据长度”是否超过该值，超过则关闭旧文件，
    // 在同目录按当前时间新建下一个文件继续保存。
    //
    // 本接口用于在运行时覆盖宏默认值，可在 recvpro 构造函数或
    // 启动接收线程前调用；只影响之后的分卷判断，不关闭已打开的文件。
    //
    // 调用示例（Aurora 通道限制为 100 MB）：
    //   AuroraRecvPro recv(1);                       // 构造时已设总线
    //   recv.setReceiveSaveMaxBytes(100LL * 1024 * 1024);
    //
    // 优先级：运行时设置 > 本总线宏 > 默认 1 GB。
    // 注意：之后若再调用 setReceiveSaveBus()，上限会被重置为该总线宏默认值。
    //
    // @param bytes 单文件上限（字节）；<= 0 时恢复本总线宏的默认值。
    void setReceiveSaveMaxBytes(qint64 bytes);

    // 返回当前生效的单文件上限（字节）。
    // 未调用过 setReceiveSaveMaxBytes() 时返回本总线宏的值。
    qint64 receiveSaveMaxBytes() const;

    // 设置落盘文件名使用的总线类型（由各 recvpro 构造函数调用）：
    //   0 = FC，1 = Aurora，2 = 以太网（默认），
    //   对应文件名后缀 _FC / _Aurora / _TCP。
    // 调用后会同时把单文件上限切换为该总线宏的默认值，
    // 因此自定义上限需在本函数之后调用 setReceiveSaveMaxBytes()。
    // @param busIndex 总线索引，超出 0~2 时按以太网处理。
    void setReceiveSaveBus(int busIndex);

public slots:
    // 线程入口：循环执行 processData()，每轮间隔 20ms。
    // 暂停时只休眠不处理数据；收到停止请求后返回。
    void doWork();

signals:
    // 收到一笔数据时发出，经 DeviceWork 转发到 GUI 线程。
    // @param data 收到的原始数据。
    void dataReceived(const QByteArray &data);

    // 发送进度信号（所有 sendpro 工作类都继承此信号）。
    // 每成功发送一帧后由 reportSendProgress() 发出；
    // 发送到文件末尾时再额外发一次“已发送 = 总长”，进度条即为 100%。
    // FCSendPro / AuroraSendPro / TcpSendPro 中均直接使用本信号。
    // @param sentBytes 已发送字节数。
    // @param totalBytes 数据源文件总字节数。
    void sendJD(qint64 sentBytes, qint64 totalBytes);

protected:
    // 单次数据处理，子类重写以实现具体的发送或接收逻辑。
    // 基类默认什么也不做。
    virtual void processData();

    // 上报发送进度：emit sendJD(m_position, m_totalBytes)。
    // 发送类在 send() 成功并前移 m_position 之后调用本函数即可，
    // 页面会按 sentBytes / totalBytes 换算成百分比刷新进度条。
    void reportSendProgress();

    // 从数据源文件读取下一帧（仅发送类使用）。
    //
    // 读取过程：
    //   1. seek 到 m_position；
    //   2. 读 6 字节包头，用 BinFormat::decodeDataCount() 解析 n；
    //   3. Aurora 数据段长度 = n × 1024，FC / 以太网 = n；
    //   4. 只读取当前帧，返回完整的“包头 + 数据段”。
    // 若文件剩余不足一帧，会把 m_position 归零并返回 false，
    // 下一轮从头开始发送。
    //
    // @param packet 输出参数：完整一帧数据，可直接交给 send()。
    // @param blockPayload true 表示 Aurora（n × 1024），
    //                     false 表示 FC / 以太网（n）。
    // @return true 表示已读到完整一帧；false 表示暂无可用帧。
    bool readNextPacket(QByteArray &packet, bool blockPayload);

    // 发送一帧数据（仅发送类使用）。
    // 基类为空实现并返回 true；具体总线在子类中重写
    // （TcpSendPro 写入套接字，FC/Aurora 待接入总线下发接口）。
    //
    // @param data 待发送的一帧数据（含包头）。
    // @return true 表示发送成功，调用方会前移 m_position；
    //         false 表示失败，下一轮重发同一帧。
    virtual bool send(const QByteArray &data);

    int m_ch;                // 通道号，从 1 开始。
    QString m_sourcePath;    // 数据源文件路径，接收类为空。
    qint64 m_position = 0;   // 当前已发送到的文件偏移（字节）。
    qint64 m_totalBytes = 0; // 数据源文件总字节数，首次打开文件时记录。

private:
    // 首次读取时打开数据源文件。
    bool openSourceFile();

    // 为当前总线新建一个落盘文件（上一文件会先关闭并释放）。
    // 目录、文件命名与重名处理都在此完成。
    // @return 目录创建并文件打开成功时返回 true。
    bool openReceiveSaveFile();

    DeviceWork::Direction m_direction;
    QFile *m_file = nullptr;
    QAtomicInt m_running; // 0 表示停止。
    QAtomicInt m_paused;  // 1 表示暂停。

    // 接收数据落盘状态：
    // m_recvSaveFile 为当前打开的文件句柄，仅在接收线程中使用；
    // m_recvSaveBus 决定文件名后缀（0=FC，1=Aurora，2=以太网）；
    // m_recvSaveMaxBytes 为单文件大小上限，初值取自对应总线的
    // FC/AURORA/TCP_RECV_SAVE_MAX_BYTES 宏。
    QFile *m_recvSaveFile = nullptr;
    int m_recvSaveBus = 2;
    qint64 m_recvSaveMaxBytes = TCP_RECV_SAVE_MAX_BYTES;
};

// Aurora 发送、FC 发送、TCP 收发以及各总线接收的工作对象
// 分别定义在 aurorasendpro / fcsendpro / tcpsendpro / fcrecvpro /
// aurorarecvpro / tcprecvpro 中，本类只负责按设备类型创建并管理线程。
