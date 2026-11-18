#ifndef _PLANITIA_MATCHCHECKSUM_H_
#define _PLANITIA_MATCHCHECKSUM_H_

#include <cstdint>

class GameSim;

// Lockstep desync fingerprint. One copy, shared with the harness replay check.
uint32_t PlanitiaMatchChecksum(const GameSim& sim);

#endif
