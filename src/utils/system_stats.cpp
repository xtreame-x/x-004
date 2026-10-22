#include "system_stats.h"
#include <cwchar>

SystemStatsCollector::SystemStatsCollector()
    : m_firstRun(true) {
    m_prevIdleTime = { 0, 0 };
    m_prevKernelTime = { 0, 0 };
    m_prevUserTime = { 0, 0 };
}

ULONGLONG SystemStatsCollector::FileTimeToUint64(const FILETIME& ft) {
    return (((ULONGLONG)ft.dwHighDateTime) << 32) | ((ULONGLONG)ft.dwLowDateTime);
}

SystemMetrics SystemStatsCollector::Update() {
    SystemMetrics metrics;

    // 1. CPU Usage Calculation
    FILETIME idleTime, kernelTime, userTime;
    if (GetSystemTimes(&idleTime, &kernelTime, &userTime)) {
        if (!m_firstRun) {
            ULONGLONG idle = FileTimeToUint64(idleTime) - FileTimeToUint64(m_prevIdleTime);
            ULONGLONG kernel = FileTimeToUint64(kernelTime) - FileTimeToUint64(m_prevKernelTime);
            ULONGLONG user = FileTimeToUint64(userTime) - FileTimeToUint64(m_prevUserTime);
            ULONGLONG total = kernel + user;

            if (total > 0) {
                double cpu = (1.0 - (double)idle / (double)total) * 100.0;
                if (cpu < 0.0) cpu = 0.0;
                if (cpu > 100.0) cpu = 100.0;
                metrics.cpuUsage = (float)cpu;
            }
        }
        m_prevIdleTime = idleTime;
        m_prevKernelTime = kernelTime;
        m_prevUserTime = userTime;
        m_firstRun = false;
    }

    // 2. RAM Usage Calculation
    MEMORYSTATUSEX memStatus;
    memStatus.dwLength = sizeof(MEMORYSTATUSEX);
    if (GlobalMemoryStatusEx(&memStatus)) {
        metrics.ramUsagePercent = (float)memStatus.dwMemoryLoad;
        double totalBytes = (double)memStatus.ullTotalPhys;
        double availBytes = (double)memStatus.ullAvailPhys;
        double usedBytes = totalBytes - availBytes;

        const double GB = 1024.0 * 1024.0 * 1024.0;
        metrics.ramTotalGB = (float)(totalBytes / GB);
        metrics.ramUsedGB = (float)(usedBytes / GB);
    }

    // 3. Battery & Power Status
    SYSTEM_POWER_STATUS sps;
    if (GetSystemPowerStatus(&sps)) {
        // BatteryFlag 128 means no system battery
        metrics.hasBattery = (sps.BatteryFlag != 128 && sps.BatteryLifePercent != 255);
        metrics.batteryPercent = (sps.BatteryLifePercent <= 100) ? sps.BatteryLifePercent : 100;
        metrics.isCharging = (sps.BatteryFlag & 8) != 0;
        metrics.isACConnected = (sps.ACLineStatus == 1);
    }

    // 4. System Uptime
    ULONGLONG uptimeMs = GetTickCount64();
    ULONGLONG totalSeconds = uptimeMs / 1000;
    ULONGLONG days = totalSeconds / 86400;
    ULONGLONG hours = (totalSeconds % 86400) / 3600;
    ULONGLONG minutes = (totalSeconds % 3600) / 60;

    wchar_t buf[128];
    if (days > 0) {
        swprintf_s(buf, L"Uptime: %llud %lluh %llum", days, hours, minutes);
    } else {
        swprintf_s(buf, L"Uptime: %lluh %llum", hours, minutes);
    }
    metrics.uptimeString = buf;

    return metrics;
}
