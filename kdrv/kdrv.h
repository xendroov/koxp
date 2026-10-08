#pragma once
// Shared between kernel driver (kdrv.c) and injector (main.cpp).
// Include <ntddk.h> first in kernel code, or <Windows.h> in user code.

#define IOCTL_KDRV_INJECT  CTL_CODE(FILE_DEVICE_UNKNOWN, 0x900, METHOD_BUFFERED, FILE_ANY_ACCESS)

#pragma pack(push, 1)
typedef struct {
    unsigned long pid;           // target process PID
    unsigned long loadLibraryA;  // 32-bit address of LoadLibraryA in target process
    char          dllPath[260];  // full absolute path to DLL (ANSI)
} INJECT_REQUEST;
#pragma pack(pop)
