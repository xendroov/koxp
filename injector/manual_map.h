#pragma once
#include <Windows.h>

// DLL'i hedef process'e manuel olarak map eder.
// - PEB/LDR'a kayıt yapmaz (LoadLibrary izleri yok)
// - PsSetLoadImageNotifyRoutine tetiklenmez
// Döner: inject edilen remote base adresi, 0 ise hata.
uintptr_t ManualMap(HANDLE hProc, const char* dllPath);
