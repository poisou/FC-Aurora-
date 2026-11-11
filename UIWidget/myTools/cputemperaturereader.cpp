#include "cputemperaturereader.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QThread>
#include <QtGlobal>

#ifdef Q_OS_WIN
#include <windows.h>
#include <shellapi.h>

#include <cstring>

namespace
{
const int coreTempRetryDelayMs = 30000;

// 官方 CoreTempSharedData 使用 4 字节对齐；这里只声明读取温度所需的前半部分。
#pragma pack(push, 4)
struct CoreTempSharedDataV1
{
    quint32 uiLoad[256];
    quint32 uiTjMax[128];
    quint32 uiCoreCnt;
    quint32 uiCPUCnt;
    float fTemp[256];
    float fVID;
    float fCPUSpeed;
    float fFSBSpeed;
    float fMultiplier;
    char sCPUName[100];
    quint8 ucFahrenheit;
    quint8 ucDeltaToTjMax;
};
#pragma pack(pop)

// 编译期校验结构布局，避免共享内存字段错位导致温度错误。
static_assert(sizeof(CoreTempSharedDataV1) == 2688, "Unexpected Core Temp data layout");

// 打开 Core Temp 创建的内存映射，并复制一份只读快照。
bool openCoreTempData(CoreTempSharedDataV1 *data, DWORD *errorCode)
{
    // 新版本优先使用 Ex 映射，同时兼容旧版 Core Temp。
    HANDLE mapping = OpenFileMappingW(
        FILE_MAP_READ,
        FALSE,
        L"CoreTempMappingObjectEx");

    if (!mapping) {
        mapping = OpenFileMappingW(
            FILE_MAP_READ,
            FALSE,
            L"CoreTempMappingObject");
    }

    if (!mapping) {
        if (errorCode) {
            *errorCode = GetLastError();
        }
        return false;
    }

    const void *view = MapViewOfFile(
        mapping,
        FILE_MAP_READ,
        0,
        0,
        sizeof(CoreTempSharedDataV1));

    if (!view) {
        if (errorCode) {
            *errorCode = GetLastError();
        }
        CloseHandle(mapping);
        return false;
    }

    // 复制共享内存后立即释放映射，避免长时间占用 Core Temp 的内存区域。
    std::memset(data, 0, sizeof(CoreTempSharedDataV1));
    std::memcpy(data, view, sizeof(CoreTempSharedDataV1));

    UnmapViewOfFile(view);
    CloseHandle(mapping);
    return true;
}

// 从程序所在目录启动 Core Temp，并使用 Windows runas 请求管理员权限。
bool launchCoreTempAsAdministrator(QString *error)
{
    const QString applicationDirectory = QCoreApplication::applicationDirPath();
    const QString coreTempPath =
        QDir(applicationDirectory).filePath(QStringLiteral("Core Temp.exe"));

    if (!QFileInfo::exists(coreTempPath)) {
        if (error) {
            *error = QStringLiteral("\u672A\u627E\u5230 Core Temp.exe\uFF0C\u8BF7\u786E\u8BA4\u5B83\u4E0E\u672C\u7A0B\u5E8F\u4F4D\u4E8E\u540C\u4E00\u76EE\u5F55");
        }
        return false;
    }

    // lpVerb=runas 会触发 UAC；SEE_MASK_NOASYNC 保证调用完成后再继续等待共享内存。
    SHELLEXECUTEINFOW executeInfo;
    std::memset(&executeInfo, 0, sizeof(executeInfo));
    executeInfo.cbSize = sizeof(executeInfo);
    executeInfo.fMask = SEE_MASK_NOCLOSEPROCESS | SEE_MASK_NOASYNC;
    executeInfo.lpVerb = L"runas";
    executeInfo.lpFile = reinterpret_cast<const wchar_t *>(coreTempPath.utf16());
    executeInfo.lpDirectory =
        reinterpret_cast<const wchar_t *>(applicationDirectory.utf16());
    executeInfo.nShow = SW_SHOWNORMAL;

    if (!ShellExecuteExW(&executeInfo)) {
        if (error) {
            *error = QStringLiteral("\u65E0\u6CD5\u4EE5\u7BA1\u7406\u5458\u8EAB\u4EFD\u542F\u52A8 Core Temp\uFF08\u9519\u8BEF %1\uFF09")
                         .arg(GetLastError());
        }
        return false;
    }

    if (executeInfo.hProcess) {
        CloseHandle(executeInfo.hProcess);
    }
    return true;
}
} // namespace
#endif

TemperatureReading CpuTemperatureReader::readAverageTemperature()
{
    // 每次调用都会重新读取共享内存，保证界面拿到的是最新温度。
    TemperatureReading reading;

#ifdef Q_OS_WIN
    CoreTempSharedDataV1 data;
    DWORD openError = ERROR_SUCCESS;
    bool dataAvailable = openCoreTempData(&data, &openError);

    if (dataAvailable) {
        // Core Temp 已运行时不保留冷却状态，关闭后可立即再次自动启动。
        m_coreTempRetryTimer.invalidate();
    }

    // Core Temp 未运行或中途退出时自动启动，并保留失败重试。
    const bool retryDelayElapsed =
        !m_coreTempRetryTimer.isValid() ||
        m_coreTempRetryTimer.elapsed() >= coreTempRetryDelayMs;
    if (!dataAvailable && retryDelayElapsed) {
        QString launchError;
        if (!launchCoreTempAsAdministrator(&launchError)) {
            m_coreTempRetryTimer.restart();
            reading.error = launchError;
            return reading;
        }

        // 最多等待 3 秒，给管理员授权和 Core Temp 初始化留出时间。
        for (int attempt = 0; attempt < 12 && !dataAvailable; ++attempt) {
            QThread::msleep(250);
            dataAvailable = openCoreTempData(&data, &openError);
        }

        if (!dataAvailable) {
            m_coreTempRetryTimer.restart();
        }
    }

    if (!dataAvailable) {
        reading.error = QStringLiteral(
            "Core Temp \u5171\u4EAB\u5185\u5B58\u4E0D\u53EF\u7528\uFF08\u9519\u8BEF %1\uFF09")
                            .arg(openError);
        return reading;
    }

    // Core Temp 按“每颗物理 CPU 的核心数 × 物理 CPU 数”排列 fTemp。
    const quint32 coreCount = data.uiCoreCnt;
    const quint32 cpuCount = data.uiCPUCnt;
    const quint64 totalCoreCount =
        static_cast<quint64>(coreCount) * static_cast<quint64>(cpuCount);
    if (coreCount == 0 || cpuCount == 0 || cpuCount > 128 || totalCoreCount > 256) {
        reading.error = QStringLiteral("Core Temp \u8FD4\u56DE\u7684\u6838\u5FC3\u6570\u91CF\u65E0\u6548");
        return reading;
    }

    double temperatureSum = 0.0;
    for (quint64 index = 0; index < totalCoreCount; ++index) {
        double temperature = static_cast<double>(data.fTemp[index]);
        const quint32 cpuIndex = static_cast<quint32>(index / coreCount);
        double tjMax = static_cast<double>(data.uiTjMax[cpuIndex]);

        // Core Temp 可能以华氏度上报，先统一转换为摄氏度。
        if (data.ucFahrenheit) {
            temperature = (temperature - 32.0) * 5.0 / 9.0;
            tjMax = (tjMax - 32.0) * 5.0 / 9.0;
        }

        // DeltaToTjMax 模式下，共享内存保存的是距离对应 CPU TjMax 的差值。
        if (data.ucDeltaToTjMax) {
            temperature = tjMax - temperature;
        }

        // 过滤未初始化或明显异常的数据，避免计算出误导性的平均值。
        if (temperature < -20.0 || temperature > 150.0) {
            reading.error = QStringLiteral("Core Temp \u672A\u8FD4\u56DE\u6709\u6548\u7684\u6838\u5FC3\u6E29\u5EA6");
            return reading;
        }

        temperatureSum += temperature;
    }

    reading.valid = true;
    reading.celsius = temperatureSum / static_cast<double>(totalCoreCount);
#else
    reading.error = QStringLiteral("\u5F53\u524D\u5B9E\u73B0\u4EC5\u652F\u6301 Windows");
#endif

    return reading;
}
