#include "widget_system.h"
#include "../graphics/d2d_context.h"
#include <cwchar>

WidgetSystem::WidgetSystem()
    : WidgetBase(L"system", L"SYSTEM MONITOR", 290, 205) {
    OnUpdate();
}

void WidgetSystem::OnUpdate() {
    m_metrics = m_collector.Update();
    Redraw();
}

void WidgetSystem::OnRenderContent(ID2D1RenderTarget* rt) {
    D2DContext& ctx = D2DContext::Get();

    ID2D1SolidColorBrush* pLabelBrush = nullptr;
    rt->CreateSolidColorBrush(D2D1::ColorF(0.85f, 0.88f, 0.95f, 0.95f), &pLabelBrush);

    ID2D1SolidColorBrush* pValueBrush = nullptr;
    rt->CreateSolidColorBrush(D2D1::ColorF(0.55f, 0.80f, 1.0f, 1.0f), &pValueBrush);

    ID2D1SolidColorBrush* pSubBrush = nullptr;
    rt->CreateSolidColorBrush(D2D1::ColorF(0.55f, 0.60f, 0.72f, 0.80f), &pSubBrush);

    float leftX = 16.0f;
    float rightX = (float)m_width - 16.0f;

    // --- 1. CPU Usage ---
    float curY = 34.0f;
    wchar_t cpuValBuf[32];
    swprintf_s(cpuValBuf, L"%.1f%%", m_metrics.cpuUsage);

    if (pLabelBrush) {
        D2D1_RECT_F rLabel = D2D1::RectF(leftX, curY, leftX + 80.0f, curY + 16.0f);
        rt->DrawText(L"CPU Usage", 9, ctx.GetFormatBodyBold(), rLabel, pLabelBrush);
    }
    if (pValueBrush) {
        D2D1_RECT_F rVal = D2D1::RectF(rightX - 60.0f, curY, rightX, curY + 16.0f);
        rt->DrawText(cpuValBuf, (UINT32)wcslen(cpuValBuf), ctx.GetFormatSmall(), rVal, pValueBrush);
    }

    curY += 18.0f;
    // Dynamic CPU bar color: Cyan (< 60%), Amber (60-85%), Rose (> 85%)
    D2D1_COLOR_F cpuColor = (m_metrics.cpuUsage < 60.0f) ? D2D1::ColorF(0.22f, 0.75f, 0.98f) :
                            ((m_metrics.cpuUsage < 85.0f) ? D2D1::ColorF(0.98f, 0.75f, 0.20f) : D2D1::ColorF(0.96f, 0.25f, 0.35f));
    D2DContext::DrawProgressBar(rt, D2D1::RectF(leftX, curY, rightX, curY + 7.0f), m_metrics.cpuUsage, cpuColor, D2D1::ColorF(0.18f, 0.20f, 0.28f, 0.7f));

    // --- 2. RAM Usage ---
    curY += 18.0f;
    wchar_t ramValBuf[64];
    swprintf_s(ramValBuf, L"%.1f / %.1f GB (%.0f%%)", m_metrics.ramUsedGB, m_metrics.ramTotalGB, m_metrics.ramUsagePercent);

    if (pLabelBrush) {
        D2D1_RECT_F rLabel = D2D1::RectF(leftX, curY, leftX + 80.0f, curY + 16.0f);
        rt->DrawText(L"Memory", 6, ctx.GetFormatBodyBold(), rLabel, pLabelBrush);
    }
    if (pSubBrush) {
        D2D1_RECT_F rVal = D2D1::RectF(rightX - 140.0f, curY, rightX, curY + 16.0f);
        rt->DrawText(ramValBuf, (UINT32)wcslen(ramValBuf), ctx.GetFormatSmall(), rVal, pSubBrush);
    }

    curY += 18.0f;
    D2D1_COLOR_F ramColor = D2D1::ColorF(0.66f, 0.33f, 0.97f); // Purple
    D2DContext::DrawProgressBar(rt, D2D1::RectF(leftX, curY, rightX, curY + 7.0f), m_metrics.ramUsagePercent, ramColor, D2D1::ColorF(0.18f, 0.20f, 0.28f, 0.7f));

    // --- 3. Battery / Power ---
    curY += 18.0f;
    std::wstring pwrLabel = L"Power";
    wchar_t pwrValBuf[64];

    if (m_metrics.hasBattery) {
        pwrLabel = L"Battery";
        if (m_metrics.isCharging) {
            swprintf_s(pwrValBuf, L"%d%% (Charging)", m_metrics.batteryPercent);
        } else if (m_metrics.isACConnected) {
            swprintf_s(pwrValBuf, L"%d%% (AC Plugged)", m_metrics.batteryPercent);
        } else {
            swprintf_s(pwrValBuf, L"%d%% (Discharging)", m_metrics.batteryPercent);
        }
    } else {
        swprintf_s(pwrValBuf, L"AC Connected (Desktop)");
    }

    if (pLabelBrush) {
        D2D1_RECT_F rLabel = D2D1::RectF(leftX, curY, leftX + 80.0f, curY + 16.0f);
        rt->DrawText(pwrLabel.c_str(), (UINT32)pwrLabel.length(), ctx.GetFormatBodyBold(), rLabel, pLabelBrush);
    }
    if (pSubBrush) {
        D2D1_RECT_F rVal = D2D1::RectF(rightX - 130.0f, curY, rightX, curY + 16.0f);
        rt->DrawText(pwrValBuf, (UINT32)wcslen(pwrValBuf), ctx.GetFormatSmall(), rVal, pSubBrush);
    }

    curY += 18.0f;
    float batPercent = m_metrics.hasBattery ? (float)m_metrics.batteryPercent : 100.0f;
    D2D1_COLOR_F batColor = D2D1::ColorF(0.20f, 0.82f, 0.55f); // Emerald
    D2DContext::DrawProgressBar(rt, D2D1::RectF(leftX, curY, rightX - 32.0f, curY + 7.0f), batPercent, batColor, D2D1::ColorF(0.18f, 0.20f, 0.28f, 0.7f));

    // Mini battery icon next to progress bar
    D2DContext::DrawBatteryVector(rt, D2D1::RectF(rightX - 26.0f, curY - 3.0f, rightX, curY + 10.0f), (int)batPercent, m_metrics.isCharging, m_metrics.isACConnected);

    // --- 4. System Uptime footer ---
    curY += 20.0f;
    if (pSubBrush) {
        D2D1_RECT_F rUptime = D2D1::RectF(leftX, curY, rightX, curY + 16.0f);
        rt->DrawText(m_metrics.uptimeString.c_str(), (UINT32)m_metrics.uptimeString.length(), ctx.GetFormatTiny(), rUptime, pSubBrush);
    }

    if (pLabelBrush) pLabelBrush->Release();
    if (pValueBrush) pValueBrush->Release();
    if (pSubBrush) pSubBrush->Release();
}
