@echo off
echo [*] Building koxp.dll (x86)...

cl /LD /MD /O2 /std:c++17 /W0 ^
   /I src ^
   src\dllmain.cpp ^
   src\game\player.cpp ^
   /Fe:koxp.dll ^
   /link /MACHINE:X86 /DLL kernel32.lib user32.lib

if exist koxp.dll (
    echo [+] OK: koxp.dll hazir
) else (
    echo [-] HATA: derleme basarisiz
)
