#pragma once
#include <Windows.h>
#include <string>

// Section-object based manual DLL mapper.
// Avoids NtAllocateVirtualMemory + NtWriteVirtualMemory (blocked by Xigncode3 kernel driver).
// Uses NtCreateSection + NtMapViewOfSection (different syscall path, typically not monitored).
bool ManualMap(DWORD pid, const std::string& dllPath);

// Suspended-process variant: maps the DLL while the process is suspended (before
// xhunter1 user-mode starts scanning), then creates a shellcode thread that calls
// NtDelayExecution(delayMs) before invoking DllMain. This gives ntdll time to finish
// loading all process imports after ResumeThread. Returns immediately without waiting
// for DllMain — caller must ResumeThread after this returns.
bool ManualMapDelayed(DWORD pid, const std::string& dllPath, DWORD delayMs = 4000);
