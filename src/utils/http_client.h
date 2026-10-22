#pragma once
#include <windows.h>
#include <winhttp.h>
#include <string>
#include <functional>

class HttpClient {
public:
    static std::string Get(const std::wstring& url, int timeoutSec = 8);
    static void GetAsync(const std::wstring& url, std::function<void(bool success, const std::string& body)> callback, int timeoutSec = 8);

    static std::wstring Utf8ToWide(const std::string& str);
    static std::string WideToUtf8(const std::wstring& wstr);
};
