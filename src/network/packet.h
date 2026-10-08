#pragma once
#include <Windows.h>
#include <cstdint>
#include <vector>
#include "../core/offsets.h"

namespace KO {

// Basit packet builder
class Packet {
public:
    explicit Packet(uint16_t opcode) {
        Write<uint16_t>(opcode);
    }

    template<typename T>
    Packet& Write(T val) {
        uint8_t* p = reinterpret_cast<uint8_t*>(&val);
        buf_.insert(buf_.end(), p, p + sizeof(T));
        return *this;
    }

    Packet& WriteString(const char* str, size_t len) {
        for (size_t i = 0; i < len; ++i) buf_.push_back(static_cast<uint8_t>(str[i]));
        return *this;
    }

    const std::vector<uint8_t>& Data() const { return buf_; }
    size_t Size() const { return buf_.size(); }

private:
    std::vector<uint8_t> buf_;
};

// fn_SendPacket imzasına göre ayarla
using SendPacket_t = void(__thiscall*)(void* socket, const uint8_t* data, size_t size);

// Paketi oyuna gönder
void SendToServer(const Packet& pkt);

// Paket opcodeları (KO 2626 için bul/güncelle)
namespace Opcode {
    constexpr uint16_t ATTACK          = 0x0000;
    constexpr uint16_t USE_SKILL       = 0x0000;
    constexpr uint16_t USE_ITEM        = 0x0000;
    constexpr uint16_t SELECT_TARGET   = 0x0000;
    constexpr uint16_t MOVE            = 0x0000;
    constexpr uint16_t PICK_UP         = 0x0000;
}

} // namespace KO
