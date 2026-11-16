# Developer Command Prompt gerektirmez - bu script VS'yi otomatik bulur
# Calistirmak icin: powershell -ExecutionPolicy Bypass -File build.ps1

$vsWhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
if (-not (Test-Path $vsWhere)) {
    $vsWhere = "${env:ProgramFiles}\Microsoft Visual Studio\Installer\vswhere.exe"
}

$vsPath = & $vsWhere -latest -property installationPath 2>$null
if (-not $vsPath) { Write-Host "[-] Visual Studio bulunamadi"; exit 1 }

$vcvars = "$vsPath\VC\Auxiliary\Build\vcvars32.bat"

$cmd = "call `"$vcvars`" >nul 2>&1 && " +
       "cl /LD /MD /O2 /W0 /I src " +
       "src\dllmain.cpp src\game\player.cpp " +
       "/Fe:libvorbisfile.dll " +
       "/link /MACHINE:X86 /DLL /DEF:src\libvorbisfile.def kernel32.lib user32.lib"

Write-Host "[*] Building libvorbisfile.dll..."
$result = cmd /c $cmd 2>&1
Write-Host $result

if (Test-Path "libvorbisfile.dll") {
    Write-Host "[+] libvorbisfile.dll OK - $('{0:N0}' -f (Get-Item libvorbisfile.dll).Length) bytes"
} else {
    Write-Host "[-] libvorbisfile.dll FAILED"
    exit 1
}
