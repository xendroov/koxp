@echo off
:: WDK + VS2022 ile kdrv.sys derleme
:: Gereksinim: WDK 10 kurulu olmali (Windows Driver Kit)
:: https://learn.microsoft.com/en-us/windows-hardware/drivers/download-the-wdk

where msbuild >nul 2>&1
if errorlevel 1 (
    call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars32.bat" >nul 2>&1
)

echo [*] kdrv.sys build ediliyor...
msbuild kdrv.vcxproj /p:Configuration=Release /p:Platform=x64 /nologo /v:m

if exist x64\Release\kdrv.sys (
    copy x64\Release\kdrv.sys ..\kdrv.sys >nul
    echo [+] kdrv.sys hazir
) else (
    echo [-] FAILED — WDK kurulu mu?
)
