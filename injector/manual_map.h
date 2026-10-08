#pragma once
#include <Windows.h>
#include <string>

// Section-object based manual DLL mapper.
// Avoids NtAllocateVirtualMemory + NtWriteVirtualMemory (blocked by Xigncode3 kernel driver).
// Uses NtCreateSection + NtMapViewOfSection (different syscall path, typically not monitored).
bool ManualMap(DWORD pid, const std::string& dllPath);
