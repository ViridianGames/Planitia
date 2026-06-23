#include "LoadingState.h"
#include "PlanitiaGlobals.h"
#include "PlanitiaScene.h"
#include "PlanitiaFonts.h"
#include "../Geist/Source/StateMachine.h"
#include "../Geist/Source/Globals.h"

LoadingState::~LoadingState()
{
    Shutdown();
}

void LoadingState::Init(const std::string&)
{
    m_Loaded = 2;
}

void LoadingState::Shutdown() {}

void LoadingState::Update()
{
    if (m_Loaded == 0)
        g_StateMachine->MakeStateTransition(STATE_MAINMENUSTATE);
    --m_Loaded;
}

void LoadingState::Draw()
{
    const std::string message = "Planitia is loading.  One moment, please...";
    PlanitiaDrawTextCentered(g_PlanitiaFont, PLANITIA_FONT_SIZE, message,
        gp_Scene->m_DesignHRes * 0.5f, 288.0f, WHITE);
    gp_Scene->FlushSprites();
}