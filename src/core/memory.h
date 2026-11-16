#pragma once
#include <Windows.h>
#include <cstdint>

// Güvenli pointer okuma — erişilemeyen adres → 0 döner, crash olmaz
namespace Mem {

template<typename T>
inline T Read(uintptr_t addr) {
    T val{};
    __try { val = *reinterpret_cast<T*>(addr); }
    __except (EXCEPTION_EXECUTE_HANDLER) {}
    return val;
}

inline uintptr_t Deref(uintptr_t addr) {
    return Read<uintptr_t>(addr);
}

// Çok seviyeli pointer zinciri: base → [off1] → [off2] → ...
template<typename... Offsets>
inline uintptr_t Chain(uintptr_t base, Offsets... offsets) {
    uintptr_t cur = Deref(base);
    for (uintptr_t off : {(uintptr_t)offsets...}) {
        if (!cur) return 0;
        cur = Deref(cur + off);
    }
    return cur;
}

} // namespace Mem
