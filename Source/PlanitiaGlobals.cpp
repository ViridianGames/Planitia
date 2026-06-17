#include "PlanitiaGlobals.h"

PlanitiaEngineAdapter* gp_Engine = nullptr;
PlanitiaScene* gp_Scene = nullptr;
PlanitiaInput* gp_Input = nullptr;

Terrain* g_Terrain = nullptr;
Background* g_Background = nullptr;

RNG g_VitalRNG;
PlanitiaPlayer* g_Players[4] = {nullptr, nullptr, nullptr, nullptr};
int g_NumberOfPlayers = 0;

bool GlobalIsDistanceLessThan(float startX, float startZ, float endX, float endZ, float range)
{
    return (std::pow(std::abs(startX - endX), 2) + std::pow(std::abs(startZ - endZ), 2) <= (range * range));
}

void AddStringToConsole(const std::string&, DWORD, int, int, int, int, int, int) {}