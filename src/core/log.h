#pragma once
#include <Windows.h>
#include <cstdio>
#include <cstdarg>
#include <ctime>

// C:\koxp_log.txt dosyasına yazar — inject sonrası kontrol et
inline void KLog(const char* fmt, ...) {
    char msg[512];
    va_list args;
    va_start(args, fmt);
    vsnprintf(msg, sizeof(msg), fmt, args);
    va_end(args);

    FILE* f = nullptr;
    fopen_s(&f, "C:\\koxp_log.txt", "a");
    if (f) {
        SYSTEMTIME st{};
        GetLocalTime(&st);
        fprintf(f, "[%02d:%02d:%02d] %s\n", st.wHour, st.wMinute, st.wSecond, msg);
        fclose(f);
    }
}
