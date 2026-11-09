// https://github.com/Chainski/ProxyScraper

/* 
 This file is part of ProxyScraper. (https://github.com/Chainski/ProxyScraper)
 Copyright (C) 2024 Chainski
 This program is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.
 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.
 You should have received a copy of the GNU General Public License
 along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/

// ProxyScraper v2.0 - multithreaded proxy scraper (HTTP/S, SOCKS4, SOCKS5)
#include <windows.h>
#include <winhttp.h>
#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <thread>
#include <chrono>
#include <regex>
#include <unordered_set>
#include <mutex>
#include <atomic>
#include <algorithm>
#pragma comment(lib, "winhttp.lib")

namespace cfg {
    // Windows UA from https://github.com/microlinkhq/top-user-agents
    constexpr wchar_t kUserAgent[] = L"Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/156.0.0.0 Safari/537.36";
    constexpr wchar_t kHeaders[] =
        L"Accept: text/html,application/xhtml+xml,application/xml;q=0.9,image/avif,image/webp,*/*;q=0.8\r\n"
        L"Accept-Language: en-US,en;q=0.9\r\n"
        L"Cache-Control: no-cache\r\n"
        L"Pragma: no-cache";
    constexpr DWORD kResolveTimeout = 8000, kConnectTimeout = 10000, kSendTimeout = 10000, kRecvTimeout = 15000;
    constexpr size_t kWorkers = 32;
}

namespace clr {
    constexpr const char* RESET   = "\033[0m";
    constexpr const char* RED     = "\033[31m";
    constexpr const char* GREEN   = "\033[32m";
    constexpr const char* BLUE    = "\033[34m";
    constexpr const char* MAGENTA = "\033[35m";
    constexpr const char* CYAN    = "\033[36m";
    constexpr const char* YELLOW  = "\033[33m";
    constexpr const char* GRAY    = "\033[90m";
}

// RAII wrapper for WinHTTP handles
struct WinHttpHandle {
    HINTERNET h = nullptr;
    WinHttpHandle() = default;
    explicit WinHttpHandle(HINTERNET handle) : h(handle) {}
    ~WinHttpHandle() { if (h) WinHttpCloseHandle(h); }
    WinHttpHandle(const WinHttpHandle&) = delete;
    WinHttpHandle& operator=(const WinHttpHandle&) = delete;
    operator HINTERNET() const { return h; }
    explicit operator bool() const { return h != nullptr; }
};

void SetupConsole() {
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hOut == INVALID_HANDLE_VALUE || !hOut) return;
    DWORD mode = 0;
    if (GetConsoleMode(hOut, &mode))
        SetConsoleMode(hOut, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
    CONSOLE_SCREEN_BUFFER_INFO info{};
    if (GetConsoleScreenBufferInfo(hOut, &info)) {
        COORD size{};
        size.X = (SHORT)(std::max)((SHORT)120, (SHORT)(std::max)(info.dwSize.X, (SHORT)(info.srWindow.Right - info.srWindow.Left + 1)));
        size.Y = (SHORT)(std::max)((SHORT)9001, (SHORT)(std::max)(info.dwSize.Y, (SHORT)(info.srWindow.Bottom - info.srWindow.Top + 1)));
        SetConsoleScreenBufferSize(hOut, size);
    }
}

// Converts a wide string to UTF-8 (safe narrow printing; mixing wcout with cout on a byte-oriented stdout is undefined, so we avoid wcout entirely)
static std::string WideToUtf8(const std::wstring& w) {
    if (w.empty()) return {};
    const int len = WideCharToMultiByte(CP_UTF8, 0, w.data(), (int)w.size(), nullptr, 0, nullptr, nullptr);
    std::string out(len > 0 ? len : 0, '\0');
    if (len > 0) WideCharToMultiByte(CP_UTF8, 0, w.data(), (int)w.size(), out.data(), len, nullptr, nullptr);
    return out;
}

// Sources
enum class Protocol { Http = 0, Socks4, Socks5 };

struct Source {
    Protocol proto;
    std::wstring url;
};

static const std::vector<Source> kSources = [] {
    std::vector<Source> v;
    auto add = [&v](Protocol p, std::initializer_list<const wchar_t*> urls) {
        for (const auto* u : urls) v.push_back({p, u});
    };
    add(Protocol::Http, {
        L"https://api.proxyscrape.com/v2/?request=getproxies&protocol=http&timeout=10000&country=all",
        L"https://raw.githubusercontent.com/roosterkid/openproxylist/main/HTTPS_RAW.txt",
        L"https://raw.githubusercontent.com/monosans/proxy-list/main/proxies/http.txt",
        L"https://raw.githubusercontent.com/TheSpeedX/PROXY-List/master/http.txt",
        L"https://api.openproxylist.xyz/http.txt",
        L"https://raw.githubusercontent.com/mmpx12/proxy-list/master/http.txt",
        L"https://api.proxyscrape.com/v2/?request=getproxies&protocol=http&timeout=10000&country=all&ssl=all&anonymity=all",
        // v2.0 additions
        L"https://api.proxyscrape.com/v4/free-proxy-list/get?request=displayproxies&protocol=http&proxy_format=ipport&format=text",
        L"https://proxyspace.pro/http.txt",
        L"https://proxyspace.pro/https.txt",
        L"https://raw.githubusercontent.com/officialputuid/KangProxy/main/https/https.txt",
        L"https://raw.githubusercontent.com/Tianndev/free-proxy/main/proxy/http.txt",
        L"https://raw.githubusercontent.com/Tianndev/free-proxy/main/proxy/https.txt",
        L"https://raw.githubusercontent.com/hproxy-com/free-proxy-list/main/http.txt",
        L"https://raw.githubusercontent.com/hproxy-com/free-proxy-list/main/https.txt",
        L"https://raw.githubusercontent.com/dpangestuw/Free-Proxy/main/http_proxies.txt",
        L"https://raw.githubusercontent.com/xyzs996/free-proxy-health-list/main/http.txt",
        L"https://raw.githubusercontent.com/xyzs996/free-proxy-health-list/main/https.txt",
        L"https://raw.githubusercontent.com/vmheaven/VMHeaven.io-Free-Proxy-List/main/verified/http.txt",
        L"https://raw.githubusercontent.com/vmheaven/VMHeaven.io-Free-Proxy-List/main/verified/https.txt",
        L"https://raw.githubusercontent.com/zloi-user/hideip.me/main/http.txt",
        L"https://raw.githubusercontent.com/zloi-user/hideip.me/main/https.txt",
        L"https://raw.githubusercontent.com/iplocate/free-proxy-list/main/protocols/http.txt",
        L"https://raw.githubusercontent.com/litportnet/free-proxy-list/main/proxies/http.txt",
        L"https://raw.githubusercontent.com/proxmint/free-proxy-list/main/proxies/http.txt",
        L"https://raw.githubusercontent.com/proxygenerator1/ProxyGenerator/main/Stable/http.txt",
        L"https://raw.githubusercontent.com/proxygenerator1/ProxyGenerator/main/Stable/https.txt",
        L"https://raw.githubusercontent.com/stormsia/proxy-list/main/http.txt",
        L"https://proxylist.geonode.com/api/proxy-list?limit=500&page=1&sort_by=lastChecked&sort_type=desc&protocols=http%2Chttps",
        L"https://raw.githubusercontent.com/ProxyScraper/ProxyScraper/main/http.txt",
        L"https://raw.githubusercontent.com/ErcinDedeoglu/proxies/main/proxies/http.txt",
        L"https://raw.githubusercontent.com/ErcinDedeoglu/proxies/main/proxies/https.txt",
        L"https://raw.githubusercontent.com/proxifly/free-proxy-list/main/proxies/protocols/http/data.txt",
        L"https://raw.githubusercontent.com/proxifly/free-proxy-list/main/proxies/protocols/https/data.txt",
        L"https://raw.githubusercontent.com/6yyn/free-proxy-list/main/http.txt",
        L"https://raw.githubusercontent.com/Bes-js/public-proxy-list/main/proxies.txt",
        L"https://raw.githubusercontent.com/EDT-Pages/Proxy-List/main/data/http.json",
        L"https://raw.githubusercontent.com/dinoz0rg/proxy-list/main/checked_proxies/http.txt",
        L"https://raw.githubusercontent.com/maximilianfeix/free-proxy-list/main/https.txt",
        L"https://raw.githubusercontent.com/komutan234/Proxy-List-Free/main/proxies/http.txt",
    });
    add(Protocol::Socks4, {
        L"https://api.proxyscrape.com/v2/?request=getproxies&protocol=socks4&timeout=10000&country=all",
        L"https://raw.githubusercontent.com/roosterkid/openproxylist/main/SOCKS4_RAW.txt",
        L"https://raw.githubusercontent.com/monosans/proxy-list/main/proxies/socks4.txt",
        L"https://raw.githubusercontent.com/TheSpeedX/PROXY-List/master/socks4.txt",
        L"https://api.openproxylist.xyz/socks4.txt",
        L"https://raw.githubusercontent.com/mmpx12/proxy-list/master/socks4.txt",
        // v2.0 additions
        L"https://api.proxyscrape.com/v4/free-proxy-list/get?request=displayproxies&protocol=socks4&proxy_format=ipport&format=text",
        L"https://proxyspace.pro/socks4.txt",
        L"https://raw.githubusercontent.com/Tianndev/free-proxy/main/proxy/socks4.txt",
        L"https://raw.githubusercontent.com/hproxy-com/free-proxy-list/main/socks4.txt",
        L"https://raw.githubusercontent.com/dpangestuw/Free-Proxy/main/socks4_proxies.txt",
        L"https://raw.githubusercontent.com/xyzs996/free-proxy-health-list/main/socks4.txt",
        L"https://raw.githubusercontent.com/vmheaven/VMHeaven.io-Free-Proxy-List/main/verified/socks4.txt",
        L"https://raw.githubusercontent.com/zloi-user/hideip.me/main/socks4.txt",
        L"https://raw.githubusercontent.com/iplocate/free-proxy-list/main/protocols/socks4.txt",
        L"https://raw.githubusercontent.com/litportnet/free-proxy-list/main/proxies/socks4.txt",
        L"https://raw.githubusercontent.com/proxmint/free-proxy-list/main/proxies/socks4.txt",
        L"https://raw.githubusercontent.com/proxygenerator1/ProxyGenerator/main/Stable/socks4.txt",
        L"https://raw.githubusercontent.com/VPSLabCloud/VPSLab-Free-Proxy-List/main/socks4_all.txt",
        L"https://proxylist.geonode.com/api/proxy-list?limit=500&page=1&sort_by=lastChecked&sort_type=desc&protocols=socks4",
        L"https://raw.githubusercontent.com/ProxyScraper/ProxyScraper/main/socks4.txt",
        L"https://raw.githubusercontent.com/ErcinDedeoglu/proxies/main/proxies/socks4.txt",
        L"https://raw.githubusercontent.com/proxifly/free-proxy-list/main/proxies/protocols/socks4/data.txt",
        L"https://raw.githubusercontent.com/Anonym0usWork1221/Free-Proxies/main/proxy_files/socks4_proxies.txt",
        L"https://raw.githubusercontent.com/proxio-io/proxy-list/main/socks4.txt",
        L"https://raw.githubusercontent.com/komutan234/Proxy-List-Free/main/proxies/socks4.txt",
    });
    add(Protocol::Socks5, {
        L"https://api.proxyscrape.com/v2/?request=getproxies&protocol=socks5&timeout=10000&country=all",
        L"https://raw.githubusercontent.com/roosterkid/openproxylist/main/SOCKS5_RAW.txt",
        L"https://raw.githubusercontent.com/monosans/proxy-list/main/proxies/socks5.txt",
        L"https://raw.githubusercontent.com/TheSpeedX/PROXY-List/master/socks5.txt",
        L"https://raw.githubusercontent.com/hookzof/socks5_list/master/proxy.txt",
        L"https://api.openproxylist.xyz/socks5.txt",
        L"https://raw.githubusercontent.com/mmpx12/proxy-list/master/socks5.txt",
        // v2.0 additions
        L"https://api.proxyscrape.com/v4/free-proxy-list/get?request=displayproxies&protocol=socks5&proxy_format=ipport&format=text",
        L"https://proxyspace.pro/socks5.txt",
        L"https://raw.githubusercontent.com/Tianndev/free-proxy/main/proxy/socks5.txt",
        L"https://raw.githubusercontent.com/hproxy-com/free-proxy-list/main/socks5.txt",
        L"https://raw.githubusercontent.com/dpangestuw/Free-Proxy/main/socks5_proxies.txt",
        L"https://raw.githubusercontent.com/xyzs996/free-proxy-health-list/main/socks5.txt",
        L"https://raw.githubusercontent.com/vmheaven/VMHeaven.io-Free-Proxy-List/main/verified/socks5.txt",
        L"https://raw.githubusercontent.com/zloi-user/hideip.me/main/socks5.txt",
        L"https://raw.githubusercontent.com/iplocate/free-proxy-list/main/protocols/socks5.txt",
        L"https://raw.githubusercontent.com/litportnet/free-proxy-list/main/proxies/socks5.txt",
        L"https://raw.githubusercontent.com/proxmint/free-proxy-list/main/proxies/socks5.txt",
        L"https://raw.githubusercontent.com/proxygenerator1/ProxyGenerator/main/Stable/socks5.txt",
        L"https://raw.githubusercontent.com/VPSLabCloud/VPSLab-Free-Proxy-List/main/socks5_all.txt",
        L"https://raw.githubusercontent.com/ALIILAPRO/Proxy/main/socks5.txt",
        L"https://raw.githubusercontent.com/stormsia/proxy-list/main/socks5.txt",
        L"https://proxylist.geonode.com/api/proxy-list?limit=500&page=1&sort_by=lastChecked&sort_type=desc&protocols=socks5",
        L"https://raw.githubusercontent.com/ProxyScraper/ProxyScraper/main/socks5.txt",
        L"https://raw.githubusercontent.com/ErcinDedeoglu/proxies/main/proxies/socks5.txt",
        L"https://raw.githubusercontent.com/proxifly/free-proxy-list/main/proxies/protocols/socks5/data.txt",
        L"https://raw.githubusercontent.com/EDT-Pages/Proxy-List/main/data/socks5.json",
        L"https://raw.githubusercontent.com/ProxyScrape/free-proxy-list/main/proxies/protocols/socks5/data.txt",
        L"https://raw.githubusercontent.com/dinoz0rg/proxy-list/main/checked_proxies/socks5.txt",
        L"https://raw.githubusercontent.com/maximilianfeix/free-proxy-list/main/socks5.txt",
        L"https://raw.githubusercontent.com/nguyenduytan/NDT-Proxy-Scraper/main/socks5.txt",
        L"https://raw.githubusercontent.com/proxy-free/free-proxy-list/main/socks5.txt",
        L"https://raw.githubusercontent.com/z3a4/free-proxy-list/main/proxies/unchecked/socks5-proxies.txt",
        L"https://raw.githubusercontent.com/komutan234/Proxy-List-Free/main/proxies/socks5.txt",
    });
    return v;
}();

constexpr const char* kProtoName[3]  = {"HTTP/S", "SOCKS4", "SOCKS5"};
constexpr const char* kProtoTag[3]   = {"HTTP", "SOCKS4", "SOCKS5"};
constexpr const char* kProtoFile[3]  = {"http.txt", "socks4.txt", "socks5.txt"};

// Scraper core
std::mutex g_coutMutex;
std::mutex g_setMutex[3];
std::unordered_set<std::string> g_proxies[3];
std::atomic<int> g_done[3]{ {0}, {0}, {0} };
std::atomic<int> g_totalPerProto[3]{ {0}, {0}, {0} };
std::atomic<size_t> g_extracted[3]{ {0}, {0}, {0} };
std::atomic<size_t> g_invalid[3]{ {0}, {0}, {0} };

// Validates an IPv4 address (octets 0-255, no loopback/0.x/multicast) and port (1-65535)
static bool IsValidProxy(const std::string& ip, int port) {
    if (port < 1 || port > 65535) return false;
    int octet = 0, parts = 0, digits = 0;
    for (char c : ip) {
        if (c == '.') {
            if (digits == 0 || octet > 255) return false;
            if (parts == 0 && (octet == 0 || octet == 127 || octet >= 224)) return false;
            ++parts; octet = 0; digits = 0;
        } else if (c >= '0' && c <= '9') {
            octet = octet * 10 + (c - '0');
            ++digits;
            if (digits > 3 || octet > 255) return false;
        } else return false;
    }
    return parts == 3 && digits > 0 && octet <= 255;
}

// Extracts proxies from a response body (plain ip:port text or {"ip":..,"port":..} JSON)
static size_t ExtractProxies(const std::string& body, Protocol proto) {
    static const std::regex reText(R"rx(\b((?:\d{1,3}\.){3}\d{1,3}):(\d{1,5})\b)rx");
    static const std::regex reJson(R"rx("(?:ip|host)"\s*:\s*"((?:\d{1,3}\.){3}\d{1,3})"[^}]{0,500}?"port"\s*:\s*"?(\d{1,5})"?)rx");
    const bool json = body.find_first_not_of(" \t\r\n") != std::string::npos &&
                      (body[body.find_first_not_of(" \t\r\n")] == '{' || body[body.find_first_not_of(" \t\r\n")] == '[');
    const std::regex& re = json ? reJson : reText;
    const int p = static_cast<int>(proto);
    size_t added = 0;
    auto begin = std::sregex_iterator(body.begin(), body.end(), re);
    for (auto it = begin; it != std::sregex_iterator(); ++it) {
        const std::string ip = (*it)[1].str();
        const int port = std::atoi((*it)[2].str().c_str());
        if (!IsValidProxy(ip, port)) { ++g_invalid[p]; continue; }
        {
            std::lock_guard<std::mutex> lock(g_setMutex[p]);
            g_proxies[p].insert(ip + ":" + std::to_string(port));
        }
        ++added;
    }
    return added;
}

// Shared WinHTTP session (thread-safe for concurrent requests)
static HINTERNET g_session = nullptr;

static std::string SendRequest(const std::wstring& url, DWORD& statusOut, long& elapsedMs) {
    statusOut = 0;
    elapsedMs = 0;
    std::string result;
    URL_COMPONENTS urlComp{};
    urlComp.dwStructSize = sizeof(urlComp);
    urlComp.dwSchemeLength = urlComp.dwHostNameLength =
    urlComp.dwUrlPathLength = urlComp.dwExtraInfoLength = (DWORD)-1;
    if (!g_session || !WinHttpCrackUrl(url.c_str(), 0, 0, &urlComp)) return result;
    const auto t0 = std::chrono::steady_clock::now();
    WinHttpHandle hConnect(WinHttpConnect(g_session,
        std::wstring(urlComp.lpszHostName, urlComp.dwHostNameLength).c_str(), urlComp.nPort, 0));
    if (!hConnect) return result;
    const bool secure = urlComp.nScheme == INTERNET_SCHEME_HTTPS;
    std::wstring path;
    if (urlComp.dwUrlPathLength > 0) path.assign(urlComp.lpszUrlPath, urlComp.dwUrlPathLength);
    if (urlComp.dwExtraInfoLength > 0) path.append(urlComp.lpszExtraInfo, urlComp.dwExtraInfoLength);
    WinHttpHandle hRequest(WinHttpOpenRequest(hConnect, L"GET", path.empty() ? nullptr : path.c_str(),
        nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, secure ? WINHTTP_FLAG_SECURE : 0));
    if (!hRequest) return result;
    WinHttpSetTimeouts(hRequest, cfg::kResolveTimeout, cfg::kConnectTimeout, cfg::kSendTimeout, cfg::kRecvTimeout);
    if (secure) {
        DWORD flags = SECURITY_FLAG_IGNORE_UNKNOWN_CA; // tolerate sources with odd chains
        WinHttpSetOption(hRequest, WINHTTP_OPTION_SECURITY_FLAGS, &flags, sizeof(flags));
    }
    DWORD statusSize = sizeof(DWORD);
    const bool ok = WinHttpAddRequestHeaders(hRequest, cfg::kHeaders, (DWORD)-1L,
            WINHTTP_ADDREQ_FLAG_ADD | WINHTTP_ADDREQ_FLAG_REPLACE) &&
        WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0) &&
        WinHttpReceiveResponse(hRequest, nullptr) &&
        WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
            nullptr, &statusOut, &statusSize, nullptr) && statusOut == HTTP_STATUS_OK;
    if (!ok) return result;
    std::vector<char> buffer(16384);
    DWORD dwSize = 0, dwRead = 0;
    while (WinHttpQueryDataAvailable(hRequest, &dwSize) && dwSize > 0) {
        if (!WinHttpReadData(hRequest, buffer.data(), (std::min<DWORD>)(dwSize, (DWORD)buffer.size()), &dwRead) || dwRead == 0)
            break;
        result.append(buffer.data(), dwRead);
        if (result.size() > 32u * 1024u * 1024u) break; // sanity cap 
    }
    elapsedMs = (long)std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - t0).count();
    return result;
}

// Status-only check (GET, response body not read) - used by the pre-flight online report
static DWORD ProbeStatus(const std::wstring& url) {
    URL_COMPONENTS urlComp{};
    urlComp.dwStructSize = sizeof(urlComp);
    urlComp.dwSchemeLength = urlComp.dwHostNameLength =
    urlComp.dwUrlPathLength = urlComp.dwExtraInfoLength = (DWORD)-1;
    if (!g_session || !WinHttpCrackUrl(url.c_str(), 0, 0, &urlComp)) return 0;
    WinHttpHandle hConnect(WinHttpConnect(g_session,
        std::wstring(urlComp.lpszHostName, urlComp.dwHostNameLength).c_str(), urlComp.nPort, 0));
    if (!hConnect) return 0;
    const bool secure = urlComp.nScheme == INTERNET_SCHEME_HTTPS;
    std::wstring path;
    if (urlComp.dwUrlPathLength > 0) path.assign(urlComp.lpszUrlPath, urlComp.dwUrlPathLength);
    if (urlComp.dwExtraInfoLength > 0) path.append(urlComp.lpszExtraInfo, urlComp.dwExtraInfoLength);
    WinHttpHandle hRequest(WinHttpOpenRequest(hConnect, L"GET", path.empty() ? nullptr : path.c_str(),
        nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, secure ? WINHTTP_FLAG_SECURE : 0));
    if (!hRequest) return 0;
    WinHttpSetTimeouts(hRequest, cfg::kResolveTimeout, cfg::kConnectTimeout, cfg::kSendTimeout, cfg::kRecvTimeout);
    if (secure) {
        DWORD flags = SECURITY_FLAG_IGNORE_UNKNOWN_CA; // tolerate sources with odd chains
        WinHttpSetOption(hRequest, WINHTTP_OPTION_SECURITY_FLAGS, &flags, sizeof(flags));
    }
    DWORD status = 0, statusSize = sizeof(DWORD);
    if (WinHttpAddRequestHeaders(hRequest, cfg::kHeaders, (DWORD)-1L,
            WINHTTP_ADDREQ_FLAG_ADD | WINHTTP_ADDREQ_FLAG_REPLACE) &&
        WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0) &&
        WinHttpReceiveResponse(hRequest, nullptr))
        WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
            nullptr, &status, &statusSize, nullptr);
    return status;
}

static void ScrapeSource(const Source& src) {
    const int p = static_cast<int>(src.proto);
    DWORD status = 0; long ms = 0;
    std::string body;
    size_t added = 0;
    std::string err;
    try {
        body = SendRequest(src.url, status, ms);
        added = ExtractProxies(body, src.proto);
        g_extracted[p] += added;
    } catch (const std::exception& e) {
        err = e.what();
    } catch (...) {
        err = "unknown error";
    }
    const int done = ++g_done[p], total = g_totalPerProto[p];
    // short host label for the progress line (narrow-printed: wcout on a byte-oriented stdout is undefined - see WideToUtf8)
    std::wstring whost = src.url;
    if (auto pos = whost.find(L"://"); pos != std::wstring::npos) whost.erase(0, pos + 3);
    if (auto pos = whost.find(L'/'); pos != std::wstring::npos) whost.erase(pos);
    std::lock_guard<std::mutex> lock(g_coutMutex);
    std::cout << clr::GRAY << "  [" << kProtoTag[p] << "]" << clr::RESET << " ";
    std::cout << clr::BLUE << "[" << done << "/" << total << "]" << clr::RESET << " ";
    if (!err.empty())
        std::cout << clr::RED << "FAIL" << clr::RESET
                  << clr::GRAY << " (" << err << ")" << clr::RESET << "  ";
    else if (!body.empty())
        std::cout << clr::GREEN << "OK  " << clr::RESET
                  << clr::CYAN << added << clr::RESET << " proxies "
                  << clr::GRAY << "(" << ms << " ms)" << clr::RESET << "  ";
    else
        std::cout << clr::RED << "FAIL" << clr::RESET
                  << clr::GRAY << " (HTTP " << status << ")" << clr::RESET << "      ";
    std::cout << WideToUtf8(whost) << std::endl;
}

static void Worker(std::atomic<size_t>& next) {
    size_t i = 0;
    while ((i = next++) < kSources.size()) {
        try {
            ScrapeSource(kSources[i]);
        } catch (...) {
            // a misbehaving worker must never kill the pool
        }
    }
}

static void TypewriterEffect(const std::string& text, int delayMs) {
    for (char c : text) {
        std::cout << clr::YELLOW << c << clr::RESET << std::flush;
        std::this_thread::sleep_for(std::chrono::milliseconds(delayMs));
    }
    std::cout << std::endl;
}

static void PrintBanner() {
    std::cout << clr::CYAN << R"(
                    ╔═══╗╔═══╗╔═══╗╔═╗╔═╗╔╗  ╔╗    ╔═══╗╔═══╗╔═══╗╔═══╗╔═══╗╔═══╗╔═══╗
                    ║╔═╗║║╔═╗║║╔═╗║╚╗╚╝╔╝║╚╗╔╝║    ║╔═╗║║╔═╗║║╔═╗║║╔═╗║║╔═╗║║╔══╝║╔═╗║
                    ║╚═╝║║╚═╝║║║ ║║ ╚╗╔╝ ╚╗╚╝╔╝    ║╚══╗║║ ╚╝║╚═╝║║║ ║║║╚═╝║║╚══╗║╚═╝║
                    ║╔══╝║╔╗╔╝║║ ║║ ╔╝╚╗  ╚╗╔╝     ╚══╗║║║ ╔╗║╔╗╔╝║╚═╝║║╔══╝║╔══╝║╔╗╔╝
                    ║║   ║║║╚╗║╚═╝║╔╝╔╗╚╗  ║║      ║╚═╝║║╚═╝║║║║╚╗║╔═╗║║║   ║╚══╗║║║╚╗
                    ╚╝   ╚╝╚═╝╚═══╝╚═╝╚═╝  ╚╝      ╚═══╝╚═══╝╚╝╚═╝╚╝ ╚╝╚╝   ╚═══╝╚╝╚═╝
                                https://github.com/Chainski/ProxyScraper
                                   PROTOCOLS: HTTP/S | SOCKS4 | SOCKS5
    )" << clr::RESET << std::endl;
}

int main() {
    SetConsoleOutputCP(CP_UTF8);
    SetupConsole();
    SetConsoleTitleA("Proxy Scraper v2.0 - Made By: Chainski");
    PrintBanner();
    for (const auto& s : kSources) ++g_totalPerProto[static_cast<int>(s.proto)];
    g_session = WinHttpOpen(cfg::kUserAgent, WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
        WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!g_session) {
        std::cout << clr::RED << "[-] Failed to initialize WinHTTP" << clr::RESET << std::endl;
        return 1;
    }
    TypewriterEffect("[+] This program will autoscrape proxies into separate files", 10);
    std::cout << clr::CYAN << "[+] Scraping " << kSources.size() << " sources with "
              << cfg::kWorkers << " threads, please wait . . ." << clr::RESET << std::endl;
    // Pre-flight: verify every source answers 200 OK (status-only probes, bodies not read)
    {
        std::atomic<size_t> probeNext{0}, online{0};
        auto probe = [&online, &probeNext] {
            size_t i = 0;
            while ((i = probeNext++) < kSources.size()) {
                try { if (ProbeStatus(kSources[i].url) == HTTP_STATUS_OK) ++online; } catch (...) {}
            }
        };
        std::vector<std::thread> probePool;
        probePool.reserve(cfg::kWorkers);
        for (size_t i = 0; i < cfg::kWorkers; ++i) probePool.emplace_back(probe);
        for (auto& t : probePool) t.join();
        if (online == 0)
            std::cout << clr::RED << "[NO SOURCE REACHABLE] - check your internet connection"
                      << clr::RESET << std::endl;
        else if (online < kSources.size())
            std::cout << clr::YELLOW << "[" << online << "/" << kSources.size() << " sources online, "
                      << (kSources.size() - online) << " unreachable]" << clr::RESET << std::endl;
        else
            std::cout << clr::GREEN << "[ALL SOURCES ONLINE] " << online << "/" << kSources.size()
                      << " (200 OK)" << clr::RESET << std::endl;
    }
    for (int p = 0; p < 3; ++p)
        std::cout << clr::YELLOW << "[!] " << kProtoName[p] << ": " << g_totalPerProto[p]
                  << " sources queued" << clr::RESET << std::endl;
    std::cout << std::endl;
    const auto t0 = std::chrono::steady_clock::now();
    size_t grandTotal = 0;
    try {
        std::atomic<size_t> next{0};
        std::vector<std::thread> pool;
        pool.reserve(cfg::kWorkers);
        for (size_t i = 0; i < cfg::kWorkers; ++i)
            pool.emplace_back(Worker, std::ref(next));
        for (auto& t : pool) t.join();
        std::cout << std::endl;
        for (int p = 0; p < 3; ++p) {
            std::vector<std::string> sorted(g_proxies[p].begin(), g_proxies[p].end());
            std::sort(sorted.begin(), sorted.end());
            const size_t dupes = g_extracted[p] > sorted.size() ? g_extracted[p] - sorted.size() : 0;
            if (sorted.empty()) {
                std::cout << clr::RED << "[!] No valid " << kProtoName[p]
                          << " proxies were scraped - " << kProtoFile[p]
                          << " was not rewritten. Check your internet connection." << clr::RESET << std::endl;
                continue;
            }
            std::ofstream out(kProtoFile[p], std::ios::binary | std::ios::trunc);
            if (!out.is_open()) {
                std::cout << clr::RED << "[!] Failed to open " << kProtoFile[p]
                          << " for writing (file locked or folder read-only?)" << clr::RESET << std::endl;
                continue;
            }
            for (const auto& proxy : sorted) out << proxy << '\n';
            if (!out.good()) {
               std::cout << clr::RED << "[!] Write error while saving " << kProtoFile[p]
               << clr::RESET << std::endl;
               continue;
            }
            grandTotal += sorted.size();
            std::cout << clr::GREEN << "[+] Saved " << sorted.size() << " unique " << kProtoName[p]
                      << " proxies to " << kProtoFile[p] << clr::RESET
                      << clr::GRAY << "  (" << g_extracted[p] + g_invalid[p] << " scraped, "
                      << dupes + g_invalid[p] << " duplicates/invalid removed)" << clr::RESET << std::endl;
        }
    } catch (const std::exception& e) {
        std::cout << clr::RED << "[!] Unexpected error: " << e.what() << clr::RESET << std::endl;
    } catch (...) {
        std::cout << clr::RED << "[!] Unexpected fatal error during scraping." << clr::RESET << std::endl;
    }
    const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
    std::chrono::steady_clock::now() - t0).count();
    const auto secs = elapsed / 1000;
    const auto ms   = elapsed % 1000;
    if (grandTotal > 0)
    std::cout << clr::YELLOW << "[*] Done! " << grandTotal << " proxies scraped in "
              << secs << " seconds, " << ms << " ms. "
              << "If you like the project consider leaving a star."
              << clr::RESET << std::endl;
    WinHttpCloseHandle(g_session);
    std::cout << clr::GREEN << "Press any key to continue . . ." << clr::RESET;
    std::cin.get();
    return 0;
}
