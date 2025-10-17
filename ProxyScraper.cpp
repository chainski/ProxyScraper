// https://github.com/Chainski/ProxyScraper
#include <windows.h>
#include <winhttp.h>
#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <thread>
#include <chrono>
#include <algorithm>
#include <regex>
#include <unordered_set>
#include <mutex>
#pragma comment(lib, "winhttp.lib")
#pragma comment(lib, "ws2_32.lib")

using namespace std;

#define RESET      "\033[0m"
#define RED        "\033[31m"      /* Red */
#define GREEN      "\033[32m"      /* Green */
#define BLUE       "\033[34m"      /* Blue */
#define LIGHT_BLUE "\033[36m"      /* Light Blue */
#define MAGENTA    "\033[35m"      /* Magenta */
#define CYAN       "\033[36m"      /* Cyan */
#define YELLOW     "\033[33m"      /* Yellow */

void ansicolours() {
    DWORD mode;
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    GetConsoleMode(hOut, &mode);
    SetConsoleMode(hOut, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
}

void CleanProxyFile(const string& filename) {
    ifstream in(filename);
    if (!in) return;
    unordered_set<string> unique;
    vector<string> valid_lines;
    regex proxy_regex(R"(^\d{1,3}(?:\.\d{1,3}){3}:\d{2,5}$)");
    string line;
    while (getline(in, line)) {
        line.erase(remove_if(line.begin(), line.end(), ::isspace), line.end());
        if (line.empty() ||
            line.find("error code:") != string::npos ||
            line.find("Invalid API request") != string::npos)
            continue;
        size_t badPos = line.find_first_not_of("0123456789.:");
        if (badPos != string::npos)
            line = line.substr(0, badPos);

        if (regex_match(line, proxy_regex) && unique.insert(line).second)
            valid_lines.push_back(line);
    }
    ofstream out(filename, ios::trunc);
    for (const auto& proxy : valid_lines)
        out << proxy << '\n';
}
mutex fileMutex;
std::string SendRequest(const std::wstring& url) {
    std::string result;
    std::vector<char> buffer(8192);
    DWORD dwBytesRead = 0, statusCode = 0, statusSize = sizeof(DWORD), dwSize = 0;
    URL_COMPONENTS urlComp{};
    urlComp.dwStructSize = sizeof(urlComp);
    urlComp.dwSchemeLength = urlComp.dwHostNameLength = urlComp.dwUrlPathLength = urlComp.dwExtraInfoLength = (DWORD)-1;
    if (!WinHttpCrackUrl(url.c_str(), 0, 0, &urlComp))
    return result;
    auto close = [](HINTERNET h) { if (h) WinHttpCloseHandle(h); };
    HINTERNET hSession = WinHttpOpen(L"Mozilla/5.0 (X11; Linux x86_64; rv:140.0) Gecko/20100101 Firefox/140.0",
    WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,WINHTTP_NO_PROXY_NAME,WINHTTP_NO_PROXY_BYPASS, 0);
    HINTERNET hConnect = hSession ? WinHttpConnect(hSession,
    std::wstring(urlComp.lpszHostName, urlComp.dwHostNameLength).c_str(),
    urlComp.nScheme == INTERNET_SCHEME_HTTPS ? INTERNET_DEFAULT_HTTPS_PORT : INTERNET_DEFAULT_HTTP_PORT, 0) : nullptr;
    HINTERNET hRequest = hConnect ? WinHttpOpenRequest(hConnect, L"GET",
    std::wstring(urlComp.lpszUrlPath, urlComp.dwUrlPathLength).c_str(),
    nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,
    urlComp.nScheme == INTERNET_SCHEME_HTTPS ? WINHTTP_FLAG_SECURE : 0) : nullptr;
    bool ok = hRequest &&
    WinHttpSendRequest(hRequest, nullptr, 0, nullptr, 0, 0, 0) &&
    WinHttpReceiveResponse(hRequest, nullptr) &&
    WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
    nullptr, &statusCode, &statusSize, nullptr) &&
    statusCode == HTTP_STATUS_OK;
    if (ok) {
        while (WinHttpQueryDataAvailable(hRequest, &dwSize) && dwSize > 0) {
            if (!WinHttpReadData(hRequest, buffer.data(), std::min<DWORD>(dwSize, buffer.size()), &dwBytesRead) || dwBytesRead == 0)
                break;
            result.append(buffer.data(), dwBytesRead);
        }
    }
    close(hRequest);
    close(hConnect);
    close(hSession);
    return result;
}
void SaveFile(const string& filename, const string& url) {
    try {
        string content = SendRequest(wstring(url.begin(), url.end()));
        if (!content.empty()) {
            lock_guard<mutex> lock(fileMutex);
            ofstream file(filename, ios::binary | ios::app);
            if (file.is_open()) {
                file.write(content.c_str(), content.size());
                file.close();
            }
        }
    }
    catch (...) {
    }
}
void ProcessUrls(const vector<string>& urls, const string& filename, size_t start, size_t end) {
    for (size_t i = start; i < end && i < urls.size(); ++i) {
        SaveFile(filename, urls[i]);
    }
}
void TWE(const string& text, int delayMilliseconds) {
    for (char c : text) {
        cout << YELLOW << c << RESET << flush;  
        this_thread::sleep_for(chrono::milliseconds(delayMilliseconds));  
    }
    cout << endl;  
}
void printBanner() {
    cout << LIGHT_BLUE << R"(
                    ╔═══╗╔═══╗╔═══╗╔═╗╔═╗╔╗  ╔╗    ╔═══╗╔═══╗╔═══╗╔═══╗╔═══╗╔═══╗╔═══╗
                    ║╔═╗║║╔═╗║║╔═╗║╚╗╚╝╔╝║╚╗╔╝║    ║╔═╗║║╔═╗║║╔═╗║║╔═╗║║╔═╗║║╔══╝║╔═╗║
                    ║╚═╝║║╚═╝║║║ ║║ ╚╗╔╝ ╚╗╚╝╔╝    ║╚══╗║║ ╚╝║╚═╝║║║ ║║║╚═╝║║╚══╗║╚═╝║
                    ║╔══╝║╔╗╔╝║║ ║║ ╔╝╚╗  ╚╗╔╝     ╚══╗║║║ ╔╗║╔╗╔╝║╚═╝║║╔══╝║╔══╝║╔╗╔╝
                    ║║   ║║║╚╗║╚═╝║╔╝╔╗╚╗  ║║      ║╚═╝║║╚═╝║║║║╚╗║╔═╗║║║   ║╚══╗║║║╚╗
                    ╚╝   ╚╝╚═╝╚═══╝╚═╝╚═╝  ╚╝      ╚═══╝╚═══╝╚╝╚═╝╚╝ ╚╝╚╝   ╚═══╝╚╝╚═╝        
                                https://github.com/Chainski/ProxyScraper  
                                   PROTOCOLS: HTTP/S | SOCKS4 | SOCKS5  
    )" << RESET << endl;
}
int main() {
    ansicolours();
    system("mode con: cols=120 lines=50");
    SetConsoleTitleA("Proxy Scraper - Made By: Chainski");
    printBanner(); 
    string line;
    TWE("[+] This program will autoscrape proxies into separate files", 10);
    cout << CYAN << "[+] Scraping Proxies Please Wait . . ." << RESET << endl;
    vector<pair<vector<string>, string>> proxy_types = {
        {{
            "https://raw.githubusercontent.com/jetkai/proxy-list/main/online-proxies/txt/proxies-https.txt",
            "https://raw.githubusercontent.com/jetkai/proxy-list/main/online-proxies/txt/proxies-http.txt",
            "https://api.proxyscrape.com/v2/?request=getproxies&protocol=http&timeout=10000&country=all",
            "https://raw.githubusercontent.com/jetkai/proxy-list/main/archive/txt/proxies-https.txt",
            "https://raw.githubusercontent.com/jetkai/proxy-list/main/archive/txt/proxies-http.txt",
            "https://raw.githubusercontent.com/roosterkid/openproxylist/main/HTTPS_RAW.txt",
            "https://raw.githubusercontent.com/monosans/proxy-list/main/proxies/http.txt",
            "https://raw.githubusercontent.com/TheSpeedX/PROXY-List/master/http.txt",
            "https://www.proxy-list.download/api/v1/get?type=http",
            "https://www.proxy-list.download/api/v1/get?type=https",
            "https://api.openproxylist.xyz/http.txt",
            "https://raw.githubusercontent.com/mmpx12/proxy-list/master/http.txt",
            "https://raw.githubusercontent.com/ShiftyTR/Proxy-List/master/http.txt",
            "https://api.proxyscrape.com/v2/?request=getproxies&protocol=http&timeout=10000&country=all&ssl=all&anonymity=all",
            "https://raw.githubusercontent.com/proxy4parsing/proxy-list/main/http.txt"
        }, "http.txt"},
        {{
            "https://api.proxyscrape.com/v2/?request=getproxies&protocol=socks4&timeout=10000&country=all",
            "https://raw.githubusercontent.com/roosterkid/openproxylist/main/SOCKS4_RAW.txt",
            "https://raw.githubusercontent.com/monosans/proxy-list/main/proxies/socks4.txt",
            "https://raw.githubusercontent.com/TheSpeedX/PROXY-List/master/socks4.txt",
            "https://www.proxy-list.download/api/v1/get?type=socks4",
            "https://api.openproxylist.xyz/socks4.txt",
            "https://raw.githubusercontent.com/mmpx12/proxy-list/master/socks4.txt",
            "https://raw.githubusercontent.com/ShiftyTR/Proxy-List/master/socks4.txt",
            "https://raw.githubusercontent.com/rdavydov/proxy-list/main/proxies/socks4.txt",
            "https://raw.githubusercontent.com/jetkai/proxy-list/main/online-proxies/txt/proxies-socks4.txt"
        }, "socks4.txt"},
        {{
            "https://api.proxyscrape.com/v2/?request=getproxies&protocol=socks5&timeout=10000&country=all",
            "https://raw.githubusercontent.com/roosterkid/openproxylist/main/SOCKS5_RAW.txt",
            "https://raw.githubusercontent.com/monosans/proxy-list/main/proxies/socks5.txt",
            "https://raw.githubusercontent.com/TheSpeedX/PROXY-List/master/socks5.txt",
            "https://raw.githubusercontent.com/hookzof/socks5_list/master/proxy.txt",
            "https://www.proxy-list.download/api/v1/get?type=socks5",
            "https://api.openproxylist.xyz/socks5.txt",
            "https://raw.githubusercontent.com/mmpx12/proxy-list/master/socks5.txt",
            "https://raw.githubusercontent.com/ShiftyTR/Proxy-List/master/socks5.txt",
            "https://raw.githubusercontent.com/jetkai/proxy-list/main/online-proxies/txt/proxies-socks5.txt"
        }, "socks5.txt"}
    };
    vector<thread> threads;
    for (const auto& [urls, file] : proxy_types) {
        cout << MAGENTA << "[!] Scraping " << file.substr(0, file.find('.')) << " proxies...\n";
        for (size_t i = 0; i < urls.size(); i += max(size_t(1), urls.size() / 4))
            threads.emplace_back(ProcessUrls, cref(urls), file, i, min(i + urls.size() / 4, urls.size()));
        for (auto& t : threads) t.join();
        threads.clear();
        cout << CYAN << "[!] Saved " << file.substr(0, file.find('.')) << " proxies!\n";
        this_thread::sleep_for(chrono::seconds(1));
    }
    for (const auto& [_, file] : proxy_types)
    CleanProxyFile(file);
    cout << GREEN << "Press any key to continue . . .";
    cin.get();
    return 0;
}
