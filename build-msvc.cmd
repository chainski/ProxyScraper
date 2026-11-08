@echo off
cd /d "%~dp0"
rc.exe /nologo /fo assets\ProxyScraper.res assets\ProxyScraper.rc
cl.exe /nologo /std:c++17 /EHsc /MT /O2 ProxyScraper.cpp assets\ProxyScraper.res /link winhttp.lib /RELEASE /OPT:REF /OPT:ICF /INCREMENTAL:NO /NOCOFFGRPINFO /EMITTOOLVERSIONINFO:NO /OUT:proxyscraper-v2.0-x86_64-msvc.exe && del *.obj assets\ProxyScraper.res
