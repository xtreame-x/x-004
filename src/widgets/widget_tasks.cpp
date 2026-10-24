#include "widget_tasks.h"
#include "../graphics/d2d_context.h"
#include "../utils/json_mini.h"
#include "../utils/http_client.h"
#include <fstream>
#include <sstream>

WidgetTasks::WidgetTasks()
    : WidgetBase(L"tasks", L"TASKS & CHECKLIST", 310, 280) {
    LoadTasks();
}

void WidgetTasks::OnUpdate() {
    DWORD now = GetTickCount();
    if (now - m_lastBlinkTime > 500) {
        m_cursorVisible = !m_cursorVisible;
        m_lastBlinkTime = now;
        if (m_inputFocused) {
            Redraw();
        }
    }
}

void WidgetTasks::LoadTasks() {
    m_tasks.clear();
    std::ifstream file("tasks.json");
    if (file.is_open()) {
        std::stringstream buffer;
        buffer << file.rdbuf();
        file.close();

        mini_json::Value root = mini_json::Value::parse(buffer.str());
        if (root.is_array()) {
            for (size_t i = 0; i < root.arr_val.size(); i++) {
                TaskItem item;
                item.text = HttpClient::Utf8ToWide(root[i]["text"].as_string());
                item.completed = root[i]["completed"].as_bool();
                if (!item.text.empty()) {
                    m_tasks.push_back(item);
                }
            }
        }
    }

    if (m_tasks.empty()) {
        m_tasks.push_back({ L"Welcome to WinWidgets!", false });
        m_tasks.push_back({ L"Drag widgets around your screen", true });
        m_tasks.push_back({ L"Right-click widget for options & pin", false });
        SaveTasks();
    }
}

void WidgetTasks::SaveTasks() {
    mini_json::Value arr;
    for (const auto& item : m_tasks) {
        mini_json::Value obj;
        obj["text"] = HttpClient::WideToUtf8(item.text);
        obj["completed"] = item.completed;
        arr.push_back(obj);
    }

    std::ofstream file("tasks.json");
    if (file.is_open()) {
        file << arr.serialize(0);
        file.close();
    }
}

void WidgetTasks::AddTask(const std::wstring& text) {
    if (text.empty()) return;
    m_tasks.push_back({ text, false });
    SaveTasks();
    Redraw();
}

void WidgetTasks::DeleteTask(size_t index) {
    if (index < m_tasks.size()) {
        m_tasks.erase(m_tasks.begin() + index);
        SaveTasks();
        Redraw();
    }
}

void WidgetTasks::ToggleTask(size_t index) {
    if (index < m_tasks.size()) {
        m_tasks[index].completed = !m_tasks[index].completed;
        SaveTasks();
        Redraw();
    }
}

D2D1_RECT_F WidgetTasks::GetInputRect() const {
    return D2D1::RectF(14.0f, (float)m_height - 42.0f, (float)m_width - 50.0f, (float)m_height - 12.0f);
}

D2D1_RECT_F WidgetTasks::GetAddButtonRect() const {
    return D2D1::RectF((float)m_width - 44.0f, (float)m_height - 42.0f, (float)m_width - 14.0f, (float)m_height - 12.0f);
}

void WidgetTasks::OnMouseMove(int x, int y) {
    float startY = 56.0f;
    float rowH = 34.0f;
    size_t maxItems = 5;
    size_t count = (m_tasks.size() < maxItems) ? m_tasks.size() : maxItems;

    int oldHover = m_hoverIndex;
    int oldDel = m_hoverDeleteIndex;
    m_hoverIndex = -1;
    m_hoverDeleteIndex = -1;

    for (size_t i = 0; i < count; i++) {
        float rowTop = startY + i * rowH;
        float rowBottom = rowTop + rowH - 4.0f;

        if (y >= rowTop && y <= rowBottom && x >= 14 && x <= m_width - 14) {
            m_hoverIndex = (int)i;
            if (x >= m_width - 42 && x <= m_width - 18) {
                m_hoverDeleteIndex = (int)i;
            }
            break;
        }
    }

    if (oldHover != m_hoverIndex || oldDel != m_hoverDeleteIndex) {
        Redraw();
    }
}

void WidgetTasks::OnMouseLeave() {
    if (m_hoverIndex != -1 || m_hoverDeleteIndex != -1) {
        m_hoverIndex = -1;
        m_hoverDeleteIndex = -1;
        Redraw();
    }
}

bool WidgetTasks::OnMouseDown(int x, int y, bool isRight) {
    if (isRight) return false;

    // Check click on Add button [+]
    D2D1_RECT_F addR = GetAddButtonRect();
    if (x >= addR.left && x <= addR.right && y >= addR.top && y <= addR.bottom) {
        if (!m_inputText.empty()) {
            AddTask(m_inputText);
            m_inputText.clear();
        }
        m_inputFocused = false;
        SetKeyboardFocusRequired(false);
        return true;
    }

    // Check click on Input field
    D2D1_RECT_F inR = GetInputRect();
    if (x >= inR.left && x <= inR.right && y >= inR.top && y <= inR.bottom) {
        m_inputFocused = true;
        SetKeyboardFocusRequired(true);
        SetFocus(m_hwnd);
        Redraw();
        return true;
    } else {
        if (m_inputFocused) {
            m_inputFocused = false;
            SetKeyboardFocusRequired(false);
            Redraw();
        }
    }

    // Check task rows
    float startY = 56.0f;
    float rowH = 34.0f;
    size_t maxItems = 5;
    size_t count = (m_tasks.size() < maxItems) ? m_tasks.size() : maxItems;

    for (size_t i = 0; i < count; i++) {
        float rowTop = startY + i * rowH;
        float rowBottom = rowTop + rowH - 4.0f;

        if (y >= rowTop && y <= rowBottom && x >= 14 && x <= m_width - 14) {
            // Delete button on the right
            if (x >= m_width - 42 && x <= m_width - 18) {
                DeleteTask(i);
                return true;
            }

            // Task checkbox or row click
            ToggleTask(i);
            return true;
        }
    }

    return false;
}

void WidgetTasks::OnCharInput(wchar_t ch) {
    if (!m_inputFocused) return;

    if (ch == VK_BACK) {
        if (!m_inputText.empty()) {
            m_inputText.pop_back();
            Redraw();
        }
    } else if (ch == VK_RETURN) {
        if (!m_inputText.empty()) {
            AddTask(m_inputText);
            m_inputText.clear();
        }
        m_inputFocused = false;
        SetKeyboardFocusRequired(false);
        Redraw();
    } else if (ch >= 32) {
        if (m_inputText.length() < 50) {
            m_inputText.push_back(ch);
            Redraw();
        }
    }
}

void WidgetTasks::OnKeyDown(WPARAM key) {
    if (!m_inputFocused) return;
    if (key == VK_ESCAPE) {
        m_inputFocused = false;
        SetKeyboardFocusRequired(false);
        Redraw();
    }
}

void WidgetTasks::OnRenderContent(ID2D1RenderTarget* rt) {
    D2DContext& ctx = D2DContext::Get();

    // 1. Task progress badge in header
    size_t completedCount = 0;
    for (const auto& item : m_tasks) {
        if (item.completed) completedCount++;
    }

    wchar_t countBuf[32];
    swprintf_s(countBuf, L"%zu of %zu done", completedCount, m_tasks.size());
    D2D1_RECT_F countRect = D2D1::RectF(14.0f, 32.0f, 200.0f, 50.0f);
    ID2D1SolidColorBrush* pCountBrush = nullptr;
    rt->CreateSolidColorBrush(D2D1::ColorF(0.35f, 0.75f, 0.98f, 0.90f), &pCountBrush);
    if (pCountBrush) {
        rt->DrawText(countBuf, (UINT32)wcslen(countBuf), ctx.GetFormatSubtitle(), countRect, pCountBrush);
        pCountBrush->Release();
    }

    // 2. Render Task Rows
    float startY = 56.0f;
    float rowH = 34.0f;
    size_t maxItems = 5;
    size_t count = (m_tasks.size() < maxItems) ? m_tasks.size() : maxItems;

    ID2D1SolidColorBrush* pTextBrush = nullptr;
    rt->CreateSolidColorBrush(D2D1::ColorF(0.92f, 0.94f, 0.98f, 0.95f), &pTextBrush);

    ID2D1SolidColorBrush* pDoneBrush = nullptr;
    rt->CreateSolidColorBrush(D2D1::ColorF(0.48f, 0.52f, 0.62f, 0.60f), &pDoneBrush);

    ID2D1SolidColorBrush* pDelBrush = nullptr;
    rt->CreateSolidColorBrush(D2D1::ColorF(0.85f, 0.35f, 0.40f, 0.85f), &pDelBrush);

    for (size_t i = 0; i < count; i++) {
        float rowTop = startY + i * rowH;
        float rowBottom = rowTop + rowH - 4.0f;
        D2D1_RECT_F rowRect = D2D1::RectF(14.0f, rowTop, (float)m_width - 14.0f, rowBottom);

        bool isHovered = (m_hoverIndex == (int)i);
        D2DContext::DrawInnerCard(rt, rowRect, 6.0f, isHovered);

        // Checkbox circle
        D2D1_RECT_F checkRect = D2D1::RectF(22.0f, rowTop + 5.0f, 38.0f, rowTop + 21.0f);
        D2DContext::DrawCheckbox(rt, checkRect, m_tasks[i].completed, isHovered);

        // Task text
        D2D1_RECT_F textRect = D2D1::RectF(46.0f, rowTop + 6.0f, (float)m_width - 48.0f, rowBottom);
        ID2D1SolidColorBrush* useBrush = m_tasks[i].completed ? pDoneBrush : pTextBrush;
        if (useBrush) {
            rt->DrawText(
                m_tasks[i].text.c_str(),
                (UINT32)m_tasks[i].text.length(),
                ctx.GetFormatBody(),
                textRect,
                useBrush
            );
        }

        // Strikethrough line if completed
        if (m_tasks[i].completed && pDoneBrush) {
            float textW = (float)m_tasks[i].text.length() * 6.5f;
            if (textW > (float)m_width - 100.0f) textW = (float)m_width - 100.0f;
            float lineY = rowTop + 14.0f;
            rt->DrawLine(D2D1::Point2F(46.0f, lineY), D2D1::Point2F(46.0f + textW, lineY), pDoneBrush, 1.2f);
        }

        // Delete button (✕)
        if (isHovered) {
            D2D1_RECT_F delRect = D2D1::RectF((float)m_width - 38.0f, rowTop + 6.0f, (float)m_width - 22.0f, rowBottom - 6.0f);
            bool isDelHover = (m_hoverDeleteIndex == (int)i);
            D2DContext::DrawCloseButton(rt, delRect, isDelHover);
        }
    }

    if (pTextBrush) pTextBrush->Release();
    if (pDoneBrush) pDoneBrush->Release();
    if (pDelBrush) pDelBrush->Release();

    // 3. Input Row at Bottom
    D2D1_RECT_F inRect = GetInputRect();
    D2D1_ROUNDED_RECT inRound = D2D1::RoundedRect(inRect, 6.0f, 6.0f);

    ID2D1SolidColorBrush* pInBg = nullptr;
    rt->CreateSolidColorBrush(m_inputFocused ? D2D1::ColorF(0.18f, 0.22f, 0.32f, 0.85f) : D2D1::ColorF(0.12f, 0.14f, 0.20f, 0.65f), &pInBg);
    if (pInBg) {
        rt->FillRoundedRectangle(inRound, pInBg);
        pInBg->Release();
    }

    ID2D1SolidColorBrush* pInBorder = nullptr;
    rt->CreateSolidColorBrush(m_inputFocused ? D2D1::ColorF(0.35f, 0.75f, 0.98f, 0.80f) : D2D1::ColorF(1.0f, 1.0f, 1.0f, 0.12f), &pInBorder);
    if (pInBorder) {
        rt->DrawRoundedRectangle(inRound, pInBorder, 1.0f);
        pInBorder->Release();
    }

    // Input text / placeholder
    D2D1_RECT_F inTextRect = D2D1::RectF(inRect.left + 8.0f, inRect.top + 6.0f, inRect.right - 8.0f, inRect.bottom - 4.0f);
    if (m_inputText.empty() && !m_inputFocused) {
        ID2D1SolidColorBrush* pPlaceholder = nullptr;
        rt->CreateSolidColorBrush(D2D1::ColorF(0.50f, 0.55f, 0.68f, 0.55f), &pPlaceholder);
        if (pPlaceholder) {
            rt->DrawText(L"Add a new task...", 17, ctx.GetFormatSmall(), inTextRect, pPlaceholder);
            pPlaceholder->Release();
        }
    } else {
        ID2D1SolidColorBrush* pTyping = nullptr;
        rt->CreateSolidColorBrush(D2D1::ColorF(1.0f, 1.0f, 1.0f, 0.98f), &pTyping);
        if (pTyping) {
            rt->DrawText(m_inputText.c_str(), (UINT32)m_inputText.length(), ctx.GetFormatSmall(), inTextRect, pTyping);

            // Blinking cursor
            if (m_inputFocused && m_cursorVisible) {
                float cursorX = inTextRect.left + (float)m_inputText.length() * 6.2f;
                rt->DrawLine(D2D1::Point2F(cursorX, inTextRect.top + 1.0f), D2D1::Point2F(cursorX, inTextRect.bottom - 1.0f), pTyping, 1.5f);
            }
            pTyping->Release();
        }
    }

    // Add Button [+]
    D2D1_RECT_F addRect = GetAddButtonRect();
    D2D1_ROUNDED_RECT addRound = D2D1::RoundedRect(addRect, 6.0f, 6.0f);
    ID2D1SolidColorBrush* pAddBg = nullptr;
    rt->CreateSolidColorBrush(D2D1::ColorF(0.22f, 0.75f, 0.98f, 0.85f), &pAddBg);
    if (pAddBg) {
        rt->FillRoundedRectangle(addRound, pAddBg);
        pAddBg->Release();
    }

    ID2D1SolidColorBrush* pAddPlus = nullptr;
    rt->CreateSolidColorBrush(D2D1::ColorF(1.0f, 1.0f, 1.0f, 1.0f), &pAddPlus);
    if (pAddPlus) {
        float cx = (addRect.left + addRect.right) / 2.0f;
        float cy = (addRect.top + addRect.bottom) / 2.0f;
        rt->DrawLine(D2D1::Point2F(cx - 5.0f, cy), D2D1::Point2F(cx + 5.0f, cy), pAddPlus, 2.0f);
        rt->DrawLine(D2D1::Point2F(cx, cy - 5.0f), D2D1::Point2F(cx, cy + 5.0f), pAddPlus, 2.0f);
        pAddPlus->Release();
    }
}
