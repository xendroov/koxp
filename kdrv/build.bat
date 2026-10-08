@echo off
setlocal enabledelayedexpansion

rem ── WDK: ntddk.h olan en yeni surumu bul ──────────────────────────────────
set "WDK_BASE=C:\Program Files (x86)\Windows Kits\10"
set "WDK_INC="
set "WDK_LIB="

for /f "tokens=*" %%v in ('dir /b /od "%WDK_BASE%\Include" 2^>nul') do (
    if exist "%WDK_BASE%\Include\%%v\km\ntddk.h" (
        set "WDK_INC=%WDK_BASE%\Include\%%v"
        set "WDK_LIB=%WDK_BASE%\Lib\%%v\km\x64"
    )
)
if "%WDK_INC%"=="" (
    echo [-] ntddk.h bulunamadi.  WDK kurulu mu?
    echo     Beklenen: %WDK_BASE%\Include\^<versiyon^>\km\ntddk.h
    pause & exit /b 1
)
echo [+] WDK   : %WDK_INC%
echo [+] WDKLib: %WDK_LIB%

rem ── Visual Studio: cl.exe'yi vswhere ile bul ──────────────────────────────
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" (
    echo [-] vswhere.exe bulunamadi. Visual Studio kurulu mu?
    pause & exit /b 1
)
for /f "usebackq tokens=*" %%i in (`"%VSWHERE%" -latest -property installationPath`) do set "VS=%%i"

set "MSVC_TOOLS=%VS%\VC\Tools\MSVC"
for /f "tokens=*" %%v in ('dir /b /od "%MSVC_TOOLS%" 2^>nul') do set "MSVC_VER=%%v"

set "CL=%MSVC_TOOLS%\%MSVC_VER%\bin\HostX64\x64\cl.exe"
set "LINK=%MSVC_TOOLS%\%MSVC_VER%\bin\HostX64\x64\link.exe"

if not exist "%CL%" (
    echo [-] cl.exe bulunamadi: %CL%
    pause & exit /b 1
)
echo [+] CL    : %CL%

rem ── Build ─────────────────────────────────────────────────────────────────
cd /d "%~dp0"
if exist kdrv.obj del kdrv.obj
if exist kdrv.sys del kdrv.sys

echo.
echo [*] Derleniyor: kdrv.c ...
"%CL%" /nologo /GS- /EHa- /W3 /Od /c ^
  /D_AMD64_ /DAMD64 /D_WIN64 /DWINNT ^
  /DNTDDI_VERSION=0x0A000000 /D_WIN32_WINNT=0x0A00 ^
  /I "%WDK_INC%\km" /I "%WDK_INC%\shared" ^
  /TC kdrv.c /Fo kdrv.obj
if errorlevel 1 (
    echo [-] Derleme BASARISIZ!
    pause & exit /b 1
)

echo.
echo [*] Linkleniyor: kdrv.sys ...
"%LINK%" /nologo /DRIVER /SUBSYSTEM:NATIVE /ENTRY:DriverEntry /NODEFAULTLIB ^
  /LIBPATH:"%WDK_LIB%" /OUT:kdrv.sys /MERGE:.rdata=.text ^
  /IGNORE:4210 /IGNORE:4078 ^
  kdrv.obj ntoskrnl.lib hal.lib
if errorlevel 1 (
    echo [-] Link BASARISIZ!
    pause & exit /b 1
)

echo.
echo [OK] kdrv.sys basariyla olusturuldu!
dir /b kdrv.sys
pause
