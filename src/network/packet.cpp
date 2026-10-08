#include "packet.h"
#include "../core/memory.h"

namespace KO {

// Socket instance pointer — offset'i bul
static constexpr uintptr_t g_socketPtr = 0x00000000;

void SendToServer(const Packet& pkt) {
    uintptr_t fnAddr = Memory::Resolve(Offsets::fn_SendPacket);
    if (!fnAddr) return;

    void* socket = reinterpret_cast<void*>(
        Memory::Read<uintptr_t>(Memory::Resolve(g_socketPtr))
    );
    if (!socket) return;

    auto fn = reinterpret_cast<SendPacket_t>(fnAddr);
    fn(socket, pkt.Data().data(), pkt.Size());
}

} // namespace KO
