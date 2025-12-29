<p align= "center">
   <img src="https://img.shields.io/github/stars/Chainski/ProxyScraper?style=flat&color=%230047ab">
   <img src="https://img.shields.io/github/forks/Chainski/ProxyScraper?style=flat&color=%230047ab">
   <img src="https://hits.sh/github.com/Chainski/ProxyScraper.svg?label=views&color=0047ab">
   <br>
   <img src="https://img.shields.io/github/last-commit/Chainski/ProxyScraper?style=flat&color=%230047ab">
   <img src="https://img.shields.io/github/license/Chainski/ProxyScraper?style=flat&color=%230047ab">
   <br>
</p>

<p align="center"> 
<img src="https://raw.githubusercontent.com/Chainski/ProxyScraper/refs/heads/main/assets/ProxyScraper.png", width="500", height="500">
</p>

# 🕵️‍♂️ ProxyScraper

A high-performance multithreaded proxy scraper for Windows that fetches fresh HTTP, SOCKS4, and SOCKS5 proxies from a variety of public sources.

*NOTE: This library isn't designed for production use. It's advised to use your own proxies or purchase a service which provides an API. These are merely free ones that are retrieved from sites and should only be used for development or testing purposes.*

## 📌 Features

- [x] Scrapes HTTP/S, SOCKS4, and SOCKS5 proxies
- [x] Uses multithreading for fast downloads
- [x] Saves each proxy type to a separate `.txt` file
- [x] Displays colorful console banners and messages (ANSI)
- [x] Uses Windows WinHTTP API (native, no third-party deps)

## 🚀 Getting Started

### ⚙️ Requirements

- Windows OS
- C++ Compiler preferably [MinGW](https://github.com/brechtsanders/winlibs_mingw)

### 🛠️ Build Instructions

1. Clone the repository:

```bash
git clone https://github.com/Chainski/ProxyScraper
cd ProxyScraper
```

2. Compile using MinGW:

```bash
windres assets\ProxyScraper.rc -O coff -o assets\ProxyScraper.o
g++ ProxyScraper.cpp assets\ProxyScraper.o -w -O2 -static -lpsapi -lwinhttp -lws2_32 -fexceptions -Wl,--gc-sections -pipe -s -o ProxyScraper.exe
```

3. Run the executable:

```bash
ProxyScraper.exe
```
	
## 📁 Output

- `http.txt` — All scraped HTTP/HTTPS proxies
- `socks4.txt` — All scraped SOCKS4 proxies
- `socks5.txt` — All scraped SOCKS5 proxies
Files are appended each run with fresh results.

## 🔗 Proxy Sources
The scraper fetches proxies from over 30 reliable sources like:
- [ProxyScrape](https://proxyscrape.com)
- [JetKai](https://github.com/jetkai/proxy-list)
- [TheSpeedX](https://github.com/TheSpeedX/PROXY-List)
- and more...

## 👨‍💻 Author
**Chainski** [GitHub](https://github.com/Chainski)

## 📜 License
This project is licensed under the **GNU General Public License v3.0**.
You may copy, distribute, and modify the software as long as you track changes/dates in source files. All modifications must also be licensed under GPLv3. For full license text, see the [LICENSE](./LICENSE) file or visit: [https://www.gnu.org/licenses/gpl-3.0.html](https://www.gnu.org/licenses/gpl-3.0.html)
