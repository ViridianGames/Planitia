#ifndef PLANITIA_GLOBALS_H
#define PLANITIA_GLOBALS_H

#include "PlanitiaTypes.h"
#include "Geist/RNG.h"
#include <cmath>
#include <string>
#include <vector>

class PlanitiaScene;
class PlanitiaInput;
class PlanitiaEngineAdapter;
class Terrain;
class Background;
class Unit;

enum PlanitiaStateId
{
    STATE_MAINSTATE = 1,
    STATE_PAUSESTATE,
    STATE_MAINMENUSTATE,
    STATE_SETUPMULTIPLAYERSTATE,
    STATE_SETUPSTORYSTATE,
    STATE_SETUPSKIRMISHSTATE,
    STATE_OPTIONSSTATE,
    STATE_LOADINGSTATE,
    STATE_SETUPTUTORIALSTATE,
};

struct PlanitiaGeneral
{
    D3DXVECTOR3 m_Pos{};
    bool m_Selected = false;
};

struct PlanitiaPlayer
{
    PlanitiaGeneral* m_General = nullptr;
};

extern PlanitiaEngineAdapter* gp_Engine;
extern PlanitiaScene* gp_Scene;
extern PlanitiaInput* gp_Input;

extern Terrain* g_Terrain;
extern Background* g_Background;

extern RNG g_VitalRNG;
extern PlanitiaPlayer* g_Players[4];
extern int g_NumberOfPlayers;

bool GlobalIsDistanceLessThan(float startX, float startZ, float endX, float endZ, float range);

void AddStringToConsole(const std::string& text, DWORD timeIndex = 0,
    int r = 255, int g = 255, int b = 255, int r2 = 0, int g2 = 0, int b2 = 0);

#endif