#include "widget_weather.h"
#include "../graphics/d2d_context.h"
#include "../utils/http_client.h"
#include "../utils/json_mini.h"
#include <ctime>

WidgetWeather::WidgetWeather()
    : WidgetBase(L"weather", L"WEATHER FORECAST", 280, 155) {
    // Populate popular city presets
    m_cities = {
        { L"New York", 40.7128, -74.0060 },
        { L"London", 51.5074, -0.1278 },
        { L"Tokyo", 35.6762, 139.6503 },
        { L"Paris", 48.8566, 2.3522 },
        { L"San Francisco", 37.7749, -122.4194 },
        { L"Mumbai", 19.0760, 72.8777 },
        { L"Sydney", -33.8688, 151.2093 },
        { L"Berlin", 52.5200, 13.4050 }
    };

    FetchWeather();
}

void WidgetWeather::OnUpdate() {
    time_t now = time(nullptr);
    // Refresh weather every 15 minutes (900 seconds)
    if (now - m_lastFetchTime > 900) {
        FetchWeather();
    }
}

void WidgetWeather::FetchWeather() {
    if (m_isLoading) return;
    m_isLoading = true;
    m_lastFetchTime = time(nullptr);

    const CityPreset& city = m_cities[m_currentCityIndex];
    wchar_t urlBuf[256];
    swprintf_s(urlBuf, L"https://api.open-meteo.com/v1/forecast?latitude=%.4f&longitude=%.4f&current_weather=true",
        city.lat, city.lon);

    HWND hwnd = m_hwnd;
    HttpClient::GetAsync(urlBuf, [this, hwnd](bool success, const std::string& body) {
        if (success && !body.empty()) {
            mini_json::Value root = mini_json::Value::parse(body);
            if (root.has("current_weather")) {
                const auto& cw = root["current_weather"];
                m_temperature = (float)cw["temperature"].as_number(m_temperature);
                m_windSpeed = (float)cw["windspeed"].as_number(m_windSpeed);
                m_weatherCode = cw["weathercode"].as_int(m_weatherCode);
                m_isDay = (cw["is_day"].as_int(1) == 1);
            }
        }
        m_isLoading = false;
        if (hwnd) {
            InvalidateRect(hwnd, NULL, FALSE);
        }
    });
}

std::wstring WidgetWeather::GetConditionText(int code) const {
    switch (code) {
        case 0: return m_isDay ? L"Clear Sky" : L"Clear Night";
        case 1: return L"Mainly Clear";
        case 2: return L"Partly Cloudy";
        case 3: return L"Overcast";
        case 45:
        case 48: return L"Foggy";
        case 51:
        case 53:
        case 55: return L"Drizzle";
        case 61:
        case 63: return L"Light Rain";
        case 65: return L"Heavy Rain";
        case 71:
        case 73:
        case 75: return L"Snow Fall";
        case 80:
        case 81:
        case 82: return L"Rain Showers";
        case 95:
        case 96:
        case 99: return L"Thunderstorm";
        default: return L"Fair";
    }
}

bool WidgetWeather::OnMouseDown(int x, int y, bool isRight) {
    if (!isRight) {
        // Click on city name (top-left or middle) to cycle cities
        if (y >= 28 && y <= 54 && x <= 180) {
            m_currentCityIndex = (m_currentCityIndex + 1) % m_cities.size();
            FetchWeather();
            Redraw();
            return true;
        }

        // Click on temperature area to toggle Celsius / Fahrenheit
        if (y >= 54 && y <= 110 && x <= 160) {
            m_useCelsius = !m_useCelsius;
            Redraw();
            return true;
        }
    }
    return false;
}

void WidgetWeather::OnRenderContent(ID2D1RenderTarget* rt) {
    D2DContext& ctx = D2DContext::Get();

    // 1. City Name + click hint
    std::wstring cityName = m_cities[m_currentCityIndex].name;
    D2D1_RECT_F cityRect = D2D1::RectF(16.0f, 32.0f, 180.0f, 50.0f);
    ID2D1SolidColorBrush* pCityBrush = nullptr;
    rt->CreateSolidColorBrush(D2D1::ColorF(0.35f, 0.75f, 0.98f, 0.95f), &pCityBrush);
    if (pCityBrush) {
        rt->DrawText(cityName.c_str(), (UINT32)cityName.length(), ctx.GetFormatSubtitle(), cityRect, pCityBrush);
        pCityBrush->Release();
    }

    // 2. Vector Weather Icon (Top right content area)
    D2D1_RECT_F iconRect = D2D1::RectF((float)m_width - 80.0f, 40.0f, (float)m_width - 20.0f, 95.0f);
    D2DContext::DrawWeatherVector(rt, m_weatherCode, m_isDay, iconRect);

    // 3. Temperature Text
    float displayTemp = m_useCelsius ? m_temperature : (m_temperature * 9.0f / 5.0f + 32.0f);
    wchar_t tempBuf[32];
    swprintf_s(tempBuf, L"%.0f\u00B0%s", displayTemp, m_useCelsius ? L"C" : L"F");

    D2D1_RECT_F tempRect = D2D1::RectF(14.0f, 52.0f, 150.0f, 98.0f);
    ID2D1SolidColorBrush* pTempBrush = nullptr;
    rt->CreateSolidColorBrush(D2D1::ColorF(1.0f, 1.0f, 1.0f, 0.98f), &pTempBrush);
    if (pTempBrush) {
        rt->DrawText(tempBuf, (UINT32)wcslen(tempBuf), ctx.GetFormatClock(), tempRect, pTempBrush);
        pTempBrush->Release();
    }

    // 4. Condition & Wind Speed
    std::wstring condition = GetConditionText(m_weatherCode);
    wchar_t subBuf[64];
    swprintf_s(subBuf, L"%s \u2022 Wind: %.0f km/h", condition.c_str(), m_windSpeed);

    D2D1_RECT_F subRect = D2D1::RectF(16.0f, 102.0f, (float)m_width - 16.0f, 122.0f);
    ID2D1SolidColorBrush* pSubBrush = nullptr;
    rt->CreateSolidColorBrush(D2D1::ColorF(0.72f, 0.77f, 0.88f, 0.85f), &pSubBrush);
    if (pSubBrush) {
        rt->DrawText(subBuf, (UINT32)wcslen(subBuf), ctx.GetFormatBody(), subRect, pSubBrush);
        pSubBrush->Release();
    }

    // 5. Mini footer hint
    D2D1_RECT_F hintRect = D2D1::RectF(16.0f, 126.0f, (float)m_width - 16.0f, 144.0f);
    ID2D1SolidColorBrush* pHintBrush = nullptr;
    rt->CreateSolidColorBrush(D2D1::ColorF(0.45f, 0.50f, 0.62f, 0.70f), &pHintBrush);
    if (pHintBrush) {
        const wchar_t* hint = m_isLoading ? L"Updating weather data..." : L"Click city to change \u2022 Click temp for \u00B0F";
        rt->DrawText(hint, (UINT32)wcslen(hint), ctx.GetFormatTiny(), hintRect, pHintBrush);
        pHintBrush->Release();
    }
}
