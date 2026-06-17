#include "MainMenuState.h"
#include "PlanitiaGlobals.h"
#include "PlanitiaDisplay.h"
#include "PlanitiaEngineAdapter.h"
#include "PlanitiaResourceManager.h"
#include "PlanitiaInput.h"
#include "Terrain.h"
#include "Geist/StateMachine.h"
#include "Geist/Engine.h"
#include "Geist/Globals.h"

enum MainMenuStates
{
    MMS_CONTINUE = 2,
    MMS_NEWSTORY,
    MMS_SKIRMISHMODE,
    MMS_MULTIPLAYER,
    MMS_OPTIONS,
    MMS_QUIT,
    MMS_TUTORIAL = 14,
};

MainMenuState::~MainMenuState()
{
    Shutdown();
}

void MainMenuState::Init(const std::string&)
{
    //m_FrontEnd.Init("Data/GUIs/MainMenu.txt");
    m_Cursor = gp_ResourceManager->GetBitmap("Images/cursor.png");

    if (g_Terrain)
    {
        gp_Display->m_Camera.m_LookAtPoint = {
            static_cast<float>(g_Terrain->m_VertexWidth / 2), 0.0f,
            static_cast<float>(g_Terrain->m_VertexHeight / 2)};
    }

    m_FrontEnd.m_GuiX = (gp_Display->m_DesignHRes - 800) / 2;
    m_FrontEnd.m_GuiY = (gp_Display->m_DesignVRes - 660) / 2;
}

void MainMenuState::Shutdown() {}

void MainMenuState::Update()
{
    if (g_Terrain) g_Terrain->Update();
    m_FrontEnd.Update();
    DoInput();
}

void MainMenuState::DoInput()
{
    if (m_FrontEnd.m_Active == 0) return;

    switch (m_FrontEnd.m_Active)
    {
    case MMS_QUIT:
        g_Engine->m_Done = true;
        gp_Engine->m_Done = true;
        break;
    default:
        break;
    }
}

void MainMenuState::Draw()
{
    gp_Display->Begin3D();
    if (g_Terrain) g_Terrain->Draw();
    gp_Display->End3D();

    m_FrontEnd.Draw();
    if (m_Cursor)
    {
        gp_Display->BlitImage(m_Cursor,
            static_cast<int>(gp_Input->m_MouseX * gp_Display->UIScaleX()),
            static_cast<int>(gp_Input->m_MouseY * gp_Display->UIScaleY()));
    }
    gp_Display->FlushSprites();
}

void MainMenuState::OnEnter() {}
void MainMenuState::OnExit() {}