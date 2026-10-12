// Shared types for the MEd core library.
// Recovered from MEd.exe v0.32: the game version is a byte at CIPLFile+8
// (1 = GTA III, 2 = Vice City, 3 = San Andreas).
#pragma once
#include <string>
#include <vector>

namespace med {

enum class GameVersion : unsigned char { GTA3 = 1, ViceCity = 2, SanAndreas = 3 };

// "LOD index not set". MEd initialises every instance to 0xFFFF.
constexpr short kNoLod = static_cast<short>(0xFFFF);

}  // namespace med
