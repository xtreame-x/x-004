#pragma once
#include "widget_base.h"
#include <vector>
#include <string>

struct TaskItem {
    std::wstring text;
    bool completed = false;
};

class WidgetTasks : public WidgetBase {
public:
    WidgetTasks();
    void OnUpdate() override;

protected:
    void OnRenderContent(ID2D1RenderTarget* rt) override;
    bool OnMouseDown(int x, int y, bool isRight) override;
    void OnMouseMove(int x, int y) override;
    void OnMouseLeave() override;
    void OnCharInput(wchar_t ch) override;
    void OnKeyDown(WPARAM key) override;

private:
    void LoadTasks();
    void SaveTasks();
    void AddTask(const std::wstring& text);
    void DeleteTask(size_t index);
    void ToggleTask(size_t index);

    std::vector<TaskItem> m_tasks;
    int m_hoverIndex = -1;
    int m_hoverDeleteIndex = -1;

    // Inline input box
    bool m_inputFocused = false;
    std::wstring m_inputText;
    bool m_cursorVisible = true;
    DWORD m_lastBlinkTime = 0;

    D2D1_RECT_F GetInputRect() const;
    D2D1_RECT_F GetAddButtonRect() const;
};
