@echo off
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars32.bat" >nul 2>&1

echo [*] Building libvorbisfile.dll (proxy)...
cl /LD /MD /O2 /std:c++17 /W0 ^
   /I src ^
   src\dllmain.cpp ^
   src\game\player.cpp ^
   /Fe:libvorbisfile.dll ^
   /link /MACHINE:X86 /DLL /DEF:src\libvorbisfile.def kernel32.lib user32.lib
if exist libvorbisfile.dll (echo [+] libvorbisfile.dll OK) else (echo [-] libvorbisfile.dll FAILED & goto end)

echo [*] Building injector.exe...
cl /MD /O2 /W0 ^
   injector\main.cpp ^
   injector\manual_map.cpp ^
   /Fe:injector.exe ^
   /link /MACHINE:X86 /SUBSYSTEM:CONSOLE kernel32.lib user32.lib advapi32.lib
if exist injector.exe (echo [+] injector.exe OK) else (echo [-] injector.exe FAILED)

:end
