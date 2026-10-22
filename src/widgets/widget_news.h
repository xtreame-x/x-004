#pragma once
#include "widget_base.h"
#include "../utils/xml_mini.h"
#include <vector>

class WidgetNews : public WidgetBase {
public:
    WidgetNews();
    void OnUpdate() override;

protected:
    void OnRenderContent(ID2D1RenderTarget* rt) override;
    bool OnMouseDown(int x, int y, bool isRight) override;
    void OnMouseMove(int x, int y) override;
    void OnMouseLeave() override;

private:
    void FetchNews();
    void NextArticle();
    void PrevArticle();

    std::vector<NewsArticle> m_articles;
    size_t m_currentIndex = 0;
    bool m_isLoading = false;
    time_t m_lastFetchTime = 0;
    time_t m_lastAutoRotateTime = 0;

    bool m_hoverPrev = false;
    bool m_hoverNext = false;
    bool m_hoverRefresh = false;
    bool m_hoverCard = false;

    D2D1_RECT_F GetCardRect() const;
    D2D1_RECT_F GetPrevRect() const;
    D2D1_RECT_F GetNextRect() const;
    D2D1_RECT_F GetRefreshRect() const;
};
