#include "MainMenuState.h"
#include "PlanitiaGlobals.h"
#include "PlanitiaScene.h"
#include "Geist/Config.h"
#include "Geist/ResourceManager.h"
#include "PlanitiaTypes.h"
#include "PlanitiaInput.h"
#include "PlanitiaEngineAdapter.h"
#include "PlanitiaFonts.h"
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

namespace {

void ScaleGuiForDisplay(Gui& gui, float scaleX, float scaleY)
{
    gui.m_Pos.x *= scaleX;
    gui.m_Pos.y *= scaleY;
    gui.m_Width *= scaleX;
    gui.m_Height *= scaleY;
    gui.m_InputScale = scaleX;

    for (auto& entry : gui.m_GuiElementList)
    {
        auto& element = entry.second;
        element->m_Pos.x *= scaleX;
        element->m_Pos.y *= scaleY;
        element->m_Width *= scaleX;
        element->m_Height *= scaleY;
    }
}

} // namespace

MainMenuState::~MainMenuState()
{
    Shutdown();
}

void MainMenuState::Init(const std::string&)
{
    m_FrontEnd.m_Font = std::make_shared<Font>(g_PlanitiaFont);
    m_FrontEnd.LoadLegacyFile("Data/GUIs/MainMenu.txt");

    const int guiWidth = 800;
    const int guiHeight = 660;
    m_FrontEnd.SetLayout(
        (gp_Scene->m_DesignHRes - guiWidth) / 2,
        (gp_Scene->m_DesignVRes - guiHeight) / 2,
        guiWidth, guiHeight, 1.0f, Gui::GUIP_USE_XY);

    ScaleGuiForDisplay(m_FrontEnd, gp_Scene->UIScaleX(), gp_Scene->UIScaleY());

    m_Cursor = g_ResourceManager->GetTexture(NormalizePath("Images/cursor.png"));

    if (g_Terrain)
    {
        const float centerX = static_cast<float>(g_Terrain->m_VertexWidth / 2);
        const float centerZ = static_cast<float>(g_Terrain->m_VertexHeight / 2);
        gp_Scene->m_Camera.m_LookAtPoint = {
            centerX,
            g_Terrain->GetHeight(centerX, centerZ),
            centerZ};
    }
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
    const int active = m_FrontEnd.GetActiveElementID();
    if (active == -1) return;

    switch (active)
    {
    case MMS_QUIT:
        g_Engine->m_Done = true;
        if (gp_Engine) gp_Engine->m_Done = true;
        break;
    default:
        break;
    }
}

void MainMenuState::Draw()
{
    if (gp_Scene)
    {
        gp_Scene->Begin3D();
        if (g_Background) g_Background->Draw();
        if (g_Terrain) g_Terrain->Draw();
        gp_Scene->End3D();
    }

    // m_FrontEnd.Draw();
    // if (m_Cursor)
    // {
    //     gp_Scene->BlitImage(m_Cursor,
    //         static_cast<int>(gp_Input->m_MouseX * gp_Scene->UIScaleX()),
    //         static_cast<int>(gp_Input->m_MouseY * gp_Scene->UIScaleY()));
    // }
    // gp_Scene->FlushSprites();

    DrawTextEx(g_PlanitiaFont, "Hello!", { 4, 4}, g_PlanitiaFont.baseSize, 1, WHITE);
}

void MainMenuState::OnEnter() {}
void MainMenuState::OnExit() {}