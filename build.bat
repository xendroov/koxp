@echo off
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars32.bat" >nul 2>&1

echo [*] Building koxp.dll / winmm.dll (proxy)...
cl /LD /MD /O2 /std:c++17 /W0 ^
   /I src ^
   src\dllmain.cpp ^
   src\game\player.cpp ^
   src\winmm_proxy.cpp ^
   /Fe:koxp.dll ^
   /link /MACHINE:X86 /DLL kernel32.lib user32.lib
if exist koxp.dll (
    copy /Y koxp.dll winmm.dll >nul
    echo [+] koxp.dll + winmm.dll OK
) else (echo [-] koxp.dll FAILED & goto end)

echo [*] Building injector.exe...
cl /MD /O2 /W0 ^
   injector\main.cpp ^
   injector\manual_map.cpp ^
   /Fe:injector.exe ^
   /link /MACHINE:X86 /SUBSYSTEM:CONSOLE kernel32.lib user32.lib advapi32.lib
if exist injector.exe (echo [+] injector.exe OK) else (echo [-] injector.exe FAILED)

:end
