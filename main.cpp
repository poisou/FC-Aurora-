#include "mainwindow.h"

#include "binformat.h"

// =============================================================================
// 应用入口
//
// 负责：
//   1. 初始化 Qt 应用级信息；
//   2. 加载全局 QSS；
//   3. 创建主窗口；
//   4. 支持通过命令行参数在启动时直接打开 BIN 文件。
// =============================================================================

#include <QApplication>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QIcon>
#include <QSaveFile>
#include <QStandardPaths>
#include <QString>
#include <QTimer>

namespace {

// 两个生成函数的目标大小：1 GiB。
// 由于每个包还包含 6 字节包头，生成文件会小于或等于该值，
// 但不会为了凑满大小而写入半个包，保证文件可以被程序完整导入。
constexpr qint64 TargetFileSize = 1024LL * 1024 * 1024;

// Aurora 每包由 6 字节包头 + n × 1024 字节数据组成。
// n=1024 时，每包约 1 MB，1 GiB 约生成 1023 包，导入速度较快。
constexpr quint16 AuroraDataCount = 1024;

// TCP 每包由 6 字节包头 + n 字节数据组成，n 最大 1024。
constexpr quint16 TcpDataCount = 1024;

static_assert(
    TargetFileSize
        > BinFormat::HeaderSize
              + static_cast<qint64>(AuroraDataCount) * BinFormat::PayloadSize,
    "Target file size is too small for one Aurora packet.");
static_assert(
    TargetFileSize > BinFormat::HeaderSize + TcpDataCount,
    "Target file size is too small for one TCP packet.");

// 写入包头最后两个字节。字节序与 BIN_HEADER_BIG_ENDIAN 保持一致。
// 如果后续要修改包头 n 的字节序，只需修改 binformat.h 中的宏。
void writeHeaderDataCount(QByteArray &header, quint16 dataCount)
{
#if BIN_HEADER_BIG_ENDIAN
    header[4] = static_cast<char>((dataCount >> 8) & 0xFF);
    header[5] = static_cast<char>(dataCount & 0xFF);
#else
    header[4] = static_cast<char>(dataCount & 0xFF);
    header[5] = static_cast<char>((dataCount >> 8) & 0xFF);
#endif
}

// 获取桌面目录中的输出文件路径。
// 如果没有桌面目录，则退回用户主目录。
QString defaultDesktopFilePath(const QString &fileName)
{
    QString directory =
        QStandardPaths::writableLocation(QStandardPaths::DesktopLocation);
    if (directory.isEmpty()) {
        directory = QDir::homePath();
    }
    return QDir(directory).filePath(fileName);
}

} // namespace

// 生成一个可导入的 Aurora 1 GiB 测试文件。
//
// 生成位置：
//   Windows 桌面/Aurora_1GB_valid.bin
//
// 文件结构：
//   每个包 = 6 字节包头 + 1024 × 1024 字节数据段。
//   包头最后两个字节 n = AuroraDataCount。
//
// 若需要改为其它 n 值或文件大小，修改本函数上方的
// AuroraDataCount、TargetFileSize 常量即可。
//
// @param filePath 可选的输出路径；传空字符串时自动写到桌面。
// @return 文件生成并提交成功时返回 true。
bool generateAurora1GFile(const QString &filePath = QString())
{
    const QString outputPath =
        filePath.isEmpty()
            ? defaultDesktopFilePath(QStringLiteral("Aurora_1GB_valid.bin"))
            : filePath;
    const QFileInfo outputInfo(outputPath);
    if (!QDir().mkpath(outputInfo.absolutePath())) {
        qWarning().noquote()
            << QStringLiteral("无法创建 Aurora 输出目录：%1")
                   .arg(outputInfo.absolutePath());
        return false;
    }

    const qint64 payloadSize =
        static_cast<qint64>(AuroraDataCount) * BinFormat::PayloadSize;
    const qint64 packetSize = BinFormat::HeaderSize + payloadSize;
    const qint64 packetCount = TargetFileSize / packetSize;
    QByteArray header(BinFormat::HeaderSize, '\0');
    writeHeaderDataCount(header, AuroraDataCount);
    const QByteArray payload(payloadSize, '\0');

    QSaveFile file(outputPath);
    if (!file.open(QIODevice::WriteOnly)) {
        qWarning().noquote()
            << QStringLiteral("无法创建 Aurora 文件：%1")
                   .arg(file.errorString());
        return false;
    }

    // 流式逐包写入，不把 1 GiB 文件整体放入内存。
    for (qint64 packetIndex = 0;
         packetIndex < packetCount;
         ++packetIndex) {
        if (file.write(header) != header.size()
            || file.write(payload) != payload.size()) {
            qWarning().noquote()
                << QStringLiteral("写入 Aurora 文件失败：%1")
                       .arg(file.errorString());
            file.cancelWriting();
            return false;
        }
    }

    if (!file.commit()) {
        qWarning().noquote()
            << QStringLiteral("提交 Aurora 文件失败：%1")
                   .arg(file.errorString());
        return false;
    }

    qInfo().noquote()
        << QStringLiteral("Aurora 测试文件已生成：%1，共 %2 包，%3 字节")
               .arg(QDir::toNativeSeparators(outputPath))
               .arg(packetCount)
               .arg(packetCount * packetSize);
    return true;
}

// 生成一个可导入的 TCP 1 GiB 测试文件。
//
// 生成位置：
//   Windows 桌面/TCP_1GB_valid.bin
//
// 文件结构：
//   每个包 = 6 字节包头 + 1024 字节数据段。
//   包头最后两个字节 n = TcpDataCount。
//
// @param filePath 可选的输出路径；传空字符串时自动写到桌面。
// @return 文件生成并提交成功时返回 true。
bool generateTcp1GFile(const QString &filePath = QString())
{
    const QString outputPath =
        filePath.isEmpty()
            ? defaultDesktopFilePath(QStringLiteral("TCP_1GB_valid.bin"))
            : filePath;
    const QFileInfo outputInfo(outputPath);
    if (!QDir().mkpath(outputInfo.absolutePath())) {
        qWarning().noquote()
            << QStringLiteral("无法创建 TCP 输出目录：%1")
                   .arg(outputInfo.absolutePath());
        return false;
    }

    const qint64 payloadSize = TcpDataCount;
    const qint64 packetSize = BinFormat::HeaderSize + payloadSize;
    const qint64 packetCount = TargetFileSize / packetSize;
    QByteArray header(BinFormat::HeaderSize, '\0');
    writeHeaderDataCount(header, TcpDataCount);

    QByteArray packet;
    packet.reserve(packetSize);
    packet.append(header);
    packet.append(QByteArray(payloadSize, '\0'));

    // TCP 每包仅约 1 KB，若逐包 write() 会产生上百万次调用。
    // 这里预组合 1024 个包成约 1 MB 的块，大幅减少磁盘写入次数。
    constexpr int PacketsPerWriteBlock = 1024;
    QByteArray writeBlock;
    writeBlock.reserve(packetSize * PacketsPerWriteBlock);
    for (int index = 0; index < PacketsPerWriteBlock; ++index) {
        writeBlock.append(packet);
    }

    QSaveFile file(outputPath);
    if (!file.open(QIODevice::WriteOnly)) {
        qWarning().noquote()
            << QStringLiteral("无法创建 TCP 文件：%1")
                   .arg(file.errorString());
        return false;
    }

    const qint64 fullBlocks = packetCount / PacketsPerWriteBlock;
    const int remainingPackets =
        static_cast<int>(packetCount % PacketsPerWriteBlock);

    for (qint64 blockIndex = 0; blockIndex < fullBlocks; ++blockIndex) {
        if (file.write(writeBlock) != writeBlock.size()) {
            qWarning().noquote()
                << QStringLiteral("写入 TCP 文件失败：%1")
                       .arg(file.errorString());
            file.cancelWriting();
            return false;
        }
    }

    for (int packetIndex = 0;
         packetIndex < remainingPackets;
         ++packetIndex) {
        if (file.write(packet) != packet.size()) {
            qWarning().noquote()
                << QStringLiteral("写入 TCP 尾部数据失败：%1")
                       .arg(file.errorString());
            file.cancelWriting();
            return false;
        }
    }

    if (!file.commit()) {
        qWarning().noquote()
            << QStringLiteral("提交 TCP 文件失败：%1")
                   .arg(file.errorString());
        return false;
    }

    qInfo().noquote()
        << QStringLiteral("TCP 测试文件已生成：%1，共 %2 包，%3 字节")
               .arg(QDir::toNativeSeparators(outputPath))
               .arg(packetCount)
               .arg(packetCount * packetSize);
    return true;
}

// 程序入口。
// @param argc 命令行参数数量。
// @param argv 命令行参数数组，argv[1] 可传入初始 BIN 文件路径。
// @return Qt 事件循环退出码。
int main(int argc, char *argv[])
{
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    // Qt 5 需要显式开启高 DPI 支持和缩放位图，Qt 6 已默认启用这些能力。
    QApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);
#endif

    // 应用信息用于窗口标题、配置路径和任务栏显示。
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("DataSimulator"));
    QApplication::setApplicationDisplayName(QStringLiteral("DataSimulator"));
    QApplication::setOrganizationName(QStringLiteral("Aurora"));
    QApplication::setWindowIcon(QIcon(QStringLiteral(":/icons/app.svg")));

    // 从 Qt 资源系统加载全局样式；资源不存在时使用 Qt 默认样式继续运行。
    QFile styleFile(QStringLiteral(":/styles/app.qss"));
    if (styleFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        app.setStyleSheet(QString::fromUtf8(styleFile.readAll()));
    }

    // =========================================================================
    // 1 GiB 测试文件快捷调用位置
    //
    // 需要生成时，取消下面对应注释后运行一次程序：
//       generateAurora1GFile();
//       generateTcp1GFile();
    //
    // 生成路径默认为桌面：
    //   Aurora_1GB_valid.bin
    //   TCP_1GB_valid.bin
    // 也可以传入自定义绝对路径：
    //   generateAurora1GFile("D:/temp/Aurora.bin");
    //   generateTcp1GFile("D:/temp/TCP.bin");
    // =========================================================================
    // generateAurora1GFile();
    // generateTcp1GFile();

    MainWindow window;
    window.show();

    // 支持类似 "DataSimulator.exe sample.bin" 的启动方式。
    // 使用 singleShot(0) 是为了等待窗口进入事件循环后再执行文件选择逻辑。
    const QStringList arguments = app.arguments();
    if (arguments.size() > 1) {
        const QString initialFile = arguments.at(1);
        QTimer::singleShot(0, &window, [&window, initialFile]() {
            window.loadFile(initialFile);
        });
    }

    return app.exec();
}
