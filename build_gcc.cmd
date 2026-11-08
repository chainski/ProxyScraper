@echo off
cd /d "%~dp0"
windres assets\ProxyScraper.rc -O coff -o assets\ProxyScraper.o
g++ -std=c++17 ProxyScraper.cpp assets\ProxyScraper.o -w -O2 -static -lwinhttp -fexceptions -Wl,--gc-sections -pipe -s -o proxyscraper-v2.0-x86_64-gcc.exe && del assets\ProxyScraper.o
