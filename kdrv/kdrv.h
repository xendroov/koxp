#pragma once
// Shared between kernel driver (kdrv.c) and injector (main.cpp).
// Include <ntddk.h> first in kernel code, or <Windows.h> in user code.

// CTL_CODE(FILE_DEVICE_UNKNOWN=0x22, 0x900, METHOD_BUFFERED=0, FILE_ANY_ACCESS=0)
#define IOCTL_KDRV_INJECT  0x00222400UL

#pragma pack(push, 1)
typedef struct {
    unsigned long pid;           // target process PID
    unsigned long loadLibraryA;  // 32-bit address of LoadLibraryA in target process
    char          dllPath[260];  // full absolute path to DLL (ANSI)
} INJECT_REQUEST;
#pragma pack(pop)
