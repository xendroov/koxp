#pragma once
#include <Windows.h>
#include <cstdint>
#include <string>

namespace KO::Memory {

    inline uintptr_t Base() {
        static uintptr_t base = reinterpret_cast<uintptr_t>(GetModuleHandleA(nullptr));
        return base;
    }

    // RVA → absolute
    inline uintptr_t FromRVA(uintptr_t rva) {
        return Base() + rva;
    }

    template<typename T>
    inline T Read(uintptr_t addr) {
        return *reinterpret_cast<T*>(addr);
    }

    template<typename T>
    inline void Write(uintptr_t addr, T val) {
        *reinterpret_cast<T*>(addr) = val;
    }

    inline bool IsValidPtr(uintptr_t addr) {
        return addr > 0x10000 && addr < 0x7FFFFFFF;
    }

    inline std::string ReadString(uintptr_t addr, size_t maxLen = 64) {
        if (!IsValidPtr(addr)) return {};
        std::string s(maxLen, '\0');
        for (size_t i = 0; i < maxLen; ++i) {
            s[i] = Read<char>(addr + i);
            if (s[i] == '\0') { s.resize(i); return s; }
        }
        return s;
    }

    // Pointer chain: deref her adımda
    // Örnek: ResolveChain(staticAddr, 0x10, 0x4) = *(*(*(staticAddr) + 0x10) + 0x4)
    inline uintptr_t Deref(uintptr_t addr) {
        if (!IsValidPtr(addr)) return 0;
        return Read<uintptr_t>(addr);
    }

} // namespace KO::Memory
