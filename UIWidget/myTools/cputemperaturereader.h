#pragma once

#include <QElapsedTimer>
#include <QString>

// 温度读取结果：valid 表示读取成功，error 保存失败原因。
struct TemperatureReading
{
    bool valid = false;
    double celsius = 0.0;
    QString error;
};

// 封装 Core Temp 共享内存读取、管理员启动和全部核心平均温度解析。
// MainWindow 只需调用 readAverageTemperature() 获取一次读取结果。
class CpuTemperatureReader
{
public:
    CpuTemperatureReader() = default;

    // 读取所有 CPU 核心的平均温度；失败时返回带 error 的无效结果。
    TemperatureReading readAverageTemperature();

private:
    // Core Temp 启动失败后保留冷却时间，避免连续弹出 UAC。
    QElapsedTimer m_coreTempRetryTimer;
};
