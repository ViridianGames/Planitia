#include "MainState.h"
#include "MultiplayerMenuState.h"
#include "OptionsState.h"
#include "TitleState.h"
#include "Terrain.h"
#include "Lockstep.h"
#include "NetSession.h"
#include "raylib.h"
#include "Geist/RNG.h"

#include <memory>

enum GameStates
{
    STATE_TITLESTATE = 0,
    STATE_MAINSTATE,
    STATE_OPTIONSSTATE,
    STATE_COMBATSTATE,
    STATE_CASTLEDESIGNSTATE,
    STATE_MULTIPLAYERMENUSTATE, // lobby: host/join/start -> MainState
    STATE_LASTSTATE
};

struct ConsoleString
{
    std::string m_String;
    Color m_Color;
    unsigned int m_StartTime;
};

inline float g_drawScale = 1.0f;

inline std::unique_ptr<RNG> g_vitalRNG;
inline std::unique_ptr<RNG> g_nonVitalRNG;

inline std::shared_ptr<Font> g_font;
inline std::shared_ptr<Font> g_smallFont;

inline std::unique_ptr<Terrain> g_Terrain;

inline NetSession g_Net;
inline LockstepController g_Lockstep;

void DrawOutlinedText(std::shared_ptr<Font> font, const std::string& text, Vector2 position, float fontSize, int spacing, Color color);

void DrawParagraph(std::shared_ptr<Font> font, const std::string& text, Vector2 position, float maxwidth, float fontSize, int spacing, Color color, bool outlined = false);

void DebugPrint(std::string text);
void AddConsoleString(const std::string& text, Color color);
void DrawConsole();

// FPS + stacked Update/Draw time graph (same idea as U7Revisited sandbox profiler).
// loc: 0=bottom-left, 1=top-left, 2=bottom-right, 3=top-right
void DrawPerfCounter(Font* font, int loc = 0);

inline bool g_showPerfCounter = true;

// Cutout shader for walker billboards (and later other alpha sprites).
// Discards low-alpha texels so overlapping sprites don't blend through each other.
inline Shader g_alphaDiscard{};
inline int g_alphaDiscardCutoffLoc = -1;

// Map window mouse pixels -> virtual render pixels (same stretch as DrawTexturePro).
Vector2 GetScaledMousePosition();

// Unproject using render WxH so the ray matches BeginMode3D into the virtual RT.
// Planitia's bundled raylib lacks GetScreenToWorldRayEx; this is the local equivalent.
Ray GetTerrainMouseRay(const Camera3D& camera);
