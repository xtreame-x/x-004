#pragma once
#include <windows.h>
#include <string>

struct SystemMetrics {
    float cpuUsage = 0.0f;           // 0.0 to 100.0%
    float ramUsagePercent = 0.0f;    // 0.0 to 100.0%
    float ramUsedGB = 0.0f;          // Used RAM in Gigabytes
    float ramTotalGB = 0.0f;         // Total RAM in Gigabytes
    bool hasBattery = false;
    int batteryPercent = 100;
    bool isCharging = false;
    bool isACConnected = true;
    std::wstring uptimeString;
};

class SystemStatsCollector {
public:
    SystemStatsCollector();
    SystemMetrics Update();

private:
    FILETIME m_prevIdleTime;
    FILETIME m_prevKernelTime;
    FILETIME m_prevUserTime;
    bool m_firstRun;

    ULONGLONG FileTimeToUint64(const FILETIME& ft);
};
