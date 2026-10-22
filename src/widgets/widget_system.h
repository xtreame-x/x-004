#pragma once
#include "widget_base.h"
#include "../utils/system_stats.h"

class WidgetSystem : public WidgetBase {
public:
    WidgetSystem();
    void OnUpdate() override;

protected:
    void OnRenderContent(ID2D1RenderTarget* rt) override;

private:
    SystemStatsCollector m_collector;
    SystemMetrics m_metrics;
};
