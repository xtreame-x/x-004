#include "widget_news.h"
#include "../graphics/d2d_context.h"
#include "../utils/http_client.h"
#include <shellapi.h>
#include <ctime>

WidgetNews::WidgetNews()
    : WidgetBase(L"news", L"WORLD NEWS & HEADLINES", 340, 205) {
    // Initial sample fallback articles
    m_articles = {
        {
            L"Global Tech Summit reveals breakthroughs in AI and Quantum Computing",
            L"https://news.google.com",
            L"Tech News",
            L"Today"
        },
        {
            L"Space agency confirms historic launch of next-generation deep space observatory",
            L"https://news.google.com",
            L"Science",
            L"Today"
        },
        {
            L"Renewable energy investments reach record milestone across global grids",
            L"https://news.google.com",
            L"Environment",
            L"Today"
        }
    };

    m_lastAutoRotateTime = time(nullptr);
    FetchNews();
}

void WidgetNews::OnUpdate() {
    time_t now = time(nullptr);

    // Auto rotate article every 18 seconds
    if (now - m_lastAutoRotateTime > 18) {
        m_lastAutoRotateTime = now;
        NextArticle();
    }

    // Refresh feed every 30 minutes
    if (now - m_lastFetchTime > 1800) {
        FetchNews();
    }
}

void WidgetNews::FetchNews() {
    if (m_isLoading) return;
    m_isLoading = true;
    m_lastFetchTime = time(nullptr);

    HWND hwnd = m_hwnd;
    HttpClient::GetAsync(L"https://feeds.bbci.co.uk/news/world/rss.xml", [this, hwnd](bool success, const std::string& body) {
        if (success && !body.empty()) {
            auto list = XmlRssParser::ParseRss(body, L"BBC World News");
            if (!list.empty()) {
                m_articles = list;
                if (m_currentIndex >= m_articles.size()) {
                    m_currentIndex = 0;
                }
            }
        }
        m_isLoading = false;
        if (hwnd) {
            InvalidateRect(hwnd, NULL, FALSE);
        }
    });
}

void WidgetNews::NextArticle() {
    if (!m_articles.empty()) {
        m_currentIndex = (m_currentIndex + 1) % m_articles.size();
        Redraw();
    }
}

void WidgetNews::PrevArticle() {
    if (!m_articles.empty()) {
        if (m_currentIndex == 0) {
            m_currentIndex = m_articles.size() - 1;
        } else {
            m_currentIndex--;
        }
        Redraw();
    }
}

D2D1_RECT_F WidgetNews::GetCardRect() const {
    return D2D1::RectF(14.0f, 54.0f, (float)m_width - 14.0f, (float)m_height - 38.0f);
}

D2D1_RECT_F WidgetNews::GetPrevRect() const {
    return D2D1::RectF((float)m_width - 76.0f, 32.0f, (float)m_width - 56.0f, 48.0f);
}

D2D1_RECT_F WidgetNews::GetNextRect() const {
    return D2D1::RectF((float)m_width - 34.0f, 32.0f, (float)m_width - 14.0f, 48.0f);
}

D2D1_RECT_F WidgetNews::GetRefreshRect() const {
    return D2D1::RectF((float)m_width - 56.0f, 32.0f, (float)m_width - 36.0f, 48.0f);
}

void WidgetNews::OnMouseMove(int x, int y) {
    D2D1_RECT_F cardR = GetCardRect();
    D2D1_RECT_F prevR = GetPrevRect();
    D2D1_RECT_F nextR = GetNextRect();
    D2D1_RECT_F refR = GetRefreshRect();

    bool prevH = (x >= prevR.left && x <= prevR.right && y >= prevR.top && y <= prevR.bottom);
    bool nextH = (x >= nextR.left && x <= nextR.right && y >= nextR.top && y <= nextR.bottom);
    bool refH = (x >= refR.left && x <= refR.right && y >= refR.top && y <= refR.bottom);
    bool cardH = (x >= cardR.left && x <= cardR.right && y >= cardR.top && y <= cardR.bottom);

    if (prevH != m_hoverPrev || nextH != m_hoverNext || refH != m_hoverRefresh || cardH != m_hoverCard) {
        m_hoverPrev = prevH;
        m_hoverNext = nextH;
        m_hoverRefresh = refH;
        m_hoverCard = cardH;
        Redraw();
    }
}

void WidgetNews::OnMouseLeave() {
    if (m_hoverPrev || m_hoverNext || m_hoverRefresh || m_hoverCard) {
        m_hoverPrev = false;
        m_hoverNext = false;
        m_hoverRefresh = false;
        m_hoverCard = false;
        Redraw();
    }
}

bool WidgetNews::OnMouseDown(int x, int y, bool isRight) {
    if (isRight) return false;

    D2D1_RECT_F prevR = GetPrevRect();
    D2D1_RECT_F nextR = GetNextRect();
    D2D1_RECT_F refR = GetRefreshRect();
    D2D1_RECT_F cardR = GetCardRect();

    if (x >= prevR.left && x <= prevR.right && y >= prevR.top && y <= prevR.bottom) {
        m_lastAutoRotateTime = time(nullptr);
        PrevArticle();
        return true;
    }

    if (x >= nextR.left && x <= nextR.right && y >= nextR.top && y <= nextR.bottom) {
        m_lastAutoRotateTime = time(nullptr);
        NextArticle();
        return true;
    }

    if (x >= refR.left && x <= refR.right && y >= refR.top && y <= refR.bottom) {
        FetchNews();
        return true;
    }

    // Click on article card -> open URL in default browser
    if (x >= cardR.left && x <= cardR.right && y >= cardR.top && y <= cardR.bottom) {
        if (!m_articles.empty() && !m_articles[m_currentIndex].link.empty()) {
            ShellExecuteW(NULL, L"open", m_articles[m_currentIndex].link.c_str(), NULL, NULL, SW_SHOWNORMAL);
            return true;
        }
    }

    return false;
}

void WidgetNews::OnRenderContent(ID2D1RenderTarget* rt) {
    D2DContext& ctx = D2DContext::Get();

    // 1. Source / Counter header
    wchar_t headerBuf[64];
    if (!m_articles.empty()) {
        swprintf_s(headerBuf, L"%s (%zu / %zu)",
            m_articles[m_currentIndex].source.c_str(),
            m_currentIndex + 1,
            m_articles.size());
    } else {
        swprintf_s(headerBuf, L"News Feed");
    }

    D2D1_RECT_F sourceRect = D2D1::RectF(14.0f, 32.0f, (float)m_width - 85.0f, 50.0f);
    ID2D1SolidColorBrush* pSrcBrush = nullptr;
    rt->CreateSolidColorBrush(D2D1::ColorF(0.98f, 0.55f, 0.25f, 0.95f), &pSrcBrush); // Amber/Flame
    if (pSrcBrush) {
        rt->DrawText(headerBuf, (UINT32)wcslen(headerBuf), ctx.GetFormatSubtitle(), sourceRect, pSrcBrush);
        pSrcBrush->Release();
    }

    // Navigation buttons: ◀ ↻ ▶
    D2DContext::DrawNavArrow(rt, GetPrevRect(), false, m_hoverPrev);
    D2DContext::DrawNavArrow(rt, GetNextRect(), true, m_hoverNext);

    // Refresh icon (↻)
    D2D1_RECT_F refR = GetRefreshRect();
    float rcx = (refR.left + refR.right) / 2.0f;
    float rcy = (refR.top + refR.bottom) / 2.0f;
    ID2D1SolidColorBrush* pRefBrush = nullptr;
    rt->CreateSolidColorBrush(m_hoverRefresh ? D2D1::ColorF(1.0f, 1.0f, 1.0f, 0.95f) : D2D1::ColorF(0.6f, 0.65f, 0.75f, 0.7f), &pRefBrush);
    if (pRefBrush) {
        rt->DrawEllipse(D2D1::Ellipse(D2D1::Point2F(rcx, rcy), 4.5f, 4.5f), pRefBrush, 1.4f);
        rt->DrawLine(D2D1::Point2F(rcx + 3.0f, rcy - 5.0f), D2D1::Point2F(rcx + 5.0f, rcy - 1.0f), pRefBrush, 1.4f);
        pRefBrush->Release();
    }

    // 2. Article Card
    D2D1_RECT_F cardR = GetCardRect();
    D2DContext::DrawInnerCard(rt, cardR, 8.0f, m_hoverCard);

    if (!m_articles.empty()) {
        const auto& art = m_articles[m_currentIndex];

        // Headline text
        D2D1_RECT_F titleRect = D2D1::RectF(cardR.left + 12.0f, cardR.top + 10.0f, cardR.right - 12.0f, cardR.bottom - 22.0f);
        ID2D1SolidColorBrush* pTitleBrush = nullptr;
        rt->CreateSolidColorBrush(m_hoverCard ? D2D1::ColorF(0.35f, 0.75f, 0.98f, 0.98f) : D2D1::ColorF(0.95f, 0.96f, 0.98f, 0.95f), &pTitleBrush);
        if (pTitleBrush) {
            rt->DrawText(art.title.c_str(), (UINT32)art.title.length(), ctx.GetFormatBodyBold(), titleRect, pTitleBrush);
            pTitleBrush->Release();
        }

        // Action hint footer inside card
        D2D1_RECT_F linkHintRect = D2D1::RectF(cardR.left + 12.0f, cardR.bottom - 22.0f, cardR.right - 12.0f, cardR.bottom - 6.0f);
        ID2D1SolidColorBrush* pHintBrush = nullptr;
        rt->CreateSolidColorBrush(D2D1::ColorF(0.50f, 0.55f, 0.68f, 0.75f), &pHintBrush);
        if (pHintBrush) {
            rt->DrawText(L"Click to read in browser \u2197", 26, ctx.GetFormatTiny(), linkHintRect, pHintBrush);
            pHintBrush->Release();
        }
    }

    // Bottom status hint
    D2D1_RECT_F statusRect = D2D1::RectF(14.0f, (float)m_height - 24.0f, (float)m_width - 14.0f, (float)m_height - 6.0f);
    ID2D1SolidColorBrush* pStatusBrush = nullptr;
    rt->CreateSolidColorBrush(D2D1::ColorF(0.40f, 0.45f, 0.55f, 0.65f), &pStatusBrush);
    if (pStatusBrush) {
        const wchar_t* msg = m_isLoading ? L"Fetching latest updates..." : L"Auto-updates every 18 seconds";
        rt->DrawText(msg, (UINT32)wcslen(msg), ctx.GetFormatTiny(), statusRect, pStatusBrush);
        pStatusBrush->Release();
    }
}
