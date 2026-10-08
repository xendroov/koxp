#pragma once
#include <Windows.h>
#include <cstdint>
#include <optional>
#include <string>

namespace KO::Memory {

    inline uintptr_t Base() {
        static uintptr_t base = reinterpret_cast<uintptr_t>(GetModuleHandleA(nullptr));
        return base;
    }

    template<typename T>
    inline T Read(uintptr_t addr) {
        return *reinterpret_cast<T*>(addr);
    }

    template<typename T>
    inline void Write(uintptr_t addr, T val) {
        *reinterpret_cast<T*>(addr) = val;
    }

    // Pointer chain: base + [off0] + [off1] + ...
    template<typename... Offsets>
    inline uintptr_t ResolveChain(uintptr_t base, Offsets... offsets) {
        uintptr_t addr = base;
        ([&](uintptr_t off) {
            if (!addr) return;
            addr = Read<uintptr_t>(addr) + off;
        }(offsets), ...);
        return addr;
    }

    inline std::string ReadString(uintptr_t addr, size_t maxLen = 64) {
        std::string s(maxLen, '\0');
        for (size_t i = 0; i < maxLen; ++i) {
            s[i] = Read<char>(addr + i);
            if (s[i] == '\0') { s.resize(i); return s; }
        }
        return s;
    }

    inline bool IsValidPtr(uintptr_t addr) {
        return addr != 0 && addr > 0x10000;
    }

    // MODULE BASE + static offset
    inline uintptr_t Resolve(uintptr_t staticOffset) {
        return Base() + staticOffset;
    }

} // namespace KO::Memory
