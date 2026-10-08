#pragma once
#include <cstdint>
#include <vector>

namespace KO {

class Packet {
public:
    explicit Packet(uint16_t opcode) { Write<uint16_t>(opcode); }

    template<typename T>
    Packet& Write(T val) {
        const uint8_t* p = reinterpret_cast<const uint8_t*>(&val);
        buf_.insert(buf_.end(), p, p + sizeof(T));
        return *this;
    }

    Packet& WriteBytes(const uint8_t* data, size_t len) {
        buf_.insert(buf_.end(), data, data + len);
        return *this;
    }

    Packet& WriteString(const char* str, size_t fixedLen) {
        for (size_t i = 0; i < fixedLen; ++i)
            buf_.push_back(str[i] ? static_cast<uint8_t>(str[i]) : 0);
        return *this;
    }

    const uint8_t* Data()  const { return buf_.data(); }
    size_t         Size()  const { return buf_.size(); }

private:
    std::vector<uint8_t> buf_;
};

// KO_SND_FNC = 0x007032E0 — __thiscall(this=socket, data, size)
using SendFn_t = void(__thiscall*)(void* socket, const uint8_t* data, uint16_t size);

void SendToServer(const Packet& pkt);

// -----------------------------------------------------------------------
// Paket Opcode'ları — KO 2626 için bul/güncelle
// -----------------------------------------------------------------------
namespace Opcode {
    constexpr uint16_t ATTACK           = 0x0000; // TODO
    constexpr uint16_t USE_SKILL        = 0x0000; // TODO
    constexpr uint16_t USE_ITEM         = 0x0000; // TODO
    constexpr uint16_t SELECT_TARGET    = 0x0000; // TODO
    constexpr uint16_t MOVE             = 0x0000; // TODO
    constexpr uint16_t PICK_UP          = 0x0000; // TODO
    constexpr uint16_t USE_AREA_SKILL   = 0x0000; // TODO
}

} // namespace KO
