#include "packet.h"
#include "../core/offsets.h"
#include "../core/memory.h"

namespace KO {

void SendToServer(const Packet& pkt) {
    // KO_PTR_PKT → *KO_PTR_PKT = CGameSocket instance
    uintptr_t socketObj = Memory::Deref(Offsets::Static::PTR_PKT);
    if (!Memory::IsValidPtr(socketObj)) return;

    uintptr_t fnAddr = Offsets::Fn::SendPacket;
    if (!fnAddr) return;

    auto fn = reinterpret_cast<SendFn_t>(fnAddr);
    fn(reinterpret_cast<void*>(socketObj),
       pkt.Data(),
       static_cast<uint16_t>(pkt.Size()));
}

} // namespace KO
