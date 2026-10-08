#include "hooks.h"
// Packet hook'ları ileride buraya eklenecek (recv parse için)
// D3D9 hook -> ui/d3d9hook.cpp
// Diğer game function hook'ları buraya gelecek

namespace KO::Hooks {
bool Install() { return true; }
void Remove()  {}
} // namespace KO::Hooks
