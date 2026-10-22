#pragma once
#include <string>
#include <vector>
#include "http_client.h"

struct NewsArticle {
    std::wstring title;
    std::wstring link;
    std::wstring source;
    std::wstring pubDate;
};

class XmlRssParser {
public:
    static std::vector<NewsArticle> ParseRss(const std::string& xmlContent, const std::wstring& defaultSource = L"World News") {
        std::vector<NewsArticle> articles;
        if (xmlContent.empty()) return articles;

        size_t pos = 0;
        while (true) {
            size_t itemStart = xmlContent.find("<item", pos);
            if (itemStart == std::string::npos) {
                // try Atom <entry>
                itemStart = xmlContent.find("<entry", pos);
                if (itemStart == std::string::npos) break;
            }

            size_t itemClose = xmlContent.find(">", itemStart);
            if (itemClose == std::string::npos) break;

            size_t itemEnd = xmlContent.find("</item>", itemClose);
            if (itemEnd == std::string::npos) {
                itemEnd = xmlContent.find("</entry>", itemClose);
                if (itemEnd == std::string::npos) break;
            }

            std::string itemBlock = xmlContent.substr(itemClose + 1, itemEnd - (itemClose + 1));
            pos = itemEnd + 7;

            std::string title = ExtractTag(itemBlock, "title");
            std::string link = ExtractTag(itemBlock, "link");
            if (link.empty()) {
                // Check if link is an attribute like <link href="..." />
                size_t linkTag = itemBlock.find("<link");
                if (linkTag != std::string::npos) {
                    size_t hrefPos = itemBlock.find("href=\"", linkTag);
                    if (hrefPos != std::string::npos) {
                        size_t hrefEnd = itemBlock.find("\"", hrefPos + 6);
                        if (hrefEnd != std::string::npos) {
                            link = itemBlock.substr(hrefPos + 6, hrefEnd - (hrefPos + 6));
                        }
                    }
                }
            }

            std::string pubDate = ExtractTag(itemBlock, "pubDate");

            // Strip CDATA and HTML entities
            title = CleanXmlText(title);
            link = CleanXmlText(link);

            if (!title.empty()) {
                NewsArticle art;
                art.title = HttpClient::Utf8ToWide(title);
                art.link = HttpClient::Utf8ToWide(link);
                art.source = defaultSource;
                art.pubDate = HttpClient::Utf8ToWide(pubDate);
                articles.push_back(art);
                if (articles.size() >= 20) break; // Limit to 20 articles
            }
        }

        return articles;
    }

private:
    static std::string ExtractTag(const std::string& xml, const std::string& tagName) {
        std::string openTag = "<" + tagName;
        size_t start = xml.find(openTag);
        if (start == std::string::npos) return "";

        size_t tagClose = xml.find(">", start);
        if (tagClose == std::string::npos) return "";

        std::string closeTag = "</" + tagName + ">";
        size_t end = xml.find(closeTag, tagClose);
        if (end == std::string::npos) return "";

        return xml.substr(tagClose + 1, end - (tagClose + 1));
    }

    static std::string CleanXmlText(std::string text) {
        // Strip CDATA
        size_t cdataStart = text.find("<![CDATA[");
        if (cdataStart != std::string::npos) {
            size_t cdataEnd = text.find("]]>", cdataStart);
            if (cdataEnd != std::string::npos) {
                text = text.substr(cdataStart + 9, cdataEnd - (cdataStart + 9));
            }
        }

        // Replace common XML entities
        auto replaceAll = [](std::string& str, const std::string& from, const std::string& to) {
            size_t p = 0;
            while ((p = str.find(from, p)) != std::string::npos) {
                str.replace(p, from.length(), to);
                p += to.length();
            }
        };

        replaceAll(text, "&quot;", "\"");
        replaceAll(text, "&apos;", "'");
        replaceAll(text, "&amp;", "&");
        replaceAll(text, "&lt;", "<");
        replaceAll(text, "&gt;", ">");
        replaceAll(text, "&#39;", "'");
        replaceAll(text, "&#34;", "\"");
        replaceAll(text, "&#8217;", "'");
        replaceAll(text, "&#8216;", "'");
        replaceAll(text, "&#8220;", "\"");
        replaceAll(text, "&#8221;", "\"");
        replaceAll(text, "&#8211;", "-");
        replaceAll(text, "&#8212;", "--");

        // Trim leading and trailing whitespace
        while (!text.empty() && (text.front() == ' ' || text.front() == '\t' || text.front() == '\r' || text.front() == '\n')) {
            text.erase(text.begin());
        }
        while (!text.empty() && (text.back() == ' ' || text.back() == '\t' || text.back() == '\r' || text.back() == '\n')) {
            text.pop_back();
        }

        return text;
    }
};
