#pragma once
#include "widget_base.h"
#include <vector>

struct CityPreset {
    std::wstring name;
    double lat;
    double lon;
};

class WidgetWeather : public WidgetBase {
public:
    WidgetWeather();
    void OnUpdate() override;

protected:
    void OnRenderContent(ID2D1RenderTarget* rt) override;
    bool OnMouseDown(int x, int y, bool isRight) override;

private:
    void FetchWeather();
    std::wstring GetConditionText(int code) const;

    std::vector<CityPreset> m_cities;
    size_t m_currentCityIndex = 0;

    bool m_useCelsius = true;
    float m_temperature = 22.0f;
    float m_windSpeed = 12.0f;
    int m_weatherCode = 1;
    bool m_isDay = true;
    bool m_isLoading = false;
    time_t m_lastFetchTime = 0;
};
