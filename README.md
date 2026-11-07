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

- [x] Scrapes `HTTP/S`, `SOCKS4` and `SOCKS5` proxies
- [x] 60+ actively maintained sources (every source verified `200 OK` and updated within the last 24 hours)
- [x] Worker-pool multithreading (32 threads) — typically finishes in ~2-5 seconds
- [x] Real browser fingerprint: top Windows User-Agent from [microlinkhq/top-user-agents](https://github.com/microlinkhq/top-user-agents) + browser request headers
- [x] Per-source live progress with proxy counts and fetch latency
- [x] Plain-text **and** JSON (`{"ip":..,"port":..}`) proxy extraction
- [x] Strict validation: IPv4 octets `0-255`, ports `1-65535`, loopback/reserved ranges dropped
- [x] Full deduplication — output files contain unique `IP:PORT` entries only, sorted alphabetically
- [x] Output files are rewritten fresh on every run (no accumulation)
- [x] Colorful console banners and messages (ANSI) with end-of-run statistics
- [x] Uses Windows WinHTTP API (native, no third-party deps)

## 🚀 Getting Started

### ⚙️ Requirements

- Windows OS
- C++ Compiler — either of:
  - [MinGW](https://github.com/brechtsanders/winlibs_mingw/releases) (GCC build via `build.cmd`)
  - Portable MSVC [PortableBuildTools](https://github.com/Data-Oriented-House/PortableBuildTools) (MSVC build via `build-msvc.cmd`), with `msvc.bat` available on `PATH`

### 🛠️ Build Instructions

1. Clone the repository:

```bash
git clone https://github.com/Chainski/ProxyScraper
cd ProxyScraper
```

2. Compile using MinGW (or just run `build.cmd`):

```bash
windres assets\ProxyScraper.rc -O coff -o assets\ProxyScraper.o
g++ -std=c++17 ProxyScraper.cpp assets\ProxyScraper.o -w -O2 -static -lwinhttp -fexceptions -Wl,--gc-sections -pipe -s -o ProxyScraper.exe
```

3. Run the executable:

```bash
ProxyScraper.exe
```

Alternatively, `build-msvc.cmd` builds with the portable MSVC toolchain from [PortableBuildTools](https://github.com/Data-Oriented-House/PortableBuildTools). It expects the PortableBuildTools environment script (`msvc.bat`) to be on `PATH`.

## 📁 Output

- `http.txt` — All scraped HTTP/HTTPS proxies
- `socks4.txt` — All scraped SOCKS4 proxies
- `socks5.txt` — All scraped SOCKS5 proxies

Files are rewritten on each run with fresh, validated and deduplicated results.

## 🔗 Proxy Sources
The scraper fetches proxies from over 60 actively maintained sources like:
- [ProxyScrape (v2 + v4 API)](https://proxyscrape.com)
- [OpenProxyList](https://openproxylist.xyz)
- [GeoNode API](https://https://geonode.com/free-proxy-list)
- [ProxySpace](https://proxyspace.pro)
- [TheSpeedX](https://github.com/TheSpeedX/PROXY-List)
- [mmpx12](https://github.com/mmpx12/proxy-list)
- [monosans](https://github.com/monosans/proxy-list)
- [roosterkid](https://github.com/roosterkid/openproxylist)
- [iplocate](https://github.com/iplocate/free-proxy-list)
- [Tianndev](https://github.com/Tianndev/free-proxy)
- [dpangestuw](https://github.com/dpangestuw/Free-Proxy)
- [zloi-user/hideip.me](https://github.com/zloi-user/hideip.me)
- [VMHeaven](https://github.com/vmheaven/VMHeaven.io-Free-Proxy-List)
- and many more...

## 👨‍💻 Author
**Chainski** [GitHub](https://github.com/Chainski)

## 📜 License
This project is licensed under the **GNU General Public License v3.0**.
You may copy, distribute, and modify the software as long as you track changes/dates in source files. All modifications must also be licensed under GPLv3. For full license text, see the [LICENSE](./LICENSE) file or visit: [https://www.gnu.org/licenses/gpl-3.0.html](https://www.gnu.org/licenses/gpl-3.0.html)
