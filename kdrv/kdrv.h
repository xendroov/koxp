#pragma once

// IOCTL: CTL_CODE(FILE_DEVICE_UNKNOWN=0x22, 0x900, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_INJECT  0x00222400UL

#define KDRV_DEVICE_NAME  "\\\\.\\kdrv"

#pragma pack(push,1)
typedef struct {
    unsigned long pid;
    unsigned long loadLibraryA;  // GetProcAddress(kernel32, "LoadLibraryA") — injector doldurur
    char          dllPath[260];
} INJECT_REQ;
#pragma pack(pop)
