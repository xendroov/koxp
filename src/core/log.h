#pragma once
#include <cstdio>
#include <Windows.h>

// C:\koxp.log dosyasına yazar — thread-safe değil, sadece main thread kullanır
inline void KLog(const char* fmt, ...) {
    static FILE* f = nullptr;
    if (!f) f = fopen("C:\\koxp.log", "a");
    if (!f) return;

    char buf[512];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);

    fprintf(f, "%s\n", buf);
    fflush(f);
}
