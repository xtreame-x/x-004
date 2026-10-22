cmake_minimum_required(VERSION 3.20)
project(WinWidgets LANGUAGES CXX RC)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_definitions(-DUNICODE -D_UNICODE)

set(SOURCES
    src/main.cpp
    src/app.cpp
    src/graphics/d2d_context.cpp
    src/utils/system_stats.cpp
    src/utils/http_client.cpp
    src/widgets/widget_base.cpp
    src/widgets/widget_clock.cpp
    src/widgets/widget_system.cpp
    src/widgets/widget_weather.cpp
    src/widgets/widget_calendar.cpp
    src/widgets/widget_tasks.cpp
    src/widgets/widget_news.cpp
    src/widgets/widget_hub.cpp
    src/resources/resource.rc
)

add_executable(WinWidgets WIN32 ${SOURCES})

target_link_libraries(WinWidgets PRIVATE
    d2d1
    dwrite
    windowscodecs
    ole32
    shlwapi
    comctl32
    winhttp
    dwmapi
    gdi32
    user32
    shell32
)
